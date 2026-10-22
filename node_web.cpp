/*
 * node_web.cpp — Wi-Fi (Station / Soft AP / off), captive portal and the
 * configuration web server of the LoRaWAN node.
 *
 * Pages (all with HTTP Basic login, except favicon and /saiu):
 *   GET  /              configuration + status (page in flash, filled by JS)
 *   GET  /config.json   saved configuration (passwords never leave the node)
 *   GET  /status        live status of the node / LoRaWAN session
 *   POST /save          save the form (urlencoded)
 *   POST /action        a=send | restart | fcnt (v=<n>)    -> JSON
 *   POST /reboot        reboot page
 *   POST /factory       factory defaults + reboot
 *   GET  /logs, /logs_data, /logs_download   live log
 *   GET  /logout, /saiu  logout (same trick as the gateway)
 *
 * SPDX-License-Identifier: MIT
 */
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <esp_mac.h>
#include "node_web.h"
#include "node_config.h"
#include "node_state.h"
#include "node_log.h"
#include "node_page.h"

#define WEB_LOGOUT_USER     "_logout_"
#define WEB_LOGOUT_PASS     "_logout_"
#define AP_IDLE_OFF_MS      60000UL     /* fallback AP off 60 s after STA is up */
#define STA_RETRY_MS        60000UL     /* STA retry while in fallback AP       */

/* Soft AP addressing — defaults if node_build.h (ABP/OTAA) does not set them */
#ifndef DEF_AP_IP
#define DEF_AP_IP           192, 168, 4, 1
#endif
#ifndef DEF_AP_GATEWAY
#define DEF_AP_GATEWAY      DEF_AP_IP
#endif
#ifndef DEF_AP_MASK
#define DEF_AP_MASK         255, 255, 255, 0
#endif

static WebServer   server(80);
static DNSServer   dns;
static NodeConfig  wcfg;                /* web task copy of the config     */
static NodeConfig  bootCfg;             /* as booted: reboot-only fields   */
static bool        forcedAp;
static bool        apOn, dnsOn, mdnsOn;
static char        apName[33], host[33];
static uint32_t    staStartMs, apIdleSinceMs, staRetryMs;
static uint32_t    cfgVerSeen;
static volatile bool forceApReq;
static bool        taskRunning;

/* ===================================================================== */
/*                                 helpers                               */
/* ===================================================================== */

/* Small JSON writer on a fixed buffer. */
typedef struct { char *b; size_t size, n; } JBuf;

static void jraw(JBuf *j, const char *s)
{
    size_t l = strlen(s);
    if (j->n + l >= j->size) l = j->size - j->n - 1;
    memcpy(j->b + j->n, s, l);
    j->n += l;
    j->b[j->n] = '\0';
}

static void jfmt(JBuf *j, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int l = vsnprintf(j->b + j->n, j->size - j->n, fmt, ap);
    va_end(ap);
    if (l > 0) j->n += ((size_t)l < j->size - j->n) ? (size_t)l : j->size - j->n - 1;
}

static void jstrn(JBuf *j, const char *s, size_t len)
{
    char e[8];
    jraw(j, "\"");
    for (size_t i = 0; i < len; i++) {
        unsigned char c = s[i];
        if (c == '"' || c == '\\') { e[0] = '\\'; e[1] = c; e[2] = 0; jraw(j, e); }
        else if (c < 0x20)         { snprintf(e, sizeof(e), "\\u%04x", c); jraw(j, e); }
        else                       { e[0] = c; e[1] = 0; jraw(j, e); }
    }
    jraw(j, "\"");
}

static void jkey(JBuf *j, const char *k)
{
    if (j->n > 1 && j->b[j->n - 1] != '{' && j->b[j->n - 1] != '[') jraw(j, ",");
    jraw(j, "\""); jraw(j, k); jraw(j, "\":");
}

static void jstr(JBuf *j, const char *k, const char *v) { jkey(j, k); jstrn(j, v, strlen(v)); }
static void jint(JBuf *j, const char *k, long v)        { jkey(j, k); jfmt(j, "%ld", v); }
static void jbool(JBuf *j, const char *k, bool v)       { jkey(j, k); jraw(j, v ? "true" : "false"); }
static void jonoff(JBuf *j, const char *k, bool v)      { jstr(j, k, v ? "on" : "off"); }
static void jhex(JBuf *j, const char *k, const uint8_t *b, size_t n, char sep = 0)
{
    char h[3 * RST_CMD_MAX + 4];
    bytesToHex(b, n, h, sep);
    jstr(j, k, h);
}

static bool auth(void)
{
    if (server.authenticate(wcfg.webUser, wcfg.webPass))
        return true;
    server.requestAuthentication(BASIC_AUTH, "", "Login necessario");
    return false;
}

static void noStore(void) { server.sendHeader("Cache-Control", "no-store"); }

static String htmlEsc(const char *s)
{
    String o;
    for (; *s; s++) {
        switch (*s) {
        case '&': o += "&amp;"; break;
        case '<': o += "&lt;";  break;
        case '>': o += "&gt;";  break;
        case '"': o += "&quot;"; break;
        case '\'': o += "&#39;"; break;
        default:  o += *s;
        }
    }
    return o;
}

/* Small page after "Salvar", "Reiniciar"... (same look as the main page). */
static void sendSimple(const char *headExtra, const String &body)
{
    String p;
    p.reserve(1800 + body.length());
    p += FPSTR(SIMPLE_HEAD);
    p += "<title>"; p += htmlEsc(wcfg.name); p += "</title>";
    if (headExtra) p += headExtra;
    p += "</head><body><div>";
    p += body;
    p += FPSTR(SIMPLE_END);
    noStore();
    server.send(200, "text/html; charset=utf-8", p);
}

static bool isOtaa(void) { return NODE_ACTIVATION_OTAA; }

/* ===================================================================== */
/*                                 Wi-Fi                                 */
/* ===================================================================== */
static void startAp(bool withSta)
{
    WiFi.mode(withSta ? WIFI_AP_STA : WIFI_AP);
    WiFi.softAP(apName, wcfg.apPass, wcfg.apChannel);
    if (!WiFi.softAPConfig(IPAddress(DEF_AP_IP), IPAddress(DEF_AP_GATEWAY), IPAddress(DEF_AP_MASK)))
        LOG.println(F("[WEB] Falha ao aplicar IP fixo do Soft AP — usando o padrão."));
    delay(100);
    dns.setErrorReplyCode(DNSReplyCode::NoError);
    dnsOn = dns.start(53, "*", WiFi.softAPIP());
    apOn = true;
    apIdleSinceMs = 0;
    LOG.printf("[WEB] Soft AP \"%s\" ativo — http://%s/ (senha %s)\r\n",
               apName, WiFi.softAPIP().toString().c_str(), wcfg.apPass);
}

static void stopAp(void)
{
    if (dnsOn) dns.stop();
    dnsOn = false;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apOn = false;
    LOG.println(F("[WEB] Station conectada: Soft AP de emergência desligado."));
}

static void staBegin(void)
{
    if (wcfg.ipStatic) {
        IPAddress ip, mask, gw, d1, d2;
        ip.fromString(wcfg.ip);
        mask.fromString(wcfg.mask[0] ? wcfg.mask : "255.255.255.0");
        gw.fromString(wcfg.gw);
        if (!wcfg.dns1[0] || !d1.fromString(wcfg.dns1)) d1 = gw;
        if (!wcfg.dns2[0] || !d2.fromString(wcfg.dns2)) d2 = IPAddress(8, 8, 8, 8);
        WiFi.config(ip, gw, mask, d1, d2);
    }
    WiFi.begin(wcfg.ssid, wcfg.pass);
    staStartMs = staRetryMs = millis();
}

static void wifiStart(void)
{
    nodeApSsid(&wcfg, apName, sizeof(apName));
    nodeHostname(&wcfg, host, sizeof(host));
    WiFi.persistent(false);
    WiFi.setHostname(host);

    if (forcedAp || wcfg.wifiMode == NODE_WIFI_AP) {
        startAp(false);
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    if (!wcfg.ssid[0]) {                 /* nothing configured yet */
        LOG.println(F("[WEB] Nenhuma rede Wi-Fi configurada."));
        startAp(false);
        return;
    }
    LOG.printf("[WEB] Conectando na rede \"%s\"...\r\n", wcfg.ssid);
    staBegin();
}

/* Called every loop of the web task. */
static void wifiSupervise(void)
{
    uint32_t now = millis();
    bool sta = WiFi.status() == WL_CONNECTED;

    if (sta && !mdnsOn) {
        mdnsOn = MDNS.begin(host);
        if (mdnsOn) MDNS.addService("http", "tcp", 80);
        LOG.printf("[WEB] Wi-Fi conectado: http://%s/  ou  http://%s.local/  (RSSI %d dBm)\r\n",
                   WiFi.localIP().toString().c_str(), host, WiFi.RSSI());
    }

    if (forcedAp || wcfg.wifiMode != NODE_WIFI_STA || !wcfg.ssid[0])
        return;

    if (!apOn && !sta && now - staStartMs > wcfg.staTimeout * 1000UL) {
        LOG.printf("[WEB] Sem conexão Wi-Fi há %u s — abrindo Soft AP de emergência.\r\n", wcfg.staTimeout);
        WiFi.setAutoReconnect(false);     /* STA scans would disturb the AP */
        startAp(true);
        staRetryMs = now;
    }
    if (apOn && !sta && WiFi.softAPgetStationNum() == 0 && now - staRetryMs > STA_RETRY_MS) {
        staRetryMs = now;                 /* retry only when nobody uses the AP */
        WiFi.disconnect(false);
        staBegin();
    }
    if (apOn && sta) {
        if (WiFi.softAPgetStationNum() > 0) apIdleSinceMs = 0;
        else if (!apIdleSinceMs) apIdleSinceMs = now ? now : 1;
        else if (now - apIdleSinceMs > AP_IDLE_OFF_MS) { stopAp(); WiFi.setAutoReconnect(true); }
    }
}

/* ===================================================================== */
/*                               handlers                                */
/* ===================================================================== */
static void handleRoot(void)
{
    if (!auth()) return;
    noStore();
    server.send_P(200, "text/html; charset=utf-8", PAGE_MAIN, sizeof(PAGE_MAIN) - 1);
}

static void handleFavicon(void)
{
    server.sendHeader("Cache-Control", "max-age=86400");
    server.send_P(200, "image/svg+xml", FAVICON_SVG, sizeof(FAVICON_SVG) - 1);
}

/* GET /config.json */
static void handleConfigJson(void)
{
    static char buf[3072];
    JBuf j = { buf, sizeof(buf), 0 };
    const NodeConfig &c = wcfg;
    char t[40];
    uint8_t mac[6];

    if (!auth()) return;
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    buf[0] = 0;
    jraw(&j, "{");
    jstr(&j, "_mode", isOtaa() ? "otaa" : "abp");
    jstr(&j, "_fw", NODE_FW_NAME " " NODE_FW_VERSION);
    jstr(&j, "_lmic", NODE_LMIC_VERSION);
    jstr(&j, "_region", NODE_REGION);
    jhex(&j, "_mac", mac, 6, ':');
    jstr(&j, "_chip", ESP.getChipModel());

    jstr(&j, "name", c.name);
    jstr(&j, "wifi_mode", c.wifiMode == NODE_WIFI_AP ? "ap" : c.wifiMode == NODE_WIFI_OFF ? "off" : "sta");
    jstr(&j, "wifi_ssid", c.ssid);
    jbool(&j, "wifi_pswd_set", c.pass[0] != 0);
    jstr(&j, "ip_mode", c.ipStatic ? "static" : "dhcp");
    jstr(&j, "ip_addr", c.ip);
    jstr(&j, "ip_mask", c.mask);
    jstr(&j, "ip_gw", c.gw);
    jstr(&j, "ip_dns1", c.dns1);
    jstr(&j, "ip_dns2", c.dns2);
    jstr(&j, "ap_ssid", c.apSsid);
    jbool(&j, "ap_pswd_set", c.apPass[0] != 0);
    jint(&j, "ap_ch", c.apChannel);
    jint(&j, "sta_to", c.staTimeout);
    jint(&j, "boot_pin", c.bootPin);
    jstr(&j, "web_user", c.webUser);

    jhex(&j, "dev_eui", c.devEui, 8);
    jhex(&j, "join_eui", c.joinEui, 8);
    jhex(&j, "app_key", c.appKey, 16);
    jonoff(&j, "eui_rev", c.euiReverse);
    snprintf(t, sizeof(t), "%08lX", (unsigned long)c.devAddr);  jstr(&j, "dev_addr", t);
    jhex(&j, "nwk_skey", c.nwkSKey, 16);
    jhex(&j, "app_skey", c.appSKey, 16);
    snprintf(t, sizeof(t), "%06lX", (unsigned long)c.netId);    jstr(&j, "net_id", t);
    jint(&j, "rx1_delay", c.rx1Delay);

    jint(&j, "sub_band", c.subBand);
    jonoff(&j, "ch500", c.ch500);
    jstr(&j, "ch_list", c.chList);
    jonoff(&j, "adr", c.adr);
    jint(&j, "dr", c.dr);
    jint(&j, "tx_pow", c.txPow);
    jonoff(&j, "link_chk", c.linkCheck);
    jint(&j, "clk_err", c.clockErr);
    jint(&j, "rx2_dr", c.rx2Dr);
    if (c.rx2Freq) jint(&j, "rx2_freq", c.rx2Freq); else jstr(&j, "rx2_freq", "");

    jint(&j, "tx_int", c.txInterval);
    jint(&j, "fport", c.fport);
    jonoff(&j, "confirmed", c.confirmed);
    jstr(&j, "pl_fmt", c.payloadHex ? "hex" : "text");
    if (c.payloadHex) {
        char h[PAYLOAD_MAX * 3 + 1];
        bytesToHex(c.payload, c.payloadLen, h, ' ');
        jstr(&j, "payload", h);
    } else {
        jkey(&j, "payload");
        jstrn(&j, (const char *)c.payload, c.payloadLen);
    }

    jonoff(&j, "rst_en", c.rstEnable);
    jint(&j, "rst_port", c.rstPort);
    jhex(&j, "rst_cmd", c.rstCmd, c.rstLen, ' ');
    jint(&j, "rst_delay", c.rstDelay);

    jint(&j, "pin_sclk", c.pinSclk);
    jint(&j, "pin_miso", c.pinMiso);
    jint(&j, "pin_mosi", c.pinMosi);
    jint(&j, "pin_nss", c.pinNss);
    jint(&j, "pin_rst", c.pinRst);
    jint(&j, "pin_dio0", c.pinDio0);
    jint(&j, "pin_dio1", c.pinDio1);
    jint(&j, "pin_dio2", c.pinDio2);
    jint(&j, "spi_freq", c.spiFreq);
    jint(&j, "baud", c.baud);
    jonoff(&j, "verbose", c.verbose);
    jraw(&j, "}");

    noStore();
    server.send(200, "application/json", buf);
}

/* Fields that only take effect after a reboot (compared with the boot copy). */
static int rebootPending(char *list, size_t size)
{
    const NodeConfig &a = bootCfg, &b = wcfg;
    int n = 0;
    list[0] = 0;
#define DIFF(cond, label) if (cond) { if (n++) strlcat(list, ", ", size); strlcat(list, label, size); }
    DIFF(a.wifiMode != b.wifiMode || strcmp(a.ssid, b.ssid) || strcmp(a.pass, b.pass) ||
         a.ipStatic != b.ipStatic || strcmp(a.ip, b.ip) || strcmp(a.mask, b.mask) ||
         strcmp(a.gw, b.gw) || strcmp(a.dns1, b.dns1) || strcmp(a.dns2, b.dns2) ||
         strcmp(a.apSsid, b.apSsid) || strcmp(a.apPass, b.apPass) || a.apChannel != b.apChannel ||
         a.staTimeout != b.staTimeout || strcmp(a.name, b.name), "Wi-Fi");
    DIFF(a.bootPin != b.bootPin || a.pinSclk != b.pinSclk || a.pinMiso != b.pinMiso ||
         a.pinMosi != b.pinMosi || a.pinNss != b.pinNss || a.pinRst != b.pinRst ||
         a.pinDio0 != b.pinDio0 || a.pinDio1 != b.pinDio1 || a.pinDio2 != b.pinDio2 ||
         a.spiFreq != b.spiFreq, "pinos");
    DIFF(a.baud != b.baud, "velocidade da serial");
#undef DIFF
    return n;
}

/* GET /status */
static void handleStatus(void)
{
    static char buf[1400];
    JBuf j = { buf, sizeof(buf), 0 };
    NodeStatus s;
    uint32_t now = millis();
    char t[80];

    if (!auth()) return;
    nodeStatusGet(&s);
    buf[0] = 0;
    jraw(&j, "{\"node\":{");
    jint(&j, "up", now / 1000);
    jint(&j, "heap", ESP.getFreeHeap());
    jint(&j, "heap_min", ESP.getMinFreeHeap());
    bool sta = WiFi.status() == WL_CONNECTED;
    jstr(&j, "wifi", apOn && sta ? "apsta" : apOn ? "ap" : sta ? "sta" : (WiFi.getMode() == WIFI_OFF ? "off" : "connecting"));
    if (sta) { jint(&j, "rssi", WiFi.RSSI()); jstr(&j, "ip", WiFi.localIP().toString().c_str()); jstr(&j, "ssid", WiFi.SSID().c_str()); }
    else     { jkey(&j, "rssi"); jraw(&j, "null"); }
    if (apOn) { jstr(&j, "ap_ip", WiFi.softAPIP().toString().c_str()); jstr(&j, "ap_ssid", apName); jint(&j, "ap_clients", WiFi.softAPgetStationNum()); }
    jstr(&j, "host", host);
    jbool(&j, "forced_ap", forcedAp);
    rebootPending(t, sizeof(t));
    jstr(&j, "reboot", t);
    jraw(&j, "},\"lw\":{");
    jstr(&j, "mode", isOtaa() ? "otaa" : "abp");
    jbool(&j, "joined", s.joined);
    jint(&j, "join_tries", s.joinTries);
    snprintf(t, sizeof(t), "%08lX", (unsigned long)s.devAddr); jstr(&j, "devaddr", t);
    snprintf(t, sizeof(t), "%06lX", (unsigned long)s.netId);   jstr(&j, "netid", t);
    jint(&j, "fcnt_up", s.fcntUp);
    jint(&j, "fcnt_dn", s.fcntDn);
    jint(&j, "boot_fcnt", s.bootFcnt);
    jint(&j, "up", s.upCount);
    jint(&j, "ack", s.ackCount);
    jint(&j, "dn", s.dnCount);
    jint(&j, "last_tx", s.lastTxMs ? (long)((now - s.lastTxMs) / 1000) : -1);
    jint(&j, "next_tx", s.nextTxMs ? (long)((int32_t)(s.nextTxMs - now) / 1000) : -1);
    jbool(&j, "pending", s.txPending);
    jint(&j, "dr", s.dr);
    jint(&j, "txpow", s.txPow);
    jint(&j, "freq", s.freq);
    jint(&j, "interval", wcfg.txInterval);
    if (s.lastDnMs) {
        jkey(&j, "last_dn"); jraw(&j, "{");
        jint(&j, "port", s.lastDnPort);
        jint(&j, "len", s.lastDnLen);
        char h[sizeof(s.lastDn) * 3 + 1];
        bytesToHex(s.lastDn, s.lastDnLen < sizeof(s.lastDn) ? s.lastDnLen : sizeof(s.lastDn), h, ' ');
        jstr(&j, "hex", h);
        jint(&j, "age", (now - s.lastDnMs) / 1000);
        jint(&j, "rssi", s.rssi);
        jint(&j, "snr", s.snr);
        jraw(&j, "}");
    }
    jstr(&j, "ev", s.lastEvent);
    jint(&j, "ev_age", s.lastEventMs ? (long)((now - s.lastEventMs) / 1000) : -1);
    jraw(&j, "}}");

    noStore();
    server.send(200, "application/json", buf);
}

/* ------------------------------ POST /save ------------------------------ */
static String errs;

static void err(const char *m) { if (errs.length()) errs += "\n"; errs += m; }

static bool argStr(const char *name, char *dst, size_t size, const char *label, bool allowEmpty = true)
{
    if (!server.hasArg(name)) return false;
    String v = server.arg(name);
    v.trim();
    if (!allowEmpty && !v.length()) { err((String(label) + ": obrigatório.").c_str()); return false; }
    if (v.length() >= size) { err((String(label) + ": texto longo demais.").c_str()); return false; }
    strlcpy(dst, v.c_str(), size);
    return true;
}

static bool argLong(const char *name, long lo, long hi, long *out, const char *label)
{
    if (!server.hasArg(name)) return false;
    String v = server.arg(name);
    v.trim();
    if (!v.length()) return false;
    char *e;
    long x = strtol(v.c_str(), &e, 10);
    if (*e || x < lo || x > hi) {
        char m[96];
        snprintf(m, sizeof(m), "%s: use um número entre %ld e %ld.", label, lo, hi);
        err(m);
        return false;
    }
    *out = x;
    return true;
}

static bool argOnOff(const char *name, uint8_t *dst)
{
    if (!server.hasArg(name)) return false;
    *dst = server.arg(name) == "on" ? 1 : 0;
    return true;
}

static void argHex(const char *name, uint8_t *dst, size_t len, const char *label)
{
    if (!server.hasArg(name)) return;
    String v = server.arg(name);
    v.trim();
    if (!v.length()) return;                       /* empty keeps the value */
    if (!hexToBytes(v.c_str(), dst, len)) {
        char m[96];
        snprintf(m, sizeof(m), "%s: use exatamente %u bytes em hexadecimal (%u caracteres).", label, (unsigned)len, (unsigned)len * 2);
        err(m);
    }
}

static void argPin(const char *name, int8_t *dst, const char *label, bool optional)
{
    long v;
    if (argLong(name, optional ? -1 : 0, 48, &v, label)) *dst = (int8_t)v;
}

static void handleSave(void)
{
    NodeConfig n = wcfg;
    long v;
    char tmp[72];

    if (!auth()) return;
    errs = "";

    /* device / Wi-Fi */
    argStr("name", n.name, sizeof(n.name), "Nome do nó", false);
    if (server.hasArg("wifi_mode")) {
        String m = server.arg("wifi_mode");
        n.wifiMode = m == "ap" ? NODE_WIFI_AP : m == "off" ? NODE_WIFI_OFF : NODE_WIFI_STA;
    }
    argStr("wifi_ssid", n.ssid, sizeof(n.ssid), "Nome da rede (SSID)");
    if (server.hasArg("wifi_pswd") && server.arg("wifi_pswd").length()) {
        if (server.arg("wifi_pswd").length() < 8) err("Senha do Wi-Fi: mínimo de 8 caracteres.");
        else argStr("wifi_pswd", n.pass, sizeof(n.pass), "Senha do Wi-Fi");
    }
    if (server.hasArg("ip_mode")) n.ipStatic = server.arg("ip_mode") == "static";
    argStr("ip_addr", n.ip, sizeof(n.ip), "Endereço IP");
    argStr("ip_mask", n.mask, sizeof(n.mask), "Máscara");
    argStr("ip_gw", n.gw, sizeof(n.gw), "Gateway");
    argStr("ip_dns1", n.dns1, sizeof(n.dns1), "DNS primário");
    argStr("ip_dns2", n.dns2, sizeof(n.dns2), "DNS secundário");
    if (n.ipStatic) {
        if (!validIPv4(n.ip))   err("Endereço IP: use o formato 192.168.18.101.");
        if (!validIPv4(n.mask)) err("Máscara: use o formato 255.255.255.0.");
        if (!validIPv4(n.gw))   err("Gateway: use o formato 192.168.18.1.");
        if (n.dns1[0] && !validIPv4(n.dns1)) err("DNS primário: formato inválido.");
        if (n.dns2[0] && !validIPv4(n.dns2)) err("DNS secundário: formato inválido.");
    }
    argStr("ap_ssid", n.apSsid, sizeof(n.apSsid), "Nome do Soft AP");
    if (server.hasArg("ap_pswd") && server.arg("ap_pswd").length()) {
        if (server.arg("ap_pswd").length() < 8) err("Senha do Soft AP: mínimo de 8 caracteres.");
        else argStr("ap_pswd", n.apPass, sizeof(n.apPass), "Senha do Soft AP");
    }
    if (argLong("ap_ch", 1, 13, &v, "Canal do Soft AP")) n.apChannel = v;
    if (argLong("sta_to", 5, 3600, &v, "Tempo até o Soft AP")) n.staTimeout = v;
    if (argLong("boot_pin", -1, 48, &v, "Botão do Soft AP")) n.bootPin = v;

    /* web login */
    if (argStr("web_user", tmp, sizeof(n.webUser), "Usuário da página", false)) {
        if (strchr(tmp, ':') || !strcmp(tmp, WEB_LOGOUT_USER)) err("Usuário da página: sem dois-pontos.");
        else strlcpy(n.webUser, tmp, sizeof(n.webUser));
    }
    if (server.hasArg("web_pswd") && server.arg("web_pswd").length()) {
        String p = server.arg("web_pswd");
        if (p.length() < 6 || p.length() >= sizeof(n.webPass) || p.indexOf(':') >= 0)
            err("Nova senha da página: 6 a 31 caracteres, sem dois-pontos.");
        else strlcpy(n.webPass, p.c_str(), sizeof(n.webPass));
    }

    /* activation */
    argHex("dev_eui", n.devEui, 8, "DevEUI");
    argHex("join_eui", n.joinEui, 8, "JoinEUI / AppEUI");
    argHex("app_key", n.appKey, 16, "AppKey");
    argOnOff("eui_rev", &n.euiReverse);
    if (server.hasArg("dev_addr") && server.arg("dev_addr").length()) {
        uint8_t a[4];
        if (hexToBytes(server.arg("dev_addr").c_str(), a, 4))
            n.devAddr = (uint32_t)a[0] << 24 | (uint32_t)a[1] << 16 | (uint32_t)a[2] << 8 | a[3];
        else err("DevAddr: use 8 caracteres hexadecimais.");
    }
    argHex("nwk_skey", n.nwkSKey, 16, "NwkSKey");
    argHex("app_skey", n.appSKey, 16, "AppSKey");
    if (server.hasArg("net_id") && server.arg("net_id").length()) {
        uint8_t a[3];
        if (hexToBytes(server.arg("net_id").c_str(), a, 3))
            n.netId = (uint32_t)a[0] << 16 | (uint32_t)a[1] << 8 | a[2];
        else err("NetID: use 6 caracteres hexadecimais (000000 ChirpStack, 000013 TTN).");
    }
    if (argLong("rx1_delay", 1, 15, &v, "Atraso RX1")) n.rx1Delay = v;

    /* radio */
    if (argLong("sub_band", 0, 7, &v, "Sub-banda")) n.subBand = v;
    argOnOff("ch500", &n.ch500);
    if (argStr("ch_list", tmp, sizeof(n.chList), "Canais personalizados")) {
        uint8_t m[9];
        if (tmp[0] && parseChannelList(tmp, m) <= 0) err("Canais personalizados: use números de 0 a 71, ex.: 8-15,65.");
        else strlcpy(n.chList, tmp, sizeof(n.chList));
    }
    argOnOff("adr", &n.adr);
    if (argLong("dr", 0, 6, &v, "Data rate")) n.dr = v;
    if (argLong("tx_pow", 2, 20, &v, "Potência")) n.txPow = v;
    argOnOff("link_chk", &n.linkCheck);
    if (argLong("clk_err", 0, 10, &v, "Erro de relógio")) n.clockErr = v;
    if (argLong("rx2_dr", 8, 13, &v, "Data rate RX2")) n.rx2Dr = v;
    if (server.hasArg("rx2_freq")) {
        if (!server.arg("rx2_freq").length()) n.rx2Freq = 0;
        else if (argLong("rx2_freq", 100000000L, 999999999L, &v, "Frequência RX2 (Hz)")) n.rx2Freq = v;
    }

    /* uplink */
    if (argLong("tx_int", 5, 86400, &v, "Intervalo")) n.txInterval = v;
    if (argLong("fport", 1, 223, &v, "FPort do uplink")) n.fport = v;
    argOnOff("confirmed", &n.confirmed);
    if (server.hasArg("pl_fmt") && server.hasArg("payload")) {
        bool hex = server.arg("pl_fmt") == "hex";
        String p = server.arg("payload");
        if (hex) {
            uint8_t b[PAYLOAD_MAX];
            int l = hexToBytesVar(p.c_str(), b, PAYLOAD_MAX);
            if (l < 1) err("Payload: hexadecimal inválido (1 a 222 bytes).");
            else { n.payloadHex = 1; n.payloadLen = l; memcpy(n.payload, b, l); }
        } else {
            if (p.length() < 1 || p.length() > PAYLOAD_MAX) err("Payload: 1 a 222 bytes.");
            else { n.payloadHex = 0; n.payloadLen = p.length(); memcpy(n.payload, p.c_str(), p.length()); }
        }
    }

    /* remote reset */
    argOnOff("rst_en", &n.rstEnable);
    if (argLong("rst_port", 1, 223, &v, "FPort do reset")) n.rstPort = v;
    if (server.hasArg("rst_cmd") && server.arg("rst_cmd").length()) {
        uint8_t b[RST_CMD_MAX];
        int l = hexToBytesVar(server.arg("rst_cmd").c_str(), b, RST_CMD_MAX);
        if (l < 1) err("Comando de reset: 1 a 16 bytes em hexadecimal.");
        else { n.rstLen = l; memcpy(n.rstCmd, b, l); }
    }
    if (argLong("rst_delay", 0, 60000, &v, "Espera antes do reset")) n.rstDelay = v;

    /* hardware */
    argPin("pin_sclk", &n.pinSclk, "SCLK", false);
    argPin("pin_miso", &n.pinMiso, "MISO", false);
    argPin("pin_mosi", &n.pinMosi, "MOSI", false);
    argPin("pin_nss",  &n.pinNss,  "NSS", false);
    argPin("pin_rst",  &n.pinRst,  "RESET", true);
    argPin("pin_dio0", &n.pinDio0, "DIO0", false);
    argPin("pin_dio1", &n.pinDio1, "DIO1", true);
    argPin("pin_dio2", &n.pinDio2, "DIO2", true);
    if (argLong("spi_freq", 0, 10000000L, &v, "Clock SPI")) n.spiFreq = v;
    if (argLong("baud", 9600, 921600, &v, "Velocidade da serial")) n.baud = v;
    argOnOff("verbose", &n.verbose);

    if (errs.length()) {
        String b = "<h2>Nada foi salvo</h2><p>Corrija e tente de novo:</p><p style='text-align:left;white-space:pre-line;color:#c0362c'>";
        b += htmlEsc(errs.c_str());
        b += "</p><form action='/' method='get'><button class='p' type='submit'>Voltar</button></form>";
        sendSimple(NULL, b);
        return;
    }

    nodeConfigSanitize(&n);
    bool loginChanged = strcmp(n.webUser, wcfg.webUser) || strcmp(n.webPass, wcfg.webPass);
    if (!nodeConfigSave(&n)) {
        sendSimple(NULL, "<h2>Falha ao gravar</h2><p>A NVS não aceitou a gravação. Tente de novo.</p>"
                         "<form action='/' method='get'><button class='p'>Voltar</button></form>");
        return;
    }
    wcfg = n;
    nodeCfgPost(&n);                     /* the LoRaWAN loop applies its part */
    LOG.println(F("[WEB] Configuração salva pela página."));

    char pend[96];
    String b = "<h2>Configuração salva</h2><p>";
    b += isOtaa() ? "LoRaWAN, rádio, uplink e reset remoto já foram aplicados (mudança de chaves ou canais refaz o JOIN)."
                  : "LoRaWAN, rádio, uplink e reset remoto já foram aplicados.";
    if (rebootPending(pend, sizeof(pend))) {
        b += "</p><p><b>Precisa reiniciar para valer: ";
        b += pend;
        b += ".</b>";
    }
    if (loginChanged) b += "</p><p>O usuário/senha da página mudaram: o navegador vai pedir o novo login.";
    b += "</p><form action='/reboot' method='post'><button class='p' type='submit'>Reiniciar agora</button></form>"
         "<form action='/' method='get'><button type='submit'>Voltar</button></form>";
    sendSimple(NULL, b);
}

/* --------------------------- POST /action ---------------------------- */
static void handleAction(void)
{
    if (!auth()) return;
    String a = server.arg("a");
    const char *msg = NULL;
    noStore();

    if (a == "send") {
        nodeCmdPost(NODE_CMD_SEND_NOW);
        msg = "Uplink solicitado.";
    } else if (a == "restart") {
        nodeCmdPost(NODE_CMD_RESTART);
        msg = isOtaa() ? "Refazendo o JOIN." : "Sessão ABP reprovisionada.";
    } else if (a == "fcnt" && !isOtaa()) {
        String v = server.arg("v");
        char *e;
        unsigned long x = strtoul(v.c_str(), &e, 10);
        if (!v.length() || *e) {
            server.send(400, "application/json", "{\"ok\":false,\"error\":\"FCnt inválido\"}");
            return;
        }
        nodeCmdPost(NODE_CMD_SET_FCNT, x);
        msg = "FCnt alterado.";
    } else {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"ação desconhecida\"}");
        return;
    }
    String r = "{\"ok\":true,\"msg\":\"";
    r += msg;
    r += "\"}";
    server.send(200, "application/json", r);
}

static void rebootSoon(void)
{
    delay(400);                          /* let the response leave */
    nodeCmdPost(NODE_CMD_REBOOT);
}

static void handleReboot(void)
{
    if (!auth()) return;
    LOG.println(F("[WEB] Reinício pedido pela página."));
    sendSimple("<meta http-equiv='refresh' content='15; URL=/'>",
               "<h2>Reiniciando…</h2><p>A página volta sozinha em 15 segundos. Se o nó mudou de "
               "modo (Soft AP ↔ Station) ou de IP, acesse pelo novo endereço.</p>");
    rebootSoon();
}

static void handleFactory(void)
{
    if (!auth()) return;
    LOG.println(F("[WEB] Restaurando padrão de fábrica."));
    nodeConfigErase();
    sendSimple("<meta http-equiv='refresh' content='20; URL=/'>",
               "<h2>Padrão de fábrica restaurado</h2><p>O nó está reiniciando com os valores do "
               "<code>node_build.h</code>. Login da página: <b>" DEF_WEB_USER " / " DEF_WEB_PASS "</b>. "
               "O contador de frames (FCnt) foi mantido.</p>");
    rebootSoon();
}

/* ------------------------------ logs -------------------------------- */
static void handleLogsPage(void)
{
    if (!auth()) return;
    noStore();
    server.send_P(200, "text/html; charset=utf-8", PAGE_LOGS, sizeof(PAGE_LOGS) - 1);
}

static void handleLogsData(void)
{
    static char buf[4096];
    uint32_t start, next;
    char h[16];

    if (!auth()) return;
    uint32_t from = strtoul(server.arg("from").c_str(), NULL, 10);
    size_t n = nodeLogRead(from, buf, sizeof(buf), &start, &next);
    noStore();
    snprintf(h, sizeof(h), "%lu", (unsigned long)next);  server.sendHeader("X-Log-Next", h);
    snprintf(h, sizeof(h), "%lu", (unsigned long)start); server.sendHeader("X-Log-Start", h);
    server.setContentLength(n);
    server.send(200, "text/plain; charset=utf-8", "");
    if (n) server.sendContent(buf, n);
}

static void handleLogsDownload(void)
{
    static char buf[1024];
    uint32_t start, next, from;
    char disp[96];

    if (!auth()) return;
    snprintf(disp, sizeof(disp), "attachment; filename=\"%s_uptime%lus.log\"", host, (unsigned long)(millis() / 1000));
    server.sendHeader("Content-Disposition", disp);
    noStore();
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/plain; charset=utf-8", "");
    nodeLogRead(0, buf, 0, &start, &next);
    from = start;
    for (;;) {
        size_t n = nodeLogRead(from, buf, sizeof(buf), &start, &next), o = 0;
        if (!n) break;
        from = next;
        for (size_t i = 0; i < n; i++)
            if (buf[i] != '\r') buf[o++] = buf[i];
        server.sendContent(buf, o);
    }
    server.sendContent("");
}

/* ----------------------------- logout ------------------------------- */
static void handleLogout(void)
{
    noStore();
    if (!server.authenticate(WEB_LOGOUT_USER, WEB_LOGOUT_PASS)) {
        server.requestAuthentication(BASIC_AUTH, "", "");
        return;
    }
    server.send(200, "text/plain", "ok");
}

static void handleLoggedOut(void)
{
    sendSimple(NULL, "<h2>Você saiu</h2><p>Para voltar à configuração do nó, entre de novo com usuário e senha.</p>"
                     "<form action='/' method='get'><button class='p'>Entrar novamente</button></form>"
                     "<p style='font-size:13px'>Se o navegador entrar direto sem pedir a senha, feche todas as janelas dele.</p>");
}

/* Captive portal: any unknown host while the Soft AP is on goes to the page. */
static void handleNotFound(void)
{
    if (apOn) {
        String h = server.hostHeader();
        String ap = WiFi.softAPIP().toString();
        if (h != ap && h != String(host) + ".local") {
            server.sendHeader("Location", "http://" + ap + "/");
            server.send(302, "text/plain", "");
            return;
        }
    }
    server.send(404, "text/plain", "Not found");
}

/* ===================================================================== */
/*                                  task                                 */
/* ===================================================================== */
static void webTask(void *)
{
    wifiStart();

    server.on("/", HTTP_GET, handleRoot);
    server.on("/config.json", HTTP_GET, handleConfigJson);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/action", HTTP_POST, handleAction);
    server.on("/reboot", HTTP_POST, handleReboot);
    server.on("/factory", HTTP_POST, handleFactory);
    server.on("/logs", HTTP_GET, handleLogsPage);
    server.on("/logs_data", HTTP_GET, handleLogsData);
    server.on("/logs_download", HTTP_GET, handleLogsDownload);
    server.on("/logout", HTTP_GET, handleLogout);
    server.on("/saiu", HTTP_GET, handleLoggedOut);
    server.on("/favicon.svg", HTTP_GET, handleFavicon);
    server.on("/favicon.ico", HTTP_GET, handleFavicon);
    server.onNotFound(handleNotFound);
    server.begin();

    for (;;) {
        if (nodeCfgVersion() != cfgVerSeen) {      /* changed by console/loop */
            cfgVerSeen = nodeCfgVersion();
            nodeCfgShared(&wcfg);
        }
        if (forceApReq) {
            forceApReq = false;
            if (!apOn) {
                forcedAp = true;
                WiFi.setAutoReconnect(false);
                startAp(WiFi.status() == WL_CONNECTED);
            } else {
                LOG.printf("[WEB] Soft AP \"%s\" já está ativo — http://%s/\r\n", apName, WiFi.softAPIP().toString().c_str());
            }
        }
        if (dnsOn) dns.processNextRequest();
        server.handleClient();
        wifiSupervise();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

static void startTask(void)
{
    taskRunning = true;
    /* core 0: the Arduino loop (LMiC) runs on core 1 */
    xTaskCreatePinnedToCore(webTask, "node_web", 8192, NULL, 1, NULL, 0);
}

void nodeWebForceAp(void)
{
    LOG.println(F("[WEB] Abrindo o Soft AP de configuração..."));
    if (!taskRunning) {
        nodeCfgShared(&wcfg);
        cfgVerSeen = nodeCfgVersion();
        forcedAp = true;
        startTask();
    } else {
        forceApReq = true;
    }
}

void nodeWebBegin(bool forceAp)
{
    nodeCfgShared(&wcfg);
    cfgVerSeen = nodeCfgVersion();
    bootCfg  = wcfg;
    forcedAp = forceAp;
    nodeApSsid(&wcfg, apName, sizeof(apName));
    nodeHostname(&wcfg, host, sizeof(host));

    if (wcfg.wifiMode == NODE_WIFI_OFF && !forceAp) {
        WiFi.mode(WIFI_OFF);
        LOG.println(F("[WEB] Wi-Fi desligado (Inicialização). Segure o botão do Soft AP por 3 s, "
                      "ou digite wifi_ap no console, para abrir o Soft AP."));
        return;
    }
    startTask();
}

void nodeWebPrintAccess(void)
{
    if (wcfg.wifiMode == NODE_WIFI_OFF && !forcedAp) return;
    if (forcedAp || wcfg.wifiMode == NODE_WIFI_AP || !wcfg.ssid[0])
        LOG.printf("[WEB] Página: conecte no Wi-Fi \"%s\" (senha %s) e abra http://%s/\r\n",
                   apName, wcfg.apPass, IPAddress(DEF_AP_IP).toString().c_str());
    else
        LOG.printf("[WEB] Página: http://%s.local/ (ou o IP que aparecer abaixo). Sem Wi-Fi em %u s abre o Soft AP \"%s\".\r\n",
                   host, wcfg.staTimeout, apName);
    if (wcfg.bootPin >= 0)
        LOG.printf("[WEB] Segure o GPIO %d em nível baixo por 3 s para abrir o Soft AP a qualquer momento.\r\n", wcfg.bootPin);
}

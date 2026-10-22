/*
 * node_config.cpp — defaults, NVS load/save and validation.
 * SPDX-License-Identifier: MIT
 */
#include <Preferences.h>
#include "node_config.h"

NodeConfig cfg;

static void copyStr(char *dst, size_t size, const char *src)
{
    strncpy(dst, src ? src : "", size - 1);
    dst[size - 1] = '\0';
}

void nodeConfigDefaults(NodeConfig *c)
{
    static const uint8_t devEui[8]   = DEF_DEVEUI;
    static const uint8_t joinEui[8]  = DEF_JOINEUI;
    static const uint8_t appKey[16]  = DEF_APPKEY;
    static const uint8_t nwkSKey[16] = DEF_NWKSKEY;
    static const uint8_t appSKey[16] = DEF_APPSKEY;
    static const uint8_t rstCmd[]    = DEF_RST_CMD;

    memset(c, 0, sizeof(*c));
    c->magic   = NODE_CFG_MAGIC;
    c->version = NODE_CFG_VERSION;
    c->size    = sizeof(NodeConfig);

    copyStr(c->name, sizeof(c->name), DEF_NODE_NAME);

    c->wifiMode = DEF_WIFI_MODE;
    copyStr(c->ssid, sizeof(c->ssid), DEF_WIFI_SSID);
    copyStr(c->pass, sizeof(c->pass), DEF_WIFI_PASS);
    c->ipStatic = 0;
    copyStr(c->mask, sizeof(c->mask), "255.255.255.0");
    copyStr(c->apPass, sizeof(c->apPass), DEF_AP_PASS);
    c->apChannel  = DEF_AP_CHANNEL;
    c->staTimeout = DEF_STA_TIMEOUT_S;
    c->bootPin    = DEF_BOOT_PIN;

    copyStr(c->webUser, sizeof(c->webUser), DEF_WEB_USER);
    copyStr(c->webPass, sizeof(c->webPass), DEF_WEB_PASS);

    memcpy(c->devEui,  devEui,  8);
    memcpy(c->joinEui, joinEui, 8);
    memcpy(c->appKey,  appKey,  16);
    c->devAddr = DEF_DEVADDR;
    memcpy(c->nwkSKey, nwkSKey, 16);
    memcpy(c->appSKey, appSKey, 16);
    c->netId    = DEF_NETID;
    c->rx1Delay = DEF_RX1_DELAY_S;

    c->subBand   = DEF_SUBBAND;
    c->ch500     = DEF_CH500;
    c->adr       = DEF_ADR;
    c->dr        = DEF_DR;
    c->txPow     = DEF_TXPOW;
    c->linkCheck = DEF_LINKCHECK;
    c->clockErr  = DEF_CLOCK_ERR_PCT;
    c->rx2Dr     = DEF_RX2_DR;
    c->rx2Freq   = DEF_RX2_FREQ;

    c->txInterval = DEF_TX_INTERVAL_S;
    c->fport      = DEF_FPORT;
    c->confirmed  = DEF_CONFIRMED;
    c->payloadHex = 0;
    c->payloadLen = strlen(DEF_PAYLOAD) > PAYLOAD_MAX ? PAYLOAD_MAX : strlen(DEF_PAYLOAD);
    memcpy(c->payload, DEF_PAYLOAD, c->payloadLen);

    c->rstEnable = DEF_RST_ENABLE;
    c->rstPort   = DEF_RST_PORT;
    c->rstLen    = sizeof(rstCmd) > RST_CMD_MAX ? RST_CMD_MAX : sizeof(rstCmd);
    memcpy(c->rstCmd, rstCmd, c->rstLen);
    c->rstDelay  = DEF_RST_DELAY_MS;

    c->pinSclk = DEF_PIN_SCLK;
    c->pinMiso = DEF_PIN_MISO;
    c->pinMosi = DEF_PIN_MOSI;
    c->pinNss  = DEF_PIN_NSS;
    c->pinRst  = DEF_PIN_RESET;
    c->pinDio0 = DEF_PIN_DIO0;
    c->pinDio1 = DEF_PIN_DIO1;
    c->pinDio2 = DEF_PIN_DIO2;
    c->spiFreq = DEF_SPI_FREQ;
    c->baud    = DEF_SERIAL_BAUD;
    c->verbose = DEF_VERBOSE;
    c->euiReverse = DEF_EUI_REVERSE;
}

/* Bring every field back into its valid range (never trusts NVS/web). */
void nodeConfigSanitize(NodeConfig *c)
{
    NodeConfig d;
    nodeConfigDefaults(&d);

#define TERM(f) c->f[sizeof(c->f) - 1] = '\0'
    TERM(name); TERM(ssid); TERM(pass); TERM(ip); TERM(mask); TERM(gw);
    TERM(dns1); TERM(dns2); TERM(apSsid); TERM(apPass); TERM(webUser);
    TERM(webPass); TERM(chList);
#undef TERM
    if (!c->name[0]) copyStr(c->name, sizeof(c->name), d.name);
    if (c->wifiMode > NODE_WIFI_OFF) c->wifiMode = d.wifiMode;
    c->ipStatic = c->ipStatic ? 1 : 0;
    if (strlen(c->apPass) < 8) copyStr(c->apPass, sizeof(c->apPass), d.apPass);
    if (c->apChannel < 1 || c->apChannel > 13) c->apChannel = d.apChannel;
    if (c->staTimeout < 5 || c->staTimeout > 3600) c->staTimeout = d.staTimeout;
    if (c->bootPin < -1 || c->bootPin > 48) c->bootPin = d.bootPin;
    if (!c->webUser[0] || strchr(c->webUser, ':')) copyStr(c->webUser, sizeof(c->webUser), d.webUser);
    if (strlen(c->webPass) < 6 || strchr(c->webPass, ':')) copyStr(c->webPass, sizeof(c->webPass), d.webPass);

    c->netId &= 0xFFFFFF;
    if (c->rx1Delay < 1 || c->rx1Delay > 15) c->rx1Delay = d.rx1Delay;

    if (c->subBand > 7) c->subBand = d.subBand;
    c->ch500 = c->ch500 ? 1 : 0;
    uint8_t m[9];
    if (c->chList[0] && parseChannelList(c->chList, m) <= 0) c->chList[0] = '\0';
    c->adr = c->adr ? 1 : 0;
    if (c->dr > 6) c->dr = d.dr;                      /* AU915 uplink DR0..DR6 */
    if (c->txPow < 2 || c->txPow > 20) c->txPow = d.txPow;
    c->linkCheck = c->linkCheck ? 1 : 0;
    if (c->clockErr > 10) c->clockErr = d.clockErr;
    if (c->rx2Dr < 8 || c->rx2Dr > 13) c->rx2Dr = d.rx2Dr;      /* AU915 downlink DR8..DR13 */
    if (c->rx2Freq && (c->rx2Freq < 100000000UL || c->rx2Freq > 1000000000UL)) c->rx2Freq = 0;

    if (c->txInterval < 5 || c->txInterval > 86400) c->txInterval = d.txInterval;
    if (c->fport < 1 || c->fport > 223) c->fport = d.fport;
    c->confirmed  = c->confirmed ? 1 : 0;
    c->payloadHex = c->payloadHex ? 1 : 0;
    if (c->payloadLen > PAYLOAD_MAX) c->payloadLen = PAYLOAD_MAX;

    c->rstEnable = c->rstEnable ? 1 : 0;
    if (c->rstPort < 1 || c->rstPort > 223) c->rstPort = d.rstPort;
    if (c->rstLen < 1 || c->rstLen > RST_CMD_MAX) { c->rstLen = d.rstLen; memcpy(c->rstCmd, d.rstCmd, d.rstLen); }
    if (c->rstDelay > 60000) c->rstDelay = d.rstDelay;

    int8_t *pins[] = { &c->pinSclk, &c->pinMiso, &c->pinMosi, &c->pinNss, &c->pinRst, &c->pinDio0, &c->pinDio1, &c->pinDio2 };
    int8_t  defs[] = { d.pinSclk,   d.pinMiso,   d.pinMosi,   d.pinNss,   d.pinRst,   d.pinDio0,   d.pinDio1,   d.pinDio2 };
    for (int i = 0; i < 8; i++)
        if (*pins[i] < -1 || *pins[i] > 48) *pins[i] = defs[i];
    if (c->pinSclk < 0) c->pinSclk = d.pinSclk;     /* required pins */
    if (c->pinMiso < 0) c->pinMiso = d.pinMiso;
    if (c->pinMosi < 0) c->pinMosi = d.pinMosi;
    if (c->pinNss  < 0) c->pinNss  = d.pinNss;
    if (c->pinDio0 < 0) c->pinDio0 = d.pinDio0;
    if (c->spiFreq > 10000000UL) c->spiFreq = d.spiFreq;
    switch (c->baud) {
    case 9600: case 19200: case 38400: case 57600: case 115200: case 230400: case 460800: case 921600: break;
    default: c->baud = d.baud;
    }
    c->verbose = c->verbose ? 1 : 0;
    c->euiReverse = c->euiReverse ? 1 : 0;

    c->magic   = NODE_CFG_MAGIC;
    c->version = NODE_CFG_VERSION;
    c->size    = sizeof(NodeConfig);
}

bool nodeConfigLoad(NodeConfig *c)
{
    Preferences p;
    bool ok = false;

    nodeConfigDefaults(c);
    if (!p.begin("nodecfg", true))
        return false;                       /* namespace does not exist yet */
    size_t n = p.getBytesLength("cfg");
    if (n >= 6) {
        uint8_t *buf = (uint8_t *)malloc(n);
        if (buf && p.getBytes("cfg", buf, n) == n) {
            NodeConfig *s = (NodeConfig *)buf;
            if (s->magic == NODE_CFG_MAGIC) {
                /* keep the defaults of fields newer than the saved blob */
                size_t keep = n < sizeof(NodeConfig) ? n : sizeof(NodeConfig);
                memcpy(c, buf, keep);
                ok = true;
            }
        }
        free(buf);
    }
    p.end();
    nodeConfigSanitize(c);
    return ok;
}

bool nodeConfigSave(const NodeConfig *c)
{
    Preferences p;
    if (!p.begin("nodecfg", false))
        return false;
    bool ok = p.putBytes("cfg", c, sizeof(NodeConfig)) == sizeof(NodeConfig);
    p.end();
    return ok;
}

void nodeConfigErase(void)
{
    Preferences p;
    if (p.begin("nodecfg", false)) { p.clear(); p.end(); }
}

uint32_t nodeFcntLoad(void)
{
    Preferences p;
    uint32_t v = 0;
    if (p.begin("lmic", true)) { v = p.getUInt("seqnoUp", 0); p.end(); }
    return v;
}

void nodeFcntSave(uint32_t v)
{
    Preferences p;
    if (p.begin("lmic", false)) { p.putUInt("seqnoUp", v); p.end(); }
}

const char *nodeApSsid(const NodeConfig *c, char *buf, size_t size)
{
    copyStr(buf, size, c->apSsid[0] ? c->apSsid : c->name);
    return buf;
}

const char *nodeHostname(const NodeConfig *c, char *buf, size_t size)
{
    size_t j = 0;
    for (const char *s = c->name; *s && j < size - 1 && j < 31; s++) {
        char ch = *s;
        if (ch >= 'A' && ch <= 'Z') ch += 32;
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) buf[j++] = ch;
        else if (j && buf[j - 1] != '-') buf[j++] = '-';
    }
    while (j && buf[j - 1] == '-') j--;
    buf[j] = '\0';
    if (!j) copyStr(buf, size, "lorawan-node");
    return buf;
}

/* ------------------------------ helpers ------------------------------ */
static int hexNibble(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

int hexToBytesVar(const char *s, uint8_t *out, size_t max)
{
    size_t n = 0;
    int hi = -1;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    for (; *s; s++) {
        if (*s == ' ' || *s == '-' || *s == ':' || *s == ',') {
            if (hi >= 0) return -1;         /* odd digit before separator */
            continue;
        }
        if (*s == '0' && (s[1] == 'x' || s[1] == 'X') && hi < 0) { s++; continue; }
        int v = hexNibble(*s);
        if (v < 0) return -1;
        if (hi < 0) { hi = v; continue; }
        if (n >= max) return -1;
        out[n++] = (uint8_t)(hi << 4 | v);
        hi = -1;
    }
    return hi >= 0 ? -1 : (int)n;
}

bool hexToBytes(const char *s, uint8_t *out, size_t len)
{
    uint8_t tmp[32];
    if (len > sizeof(tmp)) return false;
    int n = hexToBytesVar(s, tmp, sizeof(tmp));
    if (n != (int)len) return false;
    memcpy(out, tmp, len);
    return true;
}

void bytesToHex(const uint8_t *in, size_t len, char *out, char sep)
{
    static const char H[] = "0123456789ABCDEF";
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (sep && i) out[j++] = sep;
        out[j++] = H[in[i] >> 4];
        out[j++] = H[in[i] & 15];
    }
    out[j] = '\0';
}

int parseChannelList(const char *s, uint8_t mask[9])
{
    int count = 0;
    memset(mask, 0, 9);
    while (*s) {
        while (*s == ' ' || *s == ',' || *s == ';') s++;
        if (!*s) break;
        if (*s < '0' || *s > '9') return -1;
        int a = strtol(s, (char **)&s, 10), b = a;
        while (*s == ' ') s++;
        if (*s == '-') {
            s++;
            while (*s == ' ') s++;
            if (*s < '0' || *s > '9') return -1;
            b = strtol(s, (char **)&s, 10);
        }
        if (a < 0 || b > 71 || a > b) return -1;
        for (int ch = a; ch <= b; ch++) {
            if (!(mask[ch >> 3] & (1 << (ch & 7)))) count++;
            mask[ch >> 3] |= 1 << (ch & 7);
        }
        while (*s == ' ') s++;
        if (*s && *s != ',' && *s != ';') return -1;
    }
    return count;
}

int nodeChannelMask(const NodeConfig *c, uint8_t mask[9])
{
    if (c->chList[0]) {
        int n = parseChannelList(c->chList, mask);
        if (n > 0) return n;
    }
    memset(mask, 0, 9);
    mask[c->subBand] = 0xFF;                       /* 125 kHz ch sb*8 .. sb*8+7 */
    if (c->ch500) {
        int ch = 64 + c->subBand;
        mask[ch >> 3] |= 1 << (ch & 7);
        return 9;
    }
    return 8;
}

bool validIPv4(const char *s)
{
    int parts = 0;
    while (*s) {
        if (*s < '0' || *s > '9') return false;
        long v = strtol(s, (char **)&s, 10);
        if (v > 255) return false;
        parts++;
        if (*s == '.') { s++; if (!*s) return false; }
        else if (*s) return false;
    }
    return parts == 4;
}

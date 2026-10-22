/*
 * || Project:          IoT Energy Meter with C/C++/FreeRTOS, Java/Spring,
 * ||                   TypeScript/Angular and Dart/Flutter.
 * || About:            End-to-end LoRaWAN network for monitoring electrical quantities.
 * || Version:          1.1 — web configuration page
 * || Hardware:         ESP32 + RFM95W
 * || LoRaWAN Stack:    MCCI Arduino LMiC 6.0.1
 * || Network Server:   ChirpStack — AU915, sub-band 1 (ch 8–15 + 65)
 * || Activation:       OTAA
 * || Author:           Adail dos Santos Silva
 * || E-mail:           adail101@hotmail.com
 * || WhatsApp:         +55 89 9 9412-9256
 * ||
 * || NOTES:
 * ||   • Every parameter below is now set from the WEB PAGE of the node
 * ||     (http://<ip>/ or http://lorawan-node-otaa.local/, login admin/lorawan101)
 * ||     and kept in the NVS. node_build.h holds only the factory defaults.
 * ||
 * || SPDX-License-Identifier: MIT
 */

/********************************************************************
 _____              __ _                       _   _
/  __ \            / _(_)                     | | (_)
| /  \/ ___  _ __ | |_ _  __ _ _   _ _ __ __ _| |_ _  ___  _ __
| |    / _ \| '_ \|  _| |/ _` | | | | '__/ _` | __| |/ _ \| '_ \
| \__/\ (_) | | | | | | | (_| | |_| | | | (_| | |_| | (_) | | | |
 \____/\___/|_| |_|_| |_|\__, |\__,_|_|  \__,_|\__|_|\___/|_| |_|
                          __/ |
                         |___/
********************************************************************/

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

#include "node_build.h"     /* ABP/OTAA + factory defaults             */
#include "node_config.h"    /* cfg: configuration kept in the NVS      */
#include "node_state.h"     /* status/commands shared with the web     */
#include "node_log.h"       /* LOG = Serial + RAM log for /logs        */
#include "node_web.h"       /* Wi-Fi + web server (core 0)             */

#if !NODE_ACTIVATION_OTAA
#error "This sketch is the OTAA firmware: set NODE_ACTIVATION_OTAA 1 in node_build.h"
#endif

/* Region of this LMiC build (lmic_project_config.h) — shown on the page. */
#if defined(CFG_au915)
const char *NODE_REGION = "AU915";
#elif defined(CFG_us915)
const char *NODE_REGION = "US915";
#elif defined(CFG_eu868)
const char *NODE_REGION = "EU868";
#elif defined(CFG_as923)
const char *NODE_REGION = "AS923";
#elif defined(CFG_kr920)
const char *NODE_REGION = "KR920";
#elif defined(CFG_in866)
const char *NODE_REGION = "IN866";
#else
const char *NODE_REGION = "?";
#endif

/* ------------------------------------------------------------------ */
/* Hardware pin mapping — ESP32                                       */
/*                                                                    */
/* The pins now come from the configuration (page → Hardware).        */
/* lmic_pins must still exist because the library references it; it  */
/* holds the factory defaults. The pin map really used is 'pins',     */
/* filled from cfg and given to os_init_ex().                         */
/* ------------------------------------------------------------------ */
const lmic_pinmap lmic_pins = {
    .nss   = DEF_PIN_NSS,
    .rxtx  = LMIC_UNUSED_PIN,
    .rst   = DEF_PIN_RESET,
    /* DIO2 is not required for LoRa mode on the RFM95W. */
    .dio   = { DEF_PIN_DIO0, DEF_PIN_DIO1, LMIC_UNUSED_PIN },
};

static lmic_pinmap pins;

static u1_t pinOrUnused(int8_t p) { return p < 0 ? LMIC_UNUSED_PIN : (u1_t)p; }

/* ------------------------------------------------------------------ */
/* LoRaWAN credentials — OTAA (ChirpStack AU915, sub-band 1)          */
/*                                                                    */
/* DevEUI, JoinEUI (AppEUI) and AppKey come from cfg (page →          */
/* Ativação OTAA), typed MSB first exactly as ChirpStack shows them.  */
/*                                                                    */
/* Byte order: the LMiC copies os_getDevEui()/os_getArtEui() as they  */
/* are into the Join Request, and LoRaWAN carries EUIs LSB first.     */
/*   cfg.euiReverse = 0 → bytes as typed (same as firmware 1.0); the  */
/*                        network server sees the EUI reversed.       */
/*   cfg.euiReverse = 1 → reversed (MCCI standard); the server sees   */
/*                        exactly the EUI typed on the page.          */
/* The page shows which DevEUI/JoinEUI ChirpStack will receive.       */
/* The AppKey is always MSB (it is a key, not a number).              */
/* ------------------------------------------------------------------ */
static void copyEui(u1_t *buf, const uint8_t *eui) {
    for (int i = 0; i < 8; i++)
        buf[i] = cfg.euiReverse ? eui[7 - i] : eui[i];
}

/* LMiC callbacks — copy keys from the configuration into RAM buffers. */
void os_getArtEui(u1_t *buf) { copyEui(buf, cfg.joinEui); }
void os_getDevEui(u1_t *buf) { copyEui(buf, cfg.devEui); }
void os_getDevKey(u1_t *buf) { memcpy(buf, cfg.appKey, 16); }

/* ------------------------------------------------------------------ */
/* Remote reset command — downlink protocol                           */
/*                                                                    */
/* FPort, command bytes, delay and on/off come from cfg (page →       */
/* Reset remoto). Factory default (8 bytes on FPort 101):             */
/*                                                                    */
/*  Byte:  0     1     2     3     4     5     6     7                */
/*        ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐           */
/*        │ HDR │ HDR │ CMD │ CMD │ CMD │ CMD │TAIL │TAIL │           */
/*        │0xAD │0xA1 │0xDE │0xAD │0xBE │0xEF │0x0D │0x0A │           */
/*        └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘           */
/*                                                                    */
/*  Header  (2 bytes): 0xAD 0xA1  — "AdAil" signature                 */
/*  Command (4 bytes): 0xDE 0xAD 0xBE 0xEF — reset magic word         */
/*  Tail    (2 bytes): 0x0D 0x0A  — CRLF end-of-frame marker          */
/*                                                                    */
/* Any byte mismatch causes the command to be silently ignored.       */
/* The reset is deferred by cfg.rstDelay ms to allow the ACK          */
/* (if confirmed downlink) to be transmitted before rebooting.        */
/* ------------------------------------------------------------------ */

/* LMiC job handle for the periodic uplink. */
static osjob_t txjob;

/* false if os_init_ex() failed (wrong pins): the page keeps working. */
static bool lmicOk;

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */
void do_send(osjob_t *j);

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Print a byte as two hex digits with a leading zero if needed. */
static void printHex2(uint8_t v) {
    if (v < 0x10) LOG.print('0');
    LOG.print(v, HEX);
}

/* Print a key array as XX-XX-XX-… */
static void printKey(const uint8_t *key, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (i) LOG.print('-');
        printHex2(key[i]);
    }
    LOG.println();
}

/* Print the session keys obtained after a successful JOIN. */
static void printSessionKeys(void) {
    u4_t      netid   = 0;
    devaddr_t devaddr = 0;
    u1_t      nwkKey[16];
    u1_t      artKey[16];

    LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);

    LOG.print(F("  netid:   ")); LOG.println(netid, DEC);
    LOG.print(F("  devaddr: 0x")); LOG.println(devaddr, HEX);
    LOG.print(F("  AppSKey: ")); printKey(artKey, sizeof(artKey));
    LOG.print(F("  NwkSKey: ")); printKey(nwkKey, sizeof(nwkKey));
}

/* Print the OTAA identity used for the JOIN. */
static void printJoinIds(void) {
    u1_t b[8];
    LOG.print(F("  DevEUI  (page): ")); printKey(cfg.devEui, 8);
    LOG.print(F("  JoinEUI (page): ")); printKey(cfg.joinEui, 8);
    os_getDevEui(b);
    LOG.print(F("  DevEUI as the server receives it: "));
    for (int i = 7; i >= 0; i--) printHex2(b[i]);
    LOG.println(cfg.euiReverse ? F("  (byte order: LSB / MCCI)") : F("  (byte order: as typed)"));
}

/* Record the last LMiC event for the web status panel. */
static void stEvent(const char *name) {
    nodeStatusLock();
    NodeStatus *s = nodeStatusRaw();
    strlcpy(s->lastEvent, name, sizeof(s->lastEvent));
    s->lastEventMs = millis();
    s->txPending   = (LMIC.opmode & OP_TXRXPEND) != 0;
    nodeStatusUnlock();
}

/* Schedule the next uplink cfg.txInterval seconds from now. */
static void scheduleNext(void) {
    os_setTimedCallback(&txjob, os_getTime() + sec2osticks(cfg.txInterval), do_send);
    nodeStatusLock();
    nodeStatusRaw()->nextTxMs = millis() + cfg.txInterval * 1000UL;
    nodeStatusUnlock();
}

/* ------------------------------------------------------------------ */
/* Remote reset handler                                               */
/*                                                                    */
/* Validates the reset command received on cfg.rstPort.               */
/* Returns true if the payload matches the expected pattern exactly.  */
/* ------------------------------------------------------------------ */
static bool isResetCommand(const uint8_t *data, uint8_t len) {
    if (len != cfg.rstLen) return false;
    return memcmp(data, cfg.rstCmd, cfg.rstLen) == 0;
}

static void handleDownlink(void) {
    if (LMIC.dataLen == 0) return;

    uint8_t  fport = LMIC.frame[LMIC.dataBeg - 1];
    uint8_t  len   = LMIC.dataLen;
    uint8_t *data  = &LMIC.frame[LMIC.dataBeg];

    /* Publish it on the web status panel. */
    nodeStatusLock();
    NodeStatus *s = nodeStatusRaw();
    s->dnCount++;
    s->lastDnMs   = millis();
    s->lastDnPort = fport;
    s->lastDnLen  = len;
    memcpy(s->lastDn, data, len < sizeof(s->lastDn) ? len : sizeof(s->lastDn));
    s->rssi       = LMIC.rssi - RSSI_OFF;
    s->snr        = LMIC.snr / 4;
    nodeStatusUnlock();

    /* Print every downlink regardless of port. */
    LOG.print(F("  Downlink received — "));
    LOG.print(len);
    LOG.print(F(" byte(s) on FPort "));
    LOG.print(fport);
    LOG.print(F(": "));
    for (uint8_t i = 0; i < len; ++i) {
        printHex2(data[i]);
        LOG.print(' ');
    }
    LOG.println();

    /* ---------------------------------------------------------- */
    /* Remote reset command — cfg.rstPort                         */
    /* ---------------------------------------------------------- */
    if (cfg.rstEnable && fport == cfg.rstPort) {
        LOG.printf("  [RESET PORT] Command received on FPort %u — validating...\r\n", cfg.rstPort);

        if (isResetCommand(data, len)) {
            LOG.printf("  [RESET] Valid reset command — rebooting in %u ms...\r\n", cfg.rstDelay);
            LOG.flush();
            delay(cfg.rstDelay);
            esp_restart();
        } else {
            LOG.println(F("  [RESET] Invalid payload — command ignored."));
            LOG.print(F("  Expected: "));
            for (uint8_t i = 0; i < cfg.rstLen; ++i) {
                printHex2(cfg.rstCmd[i]);
                if (i < cfg.rstLen - 1) LOG.print(' ');
            }
            LOG.println();
            LOG.print(F("  Received: "));
            for (uint8_t i = 0; i < len; ++i) {
                printHex2(data[i]);
                if (i < len - 1) LOG.print(' ');
            }
            LOG.println();
        }
    }
}

/* ------------------------------------------------------------------ */
/* Session diagnostics — called on EV_TXSTART (page → Hardware →      */
/* Diagnóstico de sessão)                                             */
/* ------------------------------------------------------------------ */
static void print_info_session(void) {
    LOG.print(F("LMiC NwkSKey: "));
    for (int i = 0; i < 16; i++) {
        if (LMIC.nwkKey[i] < 0x10) LOG.print('0');
        LOG.print(LMIC.nwkKey[i], HEX);
    }
    LOG.println();

    LOG.print(F("LMiC AppSKey: "));
    for (int i = 0; i < 16; i++) {
        if (LMIC.artKey[i] < 0x10) LOG.print('0');
        LOG.print(LMIC.artKey[i], HEX);
    }
    LOG.println();

    LOG.print(F("LMiC DevAddr: 0x"));
    LOG.println(LMIC.devaddr, HEX);
    LOG.println();

    LOG.print(F("Frame raw: "));
    for (int i = 0; i < LMIC.dataLen + LMIC.dataBeg; i++) {
        if (LMIC.frame[i] < 0x10) LOG.print('0');
        LOG.print(LMIC.frame[i], HEX);
        LOG.print(' ');
    }
    LOG.println();

    LOG.print(F("FCnt/seqnoUp: ")); LOG.println(LMIC.seqnoUp);
    LOG.print(F("FCnt/seqnoDn: ")); LOG.println(LMIC.seqnoDn);

    /* Print MIC — last 4 bytes of the frame. */
    uint16_t frameLen = LMIC.dataBeg + LMIC.dataLen;
    if (frameLen >= 4) {
        LOG.print(F("  MIC: "));
        for (uint16_t i = frameLen - 4; i < frameLen; i++) {
            if (LMIC.frame[i] < 0x10) LOG.print('0');
            LOG.print(LMIC.frame[i], HEX);
            if (i < frameLen - 1) LOG.print('-');
        }
        LOG.println();
    }
}

/* ------------------------------------------------------------------ */
/* LMiC event handler                                                 */
/* ------------------------------------------------------------------ */

/*****************************
 _____           _
/  __ \         | |
| /  \/ ___   __| | ___
| |    / _ \ / _` |/ _ \
| \__/\ (_) | (_| |  __/
 \____/\___/ \__,_|\___|
*****************************/

void onEvent(ev_t ev) {
    switch (ev) {

        case EV_JOINING:
            LOG.println(F("EV_JOINING"));
            printJoinIds();
            stEvent("EV_JOINING");
            break;

        case EV_JOIN_TXCOMPLETE:
            /* No JoinAccept received in the join windows — LMiC will retry. */
            LOG.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
            stEvent("EV_JOIN_TXCOMPLETE");
            break;

        case EV_JOIN_FAILED:
            LOG.println(F("EV_JOIN_FAILED"));
            stEvent("EV_JOIN_FAILED");
            break;

        case EV_REJOIN_FAILED:
            LOG.println(F("EV_REJOIN_FAILED"));
            stEvent("EV_REJOIN_FAILED");
            break;

        case EV_JOINED:
            LOG.println(F("EV_JOINED"));
            printSessionKeys();
            /*
             * Link-check mode is automatically enabled after JOIN, but
             * it can force the stack to use lower data rates and shrink
             * the maximum payload size. Must be set here AND in
             * lmicStart() for OTAA (page → Rádio LoRa → Link check).
             */
            LMIC_setLinkCheckMode(cfg.linkCheck);
            nodeStatusLock();
            {
                NodeStatus *s = nodeStatusRaw();
                s->joined  = true;
                s->devAddr = LMIC.devaddr;
                s->netId   = LMIC.netid;
                s->fcntUp  = LMIC.seqnoUp;
                s->fcntDn  = LMIC.seqnoDn;
            }
            nodeStatusUnlock();
            stEvent("EV_JOINED");
            break;

        case EV_TXSTART:
            LOG.println();
            LOG.println(F("EV_TXSTART"));
            if (cfg.verbose) print_info_session();
            nodeStatusLock();
            {
                NodeStatus *s = nodeStatusRaw();
                if (LMIC.opmode & OP_JOINING) s->joinTries++;
                s->freq  = LMIC.freq;
                s->dr    = LMIC.datarate;
                s->txPow = LMIC.txpow;
                s->nextTxMs = 0;
            }
            nodeStatusUnlock();
            stEvent("EV_TXSTART");
            break;

        case EV_RXSTART:
            /*
             * Do NOT print here — any Serial output during RX setup
             * consumes time and can mis-align the receive window.
             */
            break;

        case EV_TXCOMPLETE:
            LOG.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));

            if (LMIC.txrxFlags & TXRX_ACK) {
                LOG.println(F("  ACK received."));
            }

            nodeStatusLock();
            {
                NodeStatus *s = nodeStatusRaw();
                s->upCount++;
                if (LMIC.txrxFlags & TXRX_ACK) s->ackCount++;
                s->lastTxMs = millis();
                s->fcntUp   = LMIC.seqnoUp;
                s->fcntDn   = LMIC.seqnoDn;
            }
            nodeStatusUnlock();

            /*
             * Handle downlink — including the remote reset command.
             * Called here because LMIC.dataLen and LMIC.frame are
             * only valid inside EV_TXCOMPLETE.
             */
            handleDownlink();

            /* Schedule the next uplink. */
            scheduleNext();
            stEvent("EV_TXCOMPLETE");
            break;

        case EV_TXCANCELED:
            LOG.println(F("EV_TXCANCELED"));
            stEvent("EV_TXCANCELED");
            break;

        case EV_LINK_DEAD:
            /*
             * Fired when the network has not acknowledged several
             * consecutive uplinks.
             */
            LOG.println(F("EV_LINK_DEAD"));
            stEvent("EV_LINK_DEAD");
            break;

        case EV_LINK_ALIVE:
            LOG.println(F("EV_LINK_ALIVE"));
            stEvent("EV_LINK_ALIVE");
            break;

        case EV_RESET:
            LOG.println(F("EV_RESET"));
            stEvent("EV_RESET");
            break;

        case EV_RXCOMPLETE:
            /* Class B ping-slot reception (not used here). */
            LOG.println(F("EV_RXCOMPLETE"));
            stEvent("EV_RXCOMPLETE");
            break;

        case EV_SCAN_TIMEOUT:   LOG.println(F("EV_SCAN_TIMEOUT"));   break;
        case EV_BEACON_FOUND:   LOG.println(F("EV_BEACON_FOUND"));   break;
        case EV_BEACON_MISSED:  LOG.println(F("EV_BEACON_MISSED"));  break;
        case EV_BEACON_TRACKED: LOG.println(F("EV_BEACON_TRACKED")); break;
        case EV_LOST_TSYNC:     LOG.println(F("EV_LOST_TSYNC"));     break;

        default:
            LOG.print(F("Unknown event: "));
            LOG.println((unsigned)ev);
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Uplink function                                                    */
/* ------------------------------------------------------------------ */

void do_send(osjob_t *j) {
    (void)j;
    if (LMIC.opmode & OP_TXRXPEND) {
        LOG.println(F("OP_TXRXPEND — previous job still running, skipping."));
        return;
    }

    /* TODO: read real sensor data into cfg.payload before calling setTxData2. */
    LMIC_setTxData2(cfg.fport, cfg.payload, cfg.payloadLen, cfg.confirmed);
    LOG.println(F("Packet queued."));
    stEvent("Packet queued");
}

/* ------------------------------------------------------------------ */
/* Radio parameters that can change at any time (no session reset)    */
/* ------------------------------------------------------------------ */
static void applyRadioLive(void) {
    /* Adaptive Data Rate — off = fixed DR/power from cfg.            */
    LMIC_setAdrMode(cfg.adr);

    /*
     * Link-check mode is set here AND again in EV_JOINED
     * because the JOIN procedure re-enables it automatically.
     */
    LMIC_setLinkCheckMode(cfg.linkCheck);

    /* Default DR5 = SF7/BW125 for AU915 — highest DR, lowest range.  */
    LMIC_setDrTxpow((dr_t)cfg.dr, cfg.txPow);

    /*
     * Clock-error compensation.
     * The ESP32 XTAL is accurate enough that 1 % is sufficient to
     * widen the RX preamble detection window without introducing a
     * significant delay before the window opens.
     */
    LMIC_setClockError(MAX_CLOCK_ERROR * cfg.clockErr / 100);
}

/* ------------------------------------------------------------------ */
/* (Re)start the LoRaWAN stack with the current cfg.                  */
/* The first do_send() also triggers the OTAA JOIN.                   */
/* ------------------------------------------------------------------ */
static void lmicStart(void) {
    os_clearCallback(&txjob);
    LMIC_reset();

#if CFG_LMIC_US_like
    /* -------------------------------------------------------------- */
    /* Channel plan — default ChirpStack AU915, sub-band 1            */
    /* Uplink:   ch  8–15  (916.8–918.2 MHz, 200 kHz, BW125)          */
    /* Uplink:   ch 65     (917.5 MHz, BW500) — JOIN / ADR            */
    /* Downlink: ch  0– 7  (923.3–924.7 MHz, BW500) — managed by NS   */
    /* Page → Rádio LoRa: sub-band, 500 kHz channel or custom list.   */
    /* -------------------------------------------------------------- */
    uint8_t mask[9];
    int n = nodeChannelMask(&cfg, mask);
    for (u1_t b  = 0; b  <  8; ++b)  LMIC_disableSubBand(b);
    for (u1_t ch = 0; ch < 72; ++ch) LMIC_disableChannel(ch);
    for (u1_t ch = 0; ch < 72; ++ch)
        if (mask[ch >> 3] & (1 << (ch & 7))) LMIC_enableChannel(ch);
    LOG.printf("  Channels: %d enabled (%s)\r\n", n,
               cfg.chList[0] ? cfg.chList : "sub-band from config");
#endif

    applyRadioLive();

    /*
     * RX2 window — AU915/ChirpStack default:
     *   923.3 MHz, SF12/BW500 → DR8 (DR_SF12CR).
     * The JOIN Accept sets this automatically in OTAA.
     * Defined explicitly here as a safeguard.
     */
    LMIC.dn2Dr = cfg.rx2Dr;
    if (cfg.rx2Freq) LMIC.dn2Freq = cfg.rx2Freq;

    nodeStatusLock();
    {
        NodeStatus *s = nodeStatusRaw();
        s->joined  = false;
        s->devAddr = 0;
        s->netId   = 0;
        s->fcntUp  = LMIC.seqnoUp;
        s->fcntDn  = LMIC.seqnoDn;
        s->dr      = cfg.dr;
        s->txPow   = cfg.txPow;
    }
    nodeStatusUnlock();

    /* Kick off the first uplink — also triggers the OTAA JOIN. */
    do_send(&txjob);
}

/* ------------------------------------------------------------------ */
/* Commands from the web page / new configuration                     */
/* Executed only when no TX/RX is in progress (LMiC is not reentrant) */
/* ------------------------------------------------------------------ */
static uint32_t deferredCmd;

static bool activationChanged(const NodeConfig &a, const NodeConfig &b) {
    return memcmp(a.devEui, b.devEui, 8) || memcmp(a.joinEui, b.joinEui, 8) ||
           memcmp(a.appKey, b.appKey, 16) || a.euiReverse != b.euiReverse ||
           a.subBand != b.subBand || a.ch500 != b.ch500 || strcmp(a.chList, b.chList) ||
           a.rx2Dr != b.rx2Dr || a.rx2Freq != b.rx2Freq;
}

static bool radioChanged(const NodeConfig &a, const NodeConfig &b) {
    return a.adr != b.adr || a.dr != b.dr || a.txPow != b.txPow ||
           a.linkCheck != b.linkCheck || a.clockErr != b.clockErr;
}

static void handleCommands(void) {
    uint32_t arg = 0;
    uint32_t cmd = nodeCmdTake(&arg) | deferredCmd;
    (void)arg;                            /* NODE_CMD_SET_FCNT is ABP only */
    deferredCmd = 0;
    if (!cmd) return;

    if (cmd & NODE_CMD_REBOOT) {
        LOG.println(F("Rebooting (requested by the web page)..."));
        LOG.flush();
        delay(100);
        esp_restart();
    }

    if (!lmicOk) {                        /* only the reboot works */
        NodeConfig n;
        if (nodeCfgTake(&n)) cfg = n;
        return;
    }

    if (LMIC.opmode & OP_TXRXPEND) {      /* try again after this TX/RX */
        deferredCmd = cmd;
        return;
    }

    if (cmd & NODE_CMD_APPLY) {
        NodeConfig n;
        if (nodeCfgTake(&n)) {
            NodeConfig old = cfg;
            cfg = n;
            LOG.println(F("[CFG] New configuration from the web page."));
            if (activationChanged(old, n)) {
                LOG.println(F("[CFG] Keys/channels changed — new JOIN."));
                lmicStart();
                cmd &= ~(NODE_CMD_RESTART | NODE_CMD_SEND_NOW);
            } else {
                if (radioChanged(old, n)) {
                    applyRadioLive();
                    LOG.println(F("[CFG] Radio parameters applied."));
                }
                if (old.txInterval != n.txInterval) {
                    NodeStatus s;
                    nodeStatusGet(&s);
                    if (s.nextTxMs) scheduleNext();
                    LOG.printf("[CFG] Uplink interval: %lu s\r\n", (unsigned long)cfg.txInterval);
                }
            }
        }
    }

    if (cmd & NODE_CMD_RESTART) {
        LOG.println(F("[CMD] New JOIN requested by the web page."));
        lmicStart();
    } else if (cmd & NODE_CMD_SEND_NOW) {
        LOG.println(F("[CMD] Uplink requested by the web page."));
        os_clearCallback(&txjob);
        do_send(&txjob);
    }
}

/* ------------------------------------------------------------------ */
/* Serial console (Serial Monitor, line ending "Newline")             */
/* ------------------------------------------------------------------ */
static void consoleHelp(void) {
    LOG.println(F("Comandos: help | status | wifi_ap | web_auth --reset | send | rejoin | cfg_reset | reboot"));
}

static void consoleExec(char *line) {
    while (*line == ' ') line++;
    if (!*line) return;
    LOG.print(F("> ")); LOG.println(line);

    if (!strcmp(line, "help")) {
        consoleHelp();
    } else if (!strcmp(line, "status")) {
        NodeStatus s;
        nodeStatusGet(&s);
        LOG.printf("OTAA %s DevAddr %08lX  FCnt up %lu  dn %lu  uplinks %lu  downlinks %lu  heap %lu\r\n",
                   s.joined ? "joined" : "NOT joined", (unsigned long)s.devAddr,
                   (unsigned long)s.fcntUp, (unsigned long)s.fcntDn,
                   (unsigned long)s.upCount, (unsigned long)s.dnCount, (unsigned long)ESP.getFreeHeap());
        nodeWebPrintAccess();
    } else if (!strcmp(line, "wifi_ap")) {
        nodeWebForceAp();
    } else if (!strcmp(line, "web_auth --reset")) {
        strlcpy(cfg.webUser, DEF_WEB_USER, sizeof(cfg.webUser));
        strlcpy(cfg.webPass, DEF_WEB_PASS, sizeof(cfg.webPass));
        nodeConfigSave(&cfg);
        nodeCfgSetShared(&cfg);
        LOG.println(F("Login da página: " DEF_WEB_USER " / " DEF_WEB_PASS " (já vale)."));
    } else if (!strcmp(line, "rejoin")) {
        nodeCmdPost(NODE_CMD_RESTART);
    } else if (!strcmp(line, "send")) {
        nodeCmdPost(NODE_CMD_SEND_NOW);
    } else if (!strcmp(line, "cfg_reset")) {
        LOG.println(F("Configuração apagada — reiniciando com o padrão de fábrica."));
        nodeConfigErase();
        nodeCmdPost(NODE_CMD_REBOOT);
    } else if (!strcmp(line, "reboot")) {
        nodeCmdPost(NODE_CMD_REBOOT);
    } else {
        LOG.println(F("Comando desconhecido."));
        consoleHelp();
    }
}

static void consolePoll(void) {
    static char buf[48];
    static uint8_t n;
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r' || c == '\n') {
            buf[n] = 0;
            if (n) consoleExec(buf);
            n = 0;
        } else if (n < sizeof(buf) - 1) {
            buf[n++] = c;
        }
    }
}

/* BOOT button held 3 s -> Soft AP (GPIO 0 must NOT be held at power-on). */
static void bootButtonPoll(void) {
    static uint32_t downSince;
    static bool fired;
    if (cfg.bootPin < 0) return;
    if (digitalRead(cfg.bootPin) == LOW) {
        if (!downSince) downSince = millis() | 1;
        else if (!fired && millis() - downSince > 3000) { fired = true; nodeWebForceAp(); }
    } else {
        downSince = 0;
        fired = false;
    }
}

/* ------------------------------------------------------------------ */
/* Setup                                                              */
/* ------------------------------------------------------------------ */

/*****************************
 _____      _
/  ___|    | |
\ `--.  ___| |_ _   _ _ __
 `--. \/ _ \ __| | | | '_ \
/\__/ /  __/ |_| |_| | |_) |
\____/ \___|\__|\__,_| .__/
                     | |
                     |_|
*****************************/

void setup() {
    /* Configuration from the NVS (factory defaults on the first boot). */
    bool saved = nodeConfigLoad(&cfg);
    nodeCfgSetShared(&cfg);

    Serial.begin(cfg.baud);
    delay(100);

    LOG.println();
    LOG.println(F("Starting — " NODE_FW_NAME " " NODE_FW_VERSION " (" NODE_LMIC_VERSION ")"));
    LOG.printf("Node \"%s\" · region %s · config %s\r\n", cfg.name, NODE_REGION,
               saved ? "loaded from NVS" : "factory defaults (node_build.h)");

    /* BOOT / config button (checked in loop, 3 s press). */
    if (cfg.bootPin >= 0 && cfg.bootPin != cfg.pinNss && cfg.bootPin != cfg.pinRst &&
        cfg.bootPin != cfg.pinDio0 && cfg.bootPin != cfg.pinDio1 && cfg.bootPin != cfg.pinDio2 &&
        cfg.bootPin != cfg.pinSclk && cfg.bootPin != cfg.pinMiso && cfg.bootPin != cfg.pinMosi)
        pinMode(cfg.bootPin, INPUT_PULLUP);
    else
        cfg.bootPin = -1;

    /* Wi-Fi + web page (own task on core 0). */
    nodeWebBegin(false);
    nodeWebPrintAccess();

    /* Explicit SPI initialisation required on ESP32. */
    SPI.begin(cfg.pinSclk, cfg.pinMiso, cfg.pinMosi, cfg.pinNss);

#ifdef VCC_ENABLE
    /* Pinoccio Scout power rail enable. */
    pinMode(VCC_ENABLE, OUTPUT);
    digitalWrite(VCC_ENABLE, HIGH);
    delay(1000);
#endif

    /* Pin map from the configuration. */
    pins          = lmic_pins;
    pins.nss      = pinOrUnused(cfg.pinNss);
    pins.rst      = pinOrUnused(cfg.pinRst);
    pins.dio[0]   = pinOrUnused(cfg.pinDio0);
    pins.dio[1]   = pinOrUnused(cfg.pinDio1);
    pins.dio[2]   = pinOrUnused(cfg.pinDio2);
    pins.spi_freq = cfg.spiFreq;
    LOG.printf("Pins: SCLK %d MISO %d MOSI %d NSS %d RST %d DIO0 %d DIO1 %d DIO2 %d\r\n",
               cfg.pinSclk, cfg.pinMiso, cfg.pinMosi, cfg.pinNss, cfg.pinRst,
               cfg.pinDio0, cfg.pinDio1, cfg.pinDio2);

    if (!os_init_ex(&pins)) {
        LOG.println(F("LMiC init FAILED — check the pins on the web page (Hardware)."));
        stEvent("LMiC init FAILED");
        return;   /* the web page keeps working so the pins can be fixed */
    }

    lmicOk = true;
    lmicStart();
}

/* ------------------------------------------------------------------ */
/* Main loop — hand control to the LMiC cooperative scheduler         */
/* ------------------------------------------------------------------ */

/*****************************
 _
| |
| |     ___   ___  _ __
| |    / _ \ / _ \| '_ \
| |___| (_) | (_) | |_) |
\_____/\___/ \___/| .__/
                  | |
                  |_|
*****************************/

void loop() {
    if (lmicOk) os_runloop_once();
    handleCommands();
    consolePoll();
    bootButtonPoll();
    if (!lmicOk) { delay(10); return; }

    /* keep "transmitting now" fresh for the status panel */
    bool pend = (LMIC.opmode & OP_TXRXPEND) != 0;
    nodeStatusLock();
    nodeStatusRaw()->txPending = pend;
    nodeStatusUnlock();
}

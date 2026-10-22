/*
 * node_config.h — runtime configuration of the LoRaWAN node.
 *
 * Kept in the ESP32 NVS (namespace "nodecfg", key "cfg") as one blob.
 * The struct may only GROW at the end: an older blob is loaded over the
 * factory defaults, so new fields keep their default value.
 *
 * The uplink frame counter of ABP stays where the original firmware put
 * it (namespace "lmic", key "seqnoUp"), so an upgraded node keeps its FCnt.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef NODE_CONFIG_H
#define NODE_CONFIG_H

#include <Arduino.h>
#include "node_build.h"

#define NODE_CFG_MAGIC      0x4C4E   /* "NL" */
#define NODE_CFG_VERSION    1

#define PAYLOAD_MAX         222      /* AU915 DR4..DR6 max. application payload */
#define RST_CMD_MAX         16

enum { NODE_WIFI_STA = 0, NODE_WIFI_AP = 1, NODE_WIFI_OFF = 2 };

typedef struct {
    uint16_t magic;
    uint16_t version;
    uint16_t size;

    /* ------------------------------ device ----------------------------- */
    char     name[32];              /* hostname / AP SSID / page title     */

    /* ------------------------------ Wi-Fi ------------------------------ */
    uint8_t  wifiMode;              /* WIFI_MODE_*                          */
    char     ssid[33];
    char     pass[65];
    uint8_t  ipStatic;
    char     ip[16], mask[16], gw[16], dns1[16], dns2[16];
    char     apSsid[33];            /* empty = <name>                       */
    char     apPass[65];
    uint8_t  apChannel;
    uint16_t staTimeout;            /* s                                    */
    int8_t   bootPin;               /* 3 s press opens the Soft AP, -1 = off */

    /* ---------------------------- web login ---------------------------- */
    char     webUser[32];
    char     webPass[32];

    /* --------------------------- activation ---------------------------- */
    uint8_t  devEui[8];             /* MSB first                            */
    uint8_t  joinEui[8];
    uint8_t  appKey[16];
    uint32_t devAddr;
    uint8_t  nwkSKey[16];
    uint8_t  appSKey[16];
    uint32_t netId;
    uint8_t  rx1Delay;              /* s, ABP only                          */

    /* ------------------------------ radio ------------------------------ */
    uint8_t  subBand;               /* 0..7                                 */
    uint8_t  ch500;                 /* enable 500 kHz channel 64+subBand    */
    char     chList[48];            /* "8-15,65" — overrides subBand        */
    uint8_t  adr;
    uint8_t  dr;
    int8_t   txPow;                 /* dBm                                  */
    uint8_t  linkCheck;
    uint8_t  clockErr;              /* % of MAX_CLOCK_ERROR                 */
    uint8_t  rx2Dr;
    uint32_t rx2Freq;               /* Hz, 0 = region default               */

    /* ------------------------------ uplink ----------------------------- */
    uint32_t txInterval;            /* s                                    */
    uint8_t  fport;
    uint8_t  confirmed;
    uint8_t  payloadHex;            /* how the page shows it                */
    uint8_t  payloadLen;
    uint8_t  payload[PAYLOAD_MAX];

    /* ------------------- remote reset by downlink ---------------------- */
    uint8_t  rstEnable;
    uint8_t  rstPort;
    uint8_t  rstLen;
    uint8_t  rstCmd[RST_CMD_MAX];
    uint16_t rstDelay;              /* ms                                   */

    /* ----------------------------- hardware ---------------------------- */
    int8_t   pinSclk, pinMiso, pinMosi, pinNss, pinRst, pinDio0, pinDio1, pinDio2;
    uint32_t spiFreq;               /* Hz, 0 = LMiC default                 */
    uint32_t baud;
    uint8_t  verbose;               /* session dump on EV_TXSTART           */

    /* ------------------- appended in version 1 ------------------------- */
    uint8_t  euiReverse;            /* OTAA: give DevEUI/JoinEUI to LMiC
                                       reversed (LSB first, MCCI standard) */
} NodeConfig;

/* The configuration in use by the LoRaWAN loop. */
extern NodeConfig cfg;

void nodeConfigDefaults(NodeConfig *c);
bool nodeConfigLoad(NodeConfig *c);            /* false = factory defaults */
bool nodeConfigSave(const NodeConfig *c);
void nodeConfigErase(void);                    /* back to factory defaults */
void nodeConfigSanitize(NodeConfig *c);

/* Soft AP name actually used (apSsid or name). */
const char *nodeApSsid(const NodeConfig *c, char *buf, size_t size);
/* Valid DNS host name made from c->name. */
const char *nodeHostname(const NodeConfig *c, char *buf, size_t size);

/* ABP uplink frame counter (namespace "lmic", key "seqnoUp"). */
uint32_t nodeFcntLoad(void);
void     nodeFcntSave(uint32_t v);

/* ----------------------------- helpers ----------------------------- */
/* "AABB.." / "AA-BB-.." / "AA:BB" -> bytes; exact length required. */
bool   hexToBytes(const char *s, uint8_t *out, size_t len);
/* Same, any length up to max; returns length or -1. */
int    hexToBytesVar(const char *s, uint8_t *out, size_t max);
void   bytesToHex(const uint8_t *in, size_t len, char *out, char sep = 0);
/* Channel list "8-15,65" -> 72-bit mask (9 bytes). Returns channel count, -1 = error. */
int    parseChannelList(const char *s, uint8_t mask[9]);
/* Effective channel mask from subBand/ch500/chList. */
int    nodeChannelMask(const NodeConfig *c, uint8_t mask[9]);
bool   validIPv4(const char *s);

#endif

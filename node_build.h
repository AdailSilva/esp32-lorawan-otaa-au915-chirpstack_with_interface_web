/*
 * node_build.h — build options and FACTORY DEFAULTS of this node.
 *
 * Everything below is only the value used on the first boot (or after
 * "Restaurar padrão de fábrica"). After that the values live in the NVS
 * of the ESP32 and are changed from the web page (http://<ip>/ or
 * http://<nome>.local/) — no need to recompile.
 *
 * This file is the ONLY difference between the ABP and the OTAA project:
 * the other node_*.h / node_*.cpp files are identical in both.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef NODE_BUILD_H
#define NODE_BUILD_H

/* ------------------------------------------------------------------ */
/* Activation of this firmware (exactly one of them = 1)              */
/* ABP  -> MCCI LMiC 3.3.0  (LoRaWAN 1.0.x, see README)               */
/* OTAA -> MCCI LMiC 6.0.1                                            */
/* ------------------------------------------------------------------ */
#define NODE_ACTIVATION_ABP     0
#define NODE_ACTIVATION_OTAA    1

#define NODE_FW_NAME            "LoRaWAN Node OTAA"
#define NODE_FW_VERSION         "1.1"
#define NODE_LMIC_VERSION       "MCCI LMiC 6.0.1"

/* Device name: hostname (<nome>.local), Soft AP SSID and page title. */
#define DEF_NODE_NAME           "lorawan-node-otaa"

/* ---------------------------- Wi-Fi --------------------------------- */
/* 0 = Station (falls back to Soft AP), 1 = Soft AP only, 2 = Wi-Fi off */
#define DEF_WIFI_MODE           0
#define DEF_WIFI_SSID           ""
#define DEF_WIFI_PASS           ""
#define DEF_AP_PASS             "lorawan123"     /* min. 8 characters  */
#define DEF_AP_CHANNEL          6
#define DEF_STA_TIMEOUT_S       30   /* s without connection -> Soft AP */
#define DEF_BOOT_PIN            0    /* hold LOW 3 s (any time) -> Soft AP;
                                        GPIO 0 = BOOT button of the DevKit */

/* ------------------------- Web page login -------------------------- */
#define DEF_WEB_USER            "admin"
#define DEF_WEB_PASS            "lorawan"

/* --------------------- OTAA credentials (MSB) ---------------------- */
/* All values big-endian (MSB first), exactly as shown in ChirpStack.  */
#define DEF_DEVEUI              { 0xde, 0x63, 0x1e, 0xa3, 0xe5, 0xf5, 0x8d, 0xf3 }
#define DEF_JOINEUI             { 0xb2, 0xf6, 0x34, 0xa1, 0xe6, 0x2c, 0x10, 0xc7 }  /* AppEUI */
#define DEF_APPKEY              { 0x6f, 0xc5, 0x7c, 0x4c, 0xda, 0x46, 0xdd, 0xc5, \
                                  0x02, 0xbc, 0xb7, 0x85, 0x5e, 0x4e, 0x94, 0xae }

/* ABP fields are kept in the config (not used by the OTAA firmware). */
#define DEF_DEVADDR             0x00000000UL
#define DEF_NWKSKEY             { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
#define DEF_APPSKEY             { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
#define DEF_NETID               0x000000UL
#define DEF_RX1_DELAY_S         1

/* Byte order of DevEUI/JoinEUI handed to the LMiC (OTAA only).
 * 0 = as typed (MSB), exactly like the original firmware did. The LMiC
 *     sends these bytes as they are, and LoRaWAN reads EUIs LSB first, so
 *     the network server sees the EUI REVERSED (the page shows how).
 * 1 = reversed (LSB first, as MCCI documents) -> the network server sees
 *     exactly the value typed on the page / shown in ChirpStack.          */
#define DEF_EUI_REVERSE         0

/* ------------------------- Radio / channels ------------------------ */
#define DEF_SUBBAND             1      /* AU915 sub-band 1 = ch 8–15   */
#define DEF_CH500               1      /* + 500 kHz ch 64+sub-band (65) */
#define DEF_ADR                 0
#define DEF_DR                  5      /* DR5 = SF7/BW125 (AU915)       */
#define DEF_TXPOW               14     /* dBm                           */
#define DEF_LINKCHECK           0
#define DEF_CLOCK_ERR_PCT       1      /* % of MAX_CLOCK_ERROR          */
#define DEF_RX2_DR              8      /* DR8 = SF12/BW500 (AU915)      */
#define DEF_RX2_FREQ            0      /* 0 = region default 923.3 MHz  */

/* ----------------------------- Uplink ------------------------------ */
#define DEF_TX_INTERVAL_S       10
#define DEF_FPORT               101
#define DEF_CONFIRMED           0
#define DEF_PAYLOAD             "AdailSilva-IoT"

/* --------------- Remote reset command (downlink) ------------------- */
#define DEF_RST_ENABLE          1
#define DEF_RST_PORT            101
#define DEF_RST_CMD             { 0xAD, 0xA1, 0xDE, 0xAD, 0xBE, 0xEF, 0x0D, 0x0A }
#define DEF_RST_DELAY_MS        2000

/* --------------------- Hardware — ESP32 + RFM95W ------------------- */
#define DEF_PIN_SCLK            18
#define DEF_PIN_MISO            19
#define DEF_PIN_MOSI            23
#define DEF_PIN_NSS             5
#define DEF_PIN_RESET           14
#define DEF_PIN_DIO0            34
#define DEF_PIN_DIO1            35
#define DEF_PIN_DIO2            -1     /* not connected (39 = reference) */
#define DEF_SPI_FREQ            0      /* 0 = LMiC default              */
#define DEF_SERIAL_BAUD         9600
#define DEF_VERBOSE             1      /* session dump on every TX      */

#endif

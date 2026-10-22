/*
 * node_web.h — Wi-Fi + configuration web server of the LoRaWAN node.
 *
 * Runs in its own FreeRTOS task on core 0, so serving a page never delays
 * os_runloop_once() (the RX1/RX2 windows of the LMiC are timing critical).
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef NODE_WEB_H
#define NODE_WEB_H

#include <Arduino.h>

/* Region name of the LMiC build (defined in the .ino from CFG_xxx). */
extern const char *NODE_REGION;

/* Start Wi-Fi and the web server (forceAp = open the Soft AP right away). */
void nodeWebBegin(bool forceAp);

/* Open the Soft AP now (BOOT button held 3 s, or "wifi_ap" on the serial
 * console). Starts Wi-Fi/web even when the Wi-Fi was configured off. */
void nodeWebForceAp(void);

/* One line for the boot banner: how to reach the page. */
void nodeWebPrintAccess(void);

#endif

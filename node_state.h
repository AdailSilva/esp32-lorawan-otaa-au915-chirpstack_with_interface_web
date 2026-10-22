/*
 * node_state.h — data shared between the LoRaWAN loop (core 1) and the
 * web server task (core 0).
 *
 * The LMiC is NOT thread safe, so the web task never touches it:
 *   - the loop publishes a snapshot (NodeStatus) after each event;
 *   - the web task posts commands (NODE_CMD_*) that the loop executes
 *     at a safe moment (no TX/RX in progress).
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef NODE_STATE_H
#define NODE_STATE_H

#include <Arduino.h>
#include "node_config.h"

typedef struct {
    bool     joined;            /* OTAA: JOIN accepted. ABP: always true   */
    uint32_t joinTries;
    uint32_t devAddr;
    uint32_t netId;
    uint32_t fcntUp;            /* LMIC.seqnoUp (next FCnt)                */
    uint32_t fcntDn;
    uint32_t upCount;           /* EV_TXCOMPLETE since boot                */
    uint32_t ackCount;
    uint32_t dnCount;           /* downlinks with payload                  */
    uint32_t lastTxMs;          /* millis() of last EV_TXCOMPLETE, 0 = none */
    uint32_t nextTxMs;          /* millis() of next scheduled uplink, 0     */
    bool     txPending;         /* OP_TXRXPEND                             */
    uint8_t  dr;
    int8_t   txPow;
    uint32_t freq;              /* Hz of last TX                           */
    int16_t  rssi;              /* of last downlink                        */
    int8_t   snr;
    uint32_t lastDnMs;
    uint8_t  lastDnPort;
    uint8_t  lastDnLen;
    uint8_t  lastDn[32];
    char     lastEvent[24];
    uint32_t lastEventMs;
    uint32_t bootFcnt;          /* ABP: FCnt restored from NVS at boot     */
} NodeStatus;

enum {
    NODE_CMD_SEND_NOW  = 1 << 0,    /* uplink right now                    */
    NODE_CMD_RESTART   = 1 << 1,    /* re-provision ABP / redo OTAA JOIN   */
    NODE_CMD_SET_FCNT  = 1 << 2,    /* ABP: LMIC.seqnoUp = nodeCmdArg      */
    NODE_CMD_APPLY     = 1 << 3,    /* new config saved by the page        */
    NODE_CMD_REBOOT    = 1 << 4,    /* esp_restart (after saving FCnt)     */
};

/* --- status (written by the loop, read by the web task) --- */
void nodeStatusGet(NodeStatus *out);
void nodeStatusUpdate(void (*fn)(NodeStatus *s));
NodeStatus *nodeStatusRaw(void);   /* loop only, inside nodeStatusLock */
void nodeStatusLock(void);
void nodeStatusUnlock(void);

/* --- commands (posted by the web task, taken by the loop) --- */
void     nodeCmdPost(uint32_t cmd, uint32_t arg = 0);
uint32_t nodeCmdTake(uint32_t *arg);

/* --- new configuration handed from the web task to the loop --- */
void nodeCfgPost(const NodeConfig *c);
bool nodeCfgTake(NodeConfig *c);

/* --- copy of the configuration for the web task (always consistent) --- */
void nodeCfgShared(NodeConfig *out);
void nodeCfgSetShared(const NodeConfig *c);
uint32_t nodeCfgVersion(void);       /* changes on every Post/SetShared */

#endif

/*
 * node_state.cpp — locks around the data shared by loop and web task.
 * SPDX-License-Identifier: MIT
 */
#include "node_state.h"

static NodeStatus   st;
static portMUX_TYPE stMux = portMUX_INITIALIZER_UNLOCKED;

static volatile uint32_t cmdBits;
static volatile uint32_t cmdArg;
static portMUX_TYPE      cmdMux = portMUX_INITIALIZER_UNLOCKED;

/* configs are big (≈800 B): a FreeRTOS mutex instead of a spinlock */
static SemaphoreHandle_t cfgMutex;
static NodeConfig        cfgNext, cfgShared;
static bool              cfgNextValid;
static volatile uint32_t cfgVer;

static void cfgLock(void)
{
    /* first call is nodeCfgSetShared() in setup(), before the web task
     * exists, so this lazy creation never races */
    if (!cfgMutex) cfgMutex = xSemaphoreCreateMutex();
    xSemaphoreTake(cfgMutex, portMAX_DELAY);
}

static void cfgUnlock(void) { xSemaphoreGive(cfgMutex); }

void nodeStatusLock(void)   { portENTER_CRITICAL(&stMux); }
void nodeStatusUnlock(void) { portEXIT_CRITICAL(&stMux); }
NodeStatus *nodeStatusRaw(void) { return &st; }

void nodeStatusGet(NodeStatus *out)
{
    portENTER_CRITICAL(&stMux);
    *out = st;
    portEXIT_CRITICAL(&stMux);
}

void nodeStatusUpdate(void (*fn)(NodeStatus *s))
{
    portENTER_CRITICAL(&stMux);
    fn(&st);
    portEXIT_CRITICAL(&stMux);
}

void nodeCmdPost(uint32_t cmd, uint32_t arg)
{
    portENTER_CRITICAL(&cmdMux);
    cmdBits |= cmd;
    if (cmd & NODE_CMD_SET_FCNT) cmdArg = arg;
    portEXIT_CRITICAL(&cmdMux);
}

uint32_t nodeCmdTake(uint32_t *arg)
{
    uint32_t c;
    portENTER_CRITICAL(&cmdMux);
    c = cmdBits;
    cmdBits = 0;
    if (arg) *arg = cmdArg;
    portEXIT_CRITICAL(&cmdMux);
    return c;
}

void nodeCfgPost(const NodeConfig *c)
{
    cfgLock();
    cfgNext = *c;
    cfgNextValid = true;
    cfgShared = *c;
    cfgVer = cfgVer + 1;
    cfgUnlock();
    nodeCmdPost(NODE_CMD_APPLY);
}

bool nodeCfgTake(NodeConfig *c)
{
    bool ok;
    cfgLock();
    ok = cfgNextValid;
    if (ok) { *c = cfgNext; cfgNextValid = false; }
    cfgUnlock();
    return ok;
}

void nodeCfgShared(NodeConfig *out)
{
    cfgLock();
    *out = cfgShared;
    cfgUnlock();
}

void nodeCfgSetShared(const NodeConfig *c)
{
    cfgLock();
    cfgShared = *c;
    cfgVer = cfgVer + 1;
    cfgUnlock();
}

uint32_t nodeCfgVersion(void) { return cfgVer; }

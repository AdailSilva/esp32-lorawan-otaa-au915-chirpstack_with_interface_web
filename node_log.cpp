/*
 * node_log.cpp — RAM ring buffer behind the LOG object.
 * SPDX-License-Identifier: MIT
 */
#include "node_log.h"

NodeLog LOG;

static char         ring[NODE_LOG_SIZE];
static uint32_t     total;                 /* bytes ever written (offset) */
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

static void ringPut(const uint8_t *buf, size_t size)
{
    if (size > NODE_LOG_SIZE) {             /* keep only the tail */
        buf += size - NODE_LOG_SIZE;
        total += size - NODE_LOG_SIZE;
        size = NODE_LOG_SIZE;
    }
    portENTER_CRITICAL(&mux);
    size_t pos = total % NODE_LOG_SIZE;
    size_t first = NODE_LOG_SIZE - pos;
    if (first > size) first = size;
    memcpy(ring + pos, buf, first);
    memcpy(ring, buf + first, size - first);
    total += size;
    portEXIT_CRITICAL(&mux);
}

size_t NodeLog::write(uint8_t c)
{
    Serial.write(c);
    ringPut(&c, 1);
    return 1;
}

size_t NodeLog::write(const uint8_t *buf, size_t size)
{
    Serial.write(buf, size);
    ringPut(buf, size);
    return size;
}

size_t nodeLogRead(uint32_t from, char *out, size_t size, uint32_t *start, uint32_t *next)
{
    size_t n = 0;

    portENTER_CRITICAL(&mux);
    uint32_t oldest = total > NODE_LOG_SIZE ? total - NODE_LOG_SIZE : 0;
    if (from < oldest || from > total) from = oldest;
    n = total - from;
    if (n > size) n = size;
    for (size_t i = 0; i < n; i++)
        out[i] = ring[(from + i) % NODE_LOG_SIZE];
    *start = oldest;
    *next  = from + n;
    portEXIT_CRITICAL(&mux);
    return n;
}

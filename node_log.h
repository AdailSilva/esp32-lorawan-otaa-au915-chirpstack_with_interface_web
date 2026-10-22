/*
 * node_log.h — everything printed to Serial is also kept in a RAM ring
 * buffer, shown live on the web page /logs (same idea as the gateway).
 *
 * Use LOG exactly like Serial:  LOG.println(F("EV_TXSTART"));
 * SPDX-License-Identifier: MIT
 */
#ifndef NODE_LOG_H
#define NODE_LOG_H

#include <Arduino.h>

#define NODE_LOG_SIZE   (16 * 1024)

class NodeLog : public Print {
public:
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buf, size_t size) override;
    void   flush() override { Serial.flush(); }
    using Print::write;
};

extern NodeLog LOG;

/* Copy log bytes from absolute offset 'from'. *start = oldest offset still
 * in RAM, *next = offset to ask next time. Returns bytes copied. */
size_t nodeLogRead(uint32_t from, char *out, size_t size, uint32_t *start, uint32_t *next);

#endif

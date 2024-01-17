// Copyright 2024 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#pragma once

#define SOI2C_OK                    0
#define SOI2C_CONFIG                1
#define SOI2C_TERMINATOR            2
#define SOI2C_TX_BUFFER_OVERFLOW    3
#define SOI2C_RX_BUFFER_OVERFLOW    4
#define SOI2C_IO_TRANSMIT           5
#define SOI2C_IO_RECEIVE            6
#define SOI2C_IO_TIMEOUT            7
#define SOI2C_IO_BAD_SIZE_RETURNED  8
typedef int soiStatus_t;

typedef bool (*i2cTransmitFn) (uint16_t devAddr, uint8_t *buf, uint16_t buflen);
typedef bool (*i2cReceiveFn) (uint16_t devAddr, uint8_t *buf, uint16_t buflen);
typedef void (*i2cDelayFn) (uint32_t ms);
typedef struct {
    uint16_t addr;
    i2cTransmitFn tx;
    i2cReceiveFn rx;
    i2cDelayFn delay;
} soi2cContext_t;

int soi2cReset(soi2cContext_t *ctx);
int soi2cTransaction(soi2cContext_t *ctx, uint8_t *reqbuf, uint32_t reqbuflen, uint32_t reqlen, uint8_t *rspbuf, uint32_t rspbuflen, uint32_t *rsplen);


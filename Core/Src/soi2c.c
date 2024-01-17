// Copyright 2024 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include "soi2c.h"

// Reset the state4 of things by sending a \n to flush anything pending
// on the notecard from before this host was reset.  This ensures that
// our first transaction will be received cleanly.
int soi2cReset(soi2cContext_t *ctx)
{
    uint8_t resetReq[10];
    resetReq[0] = '\n';
    return soi2cTransaction(ctx, resetReq, sizeof(resetReq), 1, resetReq, 0, NULL);
}

// Perform a transaction.  Note that the transmit buffer will be turned to
// garbage, as it will be used as an I/O buffer for both TX and RX operations.
// The input reqbuf must always be terminated with \n on input.
// Note that if a "cmd" is being sent, specify NULL for the rspbuf.  If the
// caller sent a request, a non-null rspbuf address must be specified.  However,
// if the caller wants to ignore the response (and wants to avoid buffer overrun
// errors no matter how long the response), just specify 0 for the rspbuflen
// even though the address is specified. Note that the reqbuf and the rspbuf
// must not be identical unless rspbuflen is 0.
int soi2cTransaction(soi2cContext_t *ctx, uint8_t *reqbuf, uint32_t reqbuflen, uint32_t reqlen, uint8_t *rspbuf, uint32_t rspbuflen, uint32_t *rsplen)
{

    // Preset return
    if (rsplen != NULL) {
        *rsplen = 0;
    }

    // Exit if not configured
    if (ctx->addr == 0 || ctx->tx == NULL || ctx->rx == NULL || ctx->delay == NULL || reqbuflen < 3) {
        return SOI2C_CONFIG;
    }

    // Exit if request isn't newline-terminated
    if (reqlen > reqbuflen || reqlen == 0 || reqbuf[reqlen-1] != '\n') {
        return SOI2C_TERMINATOR;
    }

    // Begin by shifting the req in the reqbuf to allow space for the transmit header
    if ((reqbuflen - reqlen) < 1) {
        return SOI2C_TX_BUFFER_OVERFLOW;
    }
    memmove(&reqbuf[1], reqbuf, reqlen);

    // Loop, transmitting at most 250 bytes per chunk every 250 milliseconds
    uint32_t left = reqlen;
    while (left) {

        uint8_t chunklen = 250;
        if (left < chunklen) {
            chunklen = (uint8_t) left;
        }

        reqbuf[0] = chunklen;
        if (!ctx->tx(ctx->addr, reqbuf, chunklen)) {
            return SOI2C_IO_TRANSMIT;
        }

        left -= chunklen;
        memmove(&reqbuf[1], &reqbuf[1+chunklen], left);

        ctx->delay(250);

    }

    // Exit if a "cmd" was sent and no response is expected.  
    if (rspbuf == NULL) {
        return SOI2C_OK;
    }

    // Go into a receive loop, using the TX buffer as an IOBUF.  
    uint32_t rspbufleft = rspbuflen;
    uint32_t msLeftToWait = 5000;
    uint8_t chunklen = 0;
    while (true) {

        // Issue special write transaction that is a 'read will come next' transaction
        reqbuf[0] = 0;
        reqbuf[1] = chunklen;
        if (!ctx->tx(ctx->addr, reqbuf, 2)) {
            return SOI2C_IO_TRANSMIT;
        }

        // Receive the chunk of data
        if (!ctx->rx(ctx->addr, reqbuf, chunklen+2)) {
            return SOI2C_IO_TRANSMIT;
        }

        // Verify size
        uint8_t availableBytes = reqbuf[0];
        uint8_t returnedBytes = reqbuf[1];
        if (returnedBytes != chunklen) {
            return SOI2C_IO_BAD_SIZE_RETURNED;
        }

        // Only move bytes into the response buffer if a nonzero length specified,
        // else just flush it.
        if (rspbuflen != 0) {
            if (chunklen > rspbufleft) {
                return SOI2C_RX_BUFFER_OVERFLOW;
            }
            memcpy(&rspbuf[reqbuflen-rspbufleft], &reqbuf[2], chunklen);
            rspbufleft -= chunklen;
            if (rsplen != NULL) {
                *rsplen = reqbuflen-rspbufleft;
            }
        }

        // Look at what has just been received for a terminator, and stop if found
        bool receivedNewline = (reqbuf[(2+chunklen)-1] == '\n');

        // Maximize chunklen to fit in our I/O buffer WITH our 2-byte header
        if (availableBytes < (reqbuflen - 2)) {
            chunklen = availableBytes;
        } else {
            chunklen = reqbuflen - 2;
        }

        // If more to receive, do it
        if (chunklen > 0) {
            continue;
        }

        // If there's nothing available AND we've received a newline, we're done
        if (receivedNewline) {
            break;
        }

        // If no time left to process the transaction, give up
        uint32_t pollMs = 50;
        if (msLeftToWait < pollMs) {
            return SOI2C_IO_TIMEOUT;
        }

        // Delay, and subtract from what's left
        ctx->delay(pollMs);
        msLeftToWait -= pollMs;

    }

    // Done
    return SOI2C_OK;

}

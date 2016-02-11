/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * https://github.com/m8rge/cwebsocket/blob/master/lib/websocket.c
 */
#include <assert.h>
#include "websocket.h"

static char rn[] = "\r\n";

void nullHandshake( handshake *hs)
{
    hs->frameType = WS_EMPTY_FRAME;
}

void freeHandshake(  handshake *hs)
{
    if (hs->host) {
        free(hs->host);
    }
    if (hs->origin) {
        free(hs->origin);
    }
    if (hs->resource) {
        free(hs->resource);
    }
    if (hs->key) {
        free(hs->key);
    }
    nullHandshake(hs);
}

static char* getUptoLinefeed(const char *startFrom)
{
    char *writeTo = NULL;
    uint8_t newLength = strstr_P(startFrom, rn) - startFrom;
    assert(newLength);
    writeTo = (char *)malloc(newLength+1); //+1 for '\x00'
    assert(writeTo);
    memcpy(writeTo, startFrom, newLength);
    writeTo[ newLength ] = 0;
    return writeTo;
}


//ÎÕÊÖ
enum wsFrameType wsParseHandshake(const uint8_t *inputFrame, size_t inputLength, handshake *hs)
{
	
}


void wsGetHandshakeAnswer(const handshake *hs, uint8_t *outFrame, 
                          size_t *outLength , char* ver )
{
    assert(outFrame && *outLength);
    assert(hs->frameType == WS_OPENING_FRAME);
    assert(hs && hs->key);

    char responseKey[128] = {0};
    uint8_t length = strlen(hs->key)+strlen_P(secret);
    memcpy(responseKey, hs->key, strlen(hs->key));

    memcpy_P(&(responseKey[strlen(hs->key)]), secret, strlen_P(secret));
    unsigned char shaHash[SHA1_DIGEST_LENGTH];
    memset(shaHash, 0, sizeof(shaHash));
 
    sha1( responseKey , length , shaHash );
    size_t base64Length = base64_encode( responseKey, length, shaHash, SHA1_DIGEST_LENGTH ); 
    responseKey[base64Length] = '\0';
	
    int written = sprintf_P((char *)outFrame,
                            PSTR("HTTP/1.1 101 Switching Protocols\r\n"
                                 "%s%s\r\n"
                                 "%s%s\r\n"
                                 "Sec-WebSocket-Version: %s\r\n"
                                 "Sec-WebSocket-Accept: %s\r\n\r\n"),
                            upgradeField,
                            websocket,
                            connectionField,
                            upgrade2,
			    ver,
                            responseKey);

    // if assert fail, that means, that we corrupt memory
    assert(written <= *outLength);
    *outLength = written;
}

void wsMakeFrame(const uint8_t *data, size_t dataLength,
                 uint8_t *outFrame, size_t *outLength, enum wsFrameType frameType)
{
    assert(outFrame && *outLength);
    assert(frameType < 0x10);
    if (dataLength > 0)
        assert(data);
	
    outFrame[0] = 0x80 | frameType;
    
    if (dataLength <= 125) {
        outFrame[1] = dataLength;
        *outLength = 2;
    } else if (dataLength <= 0xFFFF) {
        outFrame[1] = 126;
        uint16_t payloadLength16b = htons(dataLength);
        memcpy(&outFrame[2], &payloadLength16b, 2);
        *outLength = 4;
    } else {
        assert(dataLength <= 0xFFFF);
        
        /* implementation for 64bit systems
        outFrame[1] = 127;
        dataLength = htonll(dataLength);
        memcpy(&outFrame[2], &dataLength, 8);
        *outLength = 10;
        */
    }
    memcpy(&outFrame[*outLength], data, dataLength);
    *outLength+= dataLength;
}

static size_t getPayloadLength(const uint8_t *inputFrame, size_t inputLength,
                               uint8_t *payloadFieldExtraBytes, enum wsFrameType *frameType) 
{
    size_t payloadLength = inputFrame[1] & 0x7F;
    *payloadFieldExtraBytes = 0;
    if ((payloadLength == 0x7E && inputLength < 4) || (payloadLength == 0x7F && inputLength < 10)) {
        *frameType = WS_INCOMPLETE_FRAME;
        return 0;
    }
    if (payloadLength == 0x7F && (inputFrame[3] & 0x80) != 0x0) {
        *frameType = WS_ERROR_FRAME;
        return 0;
    }

    if (payloadLength == 0x7E) {
        uint16_t payloadLength16b = 0;
        *payloadFieldExtraBytes = 2;
        memcpy(&payloadLength16b, &inputFrame[2], *payloadFieldExtraBytes);
        payloadLength = ntohs(payloadLength16b);
    } else if (payloadLength == 0x7F) {
        *frameType = WS_ERROR_FRAME;
        return 0;
        
        /* // implementation for 64bit systems
        uint64_t payloadLength64b = 0;
        *payloadFieldExtraBytes = 8;
        memcpy(&payloadLength64b, &inputFrame[2], *payloadFieldExtraBytes);
        if (payloadLength64b > SIZE_MAX) {
            *frameType = WS_ERROR_FRAME;
            return 0;
        }
        payloadLength = (size_t)ntohll(payloadLength64b);
        */
    }

    return payloadLength;
}

enum wsFrameType wsParseInputFrame(uint8_t *inputFrame, size_t inputLength,
                                   uint8_t **dataPtr, size_t *dataLength)
{
    assert(inputFrame && inputLength);

    if (inputLength < 2)
        return WS_INCOMPLETE_FRAME;
    if ((inputFrame[0] & 0x70) != 0x0) // checks extensions off
        return WS_ERROR_FRAME;
    if ((inputFrame[0] & 0x80) != 0x80) // we haven't continuation frames support
        return WS_ERROR_FRAME; // so, fin flag must be set
    if ((inputFrame[1] & 0x80) != 0x80) // checks masking bit
        return WS_ERROR_FRAME;

    uint8_t opcode = inputFrame[0] & 0x0F;
    if (opcode == WS_TEXT_FRAME ||
            opcode == WS_BINARY_FRAME ||
            opcode == WS_CLOSING_FRAME ||
            opcode == WS_PING_FRAME ||
            opcode == WS_PONG_FRAME
    ){
        enum wsFrameType frameType = opcode;

        uint8_t payloadFieldExtraBytes = 0;
        size_t payloadLength = getPayloadLength(inputFrame, inputLength,
                                                &payloadFieldExtraBytes, &frameType);
        if (payloadLength > 0) {
            if (payloadLength + 6 + payloadFieldExtraBytes > inputLength) // 4-maskingKey, 2-header
                return WS_INCOMPLETE_FRAME;
            uint8_t *maskingKey = &inputFrame[2 + payloadFieldExtraBytes];

            assert(payloadLength == inputLength - 6 - payloadFieldExtraBytes);

            *dataPtr = &inputFrame[2 + payloadFieldExtraBytes + 4];
            *dataLength = payloadLength;
		
            size_t i;
            for (i = 0; i < *dataLength; i++) {
                (*dataPtr)[i] = (*dataPtr)[i] ^ maskingKey[i%4];
            }
        }
        return frameType;
    }
    return WS_ERROR_FRAME;
}

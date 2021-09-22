#ifndef _ESPNOW_HMAC_H
#define _ESPNOW_HMAC_H

#include <stddef.h>
#include <stdint.h>

#include "espnow-types.h"

void fillHmac(espnow_msg_base *base, size_t len);
bool verifyHmac(espnow_msg_base *base, size_t len);
bool validate_and_copy(void *target, int targetLen, const uint8_t *data, int dataLen);
char* getLastHmacError();

#endif

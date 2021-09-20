#ifndef _ESPNOW_HMAC_H
#define _ESPNOW_HMAC_H

#include <stddef.h>
#include <stdint.h>

#include "espnow-types.h"

void fillHmac(espnow_msg_base *base, size_t len);
bool verifyHmac(espnow_msg_base *base, size_t len);

#endif

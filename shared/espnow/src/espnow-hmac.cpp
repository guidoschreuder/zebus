#include "espnow-hmac.h"
#include "espnow-config.h"

#include <mbedtls/md.h>
#include <string.h>
#include <stdio.h>

static char lastError[48] = {0};
static uint64_t last_nonce = 0;

void calcHmac(espnow_msg_base *base, size_t len, unsigned char* hmac) {
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

  const size_t payloadOffset = sizeof(espnow_msg_base) - sizeof(base->nonce);
  const size_t payloadLength = len - payloadOffset;
  const size_t keyLength = strlen(ESPNOW_HMAC_KEY);

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char *) ESPNOW_HMAC_KEY, keyLength);
  mbedtls_md_hmac_update(&ctx, payloadOffset + (const unsigned char *) base, payloadLength);
  mbedtls_md_hmac_finish(&ctx, (unsigned char *) hmac);
  mbedtls_md_free(&ctx);
}

void fillHmac(espnow_msg_base *base, size_t len) {
  base->nonce = ++last_nonce;
  calcHmac(base, len, (unsigned char *) base->payloadHmac);
}

bool verifyHmac(espnow_msg_base *base, size_t len) {
  uint8_t hmac[32];
  calcHmac(base, len,  (unsigned char *) hmac);

  return (memcmp(hmac, base->payloadHmac, sizeof(hmac)) == 0);
}

bool validate_and_copy(void *target, int targetLen, const uint8_t *data, int dataLen) {
  if (dataLen != targetLen) {
    sprintf(lastError, "Invalid packet length, expected %d, got %d", targetLen, dataLen);
    return false;
  }
  memcpy(target, data, dataLen);
  if (((espnow_msg_base *) target)->nonce == 0) {
    sprintf(lastError, "No nonce in message");
  }
  if (!verifyHmac((espnow_msg_base *) target, targetLen)) {
    sprintf(lastError, "Invalid HMAC");
    return false;
  }
  return true;
}

char* getLastHmacError() {
  return lastError;
}

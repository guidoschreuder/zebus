#include "espnow-hmac.h"
#include "espnow-config.h"

#include <mbedtls/md.h>
#include <string.h>
// TODO: the "printf" commands totally do not work across ESPIDF and ARDUINO, but code is functional
#include <stdio.h>

void calcHmac(espnow_msg_base *base, size_t len, unsigned char* hmac) {
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

  const size_t paylaodOffset = sizeof(espnow_msg_base);
  const size_t payloadLength = len - paylaodOffset;
  const size_t keyLength = strlen(ESPNOW_HMAC_KEY);

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char *) ESPNOW_HMAC_KEY, keyLength);
  mbedtls_md_hmac_update(&ctx, paylaodOffset + (const unsigned char *) base, payloadLength);
  mbedtls_md_hmac_finish(&ctx, (unsigned char *) hmac);
  mbedtls_md_free(&ctx);
}

void fillHmac(espnow_msg_base *base, size_t len) {
  calcHmac(base, len,  (unsigned char *) base->payloadHmac);
}

bool verifyHmac(espnow_msg_base *base, size_t len) {
  uint8_t hmac[32];
  calcHmac(base, len,  (unsigned char *) hmac);

  return (memcmp(hmac, base->payloadHmac, sizeof(hmac)) == 0);
}

bool validate_and_copy(void *target, int targetLen, const uint8_t *data, int dataLen) {
  if (dataLen != targetLen) {
    printf("ERROR: Invalid packet length, expected %d, got %d\n", targetLen, dataLen);
    return false;
  }
  memcpy(target, data, dataLen);
  if (!verifyHmac((espnow_msg_base *) target, targetLen)) {
    printf("ERROR: Invalid HMAC\n");
    return false;
  }
  return true;
}

#ifdef UNIT_TEST

#include <unity.h>
#include <stdio.h>
#include <ebus.h>

#define P99_PROTECT(...) __VA_ARGS__ 
#define TEST_CRC8(auc_bytestream_param) do {  \
    unsigned char auc_bytestream[] = auc_bytestream_param; \
    TEST_ASSERT_EQUAL_CHAR(auc_bytestream[sizeof(auc_bytestream) - 1], crc8_array(auc_bytestream, sizeof(auc_bytestream) - 1)); \
} while(0);

void test_crc() {
    TEST_CRC8(P99_PROTECT({0x03, 0x64, 0xb5, 0x12, 0x02, 0x02, 0x00, 0x66}))
    TEST_CRC8(P99_PROTECT({0x03, 0x05, 0xb5, 0x12, 0x02, 0x03, 0x00, 0xc6}))
    TEST_CRC8(P99_PROTECT({0x31, 0x08, 0xb5, 0x09, 0x03, 0x0d, 0xa9, 0x01, 0x00, 0x0e}))
    TEST_CRC8(P99_PROTECT({0x00, 0x06, 0x23, 0x08, 0x64, 0x18, 0x64, 0x18, 0x93}))
}
int main( int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_crc);

    UNITY_END();
}

#endif

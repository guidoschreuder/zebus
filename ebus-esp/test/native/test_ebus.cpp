#ifdef UNIT_TEST

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#include <ebus.h>
#include <main.h>
#include <unity.h>

// #include <cstdio>  //debug
#include <cstring>

#define P99_PROTECT(...) __VA_ARGS__
#define TEST_CRC8(auc_bytestream_param)                                                                                         \
  do {                                                                                                                          \
    unsigned char auc_bytestream[] = auc_bytestream_param;                                                                      \
    TEST_ASSERT_EQUAL_HEX8(auc_bytestream[sizeof(auc_bytestream) - 1], crc8_array(auc_bytestream, sizeof(auc_bytestream) - 1)); \
  } while (0);

#define SEND(auc_bytestream_param)                         \
  do {                                                     \
    unsigned char auc_bytestream[] = auc_bytestream_param; \
    int i;                                                 \
    for (i = 0; i < sizeof(auc_bytestream); i++) {         \
      process_received(auc_bytestream[i]);                 \
    }                                                      \
  } while (0);

void test_crc() {
  TEST_CRC8(P99_PROTECT({0x03, 0x64, 0xb5, 0x12, 0x02, 0x02, 0x00, 0x66}))
  TEST_CRC8(P99_PROTECT({0x03, 0x05, 0xb5, 0x12, 0x02, 0x03, 0x00, 0xc6}))
  TEST_CRC8(P99_PROTECT({0x31, 0x08, 0xb5, 0x09, 0x03, 0x0d, 0xa9, 0x01, 0x00, 0x0e}))
  TEST_CRC8(P99_PROTECT({0x00, 0x06, 0x23, 0x08, 0x64, 0x18, 0x64, 0x18, 0x93}))
  TEST_CRC8(P99_PROTECT({0x03, 0x64, 0xb5, 0x12, 0x02, 0xA9, 0x00, 0x45, 0x4D}))
  TEST_CRC8(P99_PROTECT({0x03, 0x00, 0xb5, 0x12, 0x00, 0x62}))
  TEST_CRC8(P99_PROTECT({0x01, 0xDD, 0x46}))
}

void test_is_master() {
  TEST_ASSERT_TRUE(is_master(0b00000000));
  TEST_ASSERT_TRUE(is_master(0b11111111));
  TEST_ASSERT_TRUE(is_master(0b00011111));
  TEST_ASSERT_FALSE(is_master(EBUS_SLAVE_ADDRESS(0)));
  TEST_ASSERT_FALSE(is_master(0b01011111));
}

// TODO: REMOVE
// temporary method, Ebus ISR should properly switch to a new active telegram
// on completion and put the old one on a queue/buffer/etc
void prepare_telegram() {
  EbusTelegram telegram;
  memset(telegram.requestBuffer, SYN, sizeof telegram.requestBuffer);
  g_activeTelegram = telegram;
  process_received(SYN);
}

void test_getter() {
  prepare_telegram();
  SEND(P99_PROTECT({0x00, 0x01, 0x02, 0x03}));
  TEST_ASSERT_EQUAL_CHAR(0x00, g_activeTelegram.getQQ());
  TEST_ASSERT_EQUAL_CHAR(0x01, g_activeTelegram.getZZ());
  TEST_ASSERT_EQUAL_CHAR(0x02, g_activeTelegram.getPB());
  TEST_ASSERT_EQUAL_CHAR(0x03, g_activeTelegram.getSB());
}

void test_telegram_completion() {
  prepare_telegram();
  SEND(P99_PROTECT({0x03, 0x64, 0xb5, 0x12}));
  TEST_ASSERT_FALSE(g_activeTelegram.isRequestComplete());
  SEND(P99_PROTECT({0x02, 0x02, 0x00}));
  TEST_ASSERT_FALSE(g_activeTelegram.isRequestComplete());
  process_received(0x66);
  TEST_ASSERT_TRUE(g_activeTelegram.isRequestComplete());
  TEST_ASSERT_EQUAL_HEX8(g_activeTelegram.getRequestCRC(), g_activeTelegram.requestRollingCRC);
}

void test_telegram_with_escape() {
  prepare_telegram();
  SEND(P99_PROTECT({0x03, 0x64, 0xb5, 0x12, 0x02, 0xA9, 0x00}));
  TEST_ASSERT_FALSE(g_activeTelegram.isRequestComplete());
  process_received(0x45);
  TEST_ASSERT_FALSE(g_activeTelegram.isRequestComplete());
  process_received(0x4D);
  TEST_ASSERT_TRUE(g_activeTelegram.isRequestComplete());
  TEST_ASSERT_TRUE(g_activeTelegram.isRequestValid());
  TEST_ASSERT_TRUE(g_activeTelegram.response_expected());
}

void test_telegram_master_master_completed_after_ack() {
  prepare_telegram();
  SEND(P99_PROTECT({0x03, 0x00, 0xb5, 0x12, 0x00, 0x62}));
  TEST_ASSERT_TRUE(g_activeTelegram.isRequestComplete());
  TEST_ASSERT_EQUAL_HEX8(g_activeTelegram.getRequestCRC(), g_activeTelegram.requestRollingCRC);
  TEST_ASSERT_TRUE(g_activeTelegram.isRequestValid());
  TEST_ASSERT_FALSE(g_activeTelegram.response_expected());
  process_received(ACK);
  TEST_ASSERT_EQUAL_INT(EbusTelegram::EbusState::endCompleted, g_activeTelegram.state);
}

void test_telegram_master_slave_completed_after_response_ack() {
  prepare_telegram();
  SEND(P99_PROTECT({0x03, 0x02, 0xb5, 0x12, 0x00, 0xDE}));

  TEST_ASSERT_TRUE(g_activeTelegram.isRequestComplete());
  TEST_ASSERT_EQUAL_HEX8(g_activeTelegram.getRequestCRC(), g_activeTelegram.requestRollingCRC);
  TEST_ASSERT_TRUE(g_activeTelegram.isRequestValid());
  TEST_ASSERT_TRUE(g_activeTelegram.response_expected());
  process_received(ACK);

  SEND(P99_PROTECT({0x01, 0xDD, 0x46}));
  TEST_ASSERT_EQUAL_HEX8(g_activeTelegram.getResponseCRC(), g_activeTelegram.responseRollingCRC);
  TEST_ASSERT_TRUE(g_activeTelegram.isResponseValid());
  TEST_ASSERT_EQUAL_INT(EbusTelegram::EbusState::waitForResponseAck, g_activeTelegram.state);

  process_received(ACK);
  TEST_ASSERT_EQUAL_INT(EbusTelegram::EbusState::endCompleted, g_activeTelegram.state);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_crc);
  RUN_TEST(test_is_master);
  RUN_TEST(test_getter);
  RUN_TEST(test_telegram_completion);
  RUN_TEST(test_telegram_with_escape);
  RUN_TEST(test_telegram_master_master_completed_after_ack);
  RUN_TEST(test_telegram_master_slave_completed_after_response_ack);

  UNITY_END();
}

#endif

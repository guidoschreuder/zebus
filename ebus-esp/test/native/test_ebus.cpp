#ifdef UNIT_TEST

#include "mock-queue.h"

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#include <ebus.h>
#include <main.h>
#include <unity.h>

#include <cstdio>  //debug
#include <cstring>

#define P99_PROTECT(...) __VA_ARGS__
#define TEST_CRC8(auc_bytestream_param)                                                                                                         \
  do {                                                                                                                                          \
    unsigned char auc_bytestream[] = auc_bytestream_param;                                                                                      \
    TEST_ASSERT_EQUAL_HEX8(auc_bytestream[sizeof(auc_bytestream) - 1], Ebus::Ebus::Elf::crc8Array(auc_bytestream, sizeof(auc_bytestream) - 1)); \
  } while (0);

#define SEND(auc_bytestream_param)                         \
  do {                                                     \
    unsigned char auc_bytestream[] = auc_bytestream_param; \
    int i;                                                 \
    for (i = 0; i < sizeof(auc_bytestream); i++) {         \
      ebus.processReceivedChar(auc_bytestream[i]);         \
    }                                                      \
  } while (0);

Queue telegramHistoryMockQueue(5, Queue::OnFull::removeOldest);
Ebus::Ebus ebus = Ebus::Ebus(0);

void test_crc() {
  TEST_CRC8(P99_PROTECT({0x03, 0x64, 0xb5, 0x12, 0x02, 0x02, 0x00, 0x66}))
  TEST_CRC8(P99_PROTECT({0x03, 0x05, 0xb5, 0x12, 0x02, 0x03, 0x00, 0xc6}))
  TEST_CRC8(P99_PROTECT({0x31, 0x08, 0xb5, 0x09, 0x03, 0x0d, 0xa9, 0x01, 0x00, 0x0e}))
  TEST_CRC8(P99_PROTECT({0x00, 0x06, 0x23, 0x08, 0x64, 0x18, 0x64, 0x18, 0x93}))
  TEST_CRC8(P99_PROTECT({0x03, 0x64, 0xb5, 0x12, 0x02, 0xA9, 0x00, 0x45, 0x4D}))
  TEST_CRC8(P99_PROTECT({0x03, 0x00, 0xb5, 0x12, 0x00, 0x62}))
  TEST_CRC8(P99_PROTECT({0x01, 0xDD, 0x46}))
  TEST_CRC8(P99_PROTECT({0x01, 0x05, 0x07, 0x04, 0x00, 0x91}))
  TEST_CRC8(P99_PROTECT({0x0A, 0x1D, 0x47, 0x75, 0x69, 0x64, 0x6F, 0x01, 0x02, 0x03, 0x04, 0xE3}))
}

void test_is_master() {
  TEST_ASSERT_TRUE(Ebus::Ebus::Elf::isMaster(0b00000000));
  TEST_ASSERT_TRUE(Ebus::Ebus::Elf::isMaster(0b11111111));
  TEST_ASSERT_TRUE(Ebus::Ebus::Elf::isMaster(0b00011111));
  TEST_ASSERT_FALSE(Ebus::Ebus::Elf::isMaster(EBUS_SLAVE_ADDRESS(0)));
  TEST_ASSERT_FALSE(Ebus::Ebus::Elf::isMaster(0b01011111));
}

void uartSend(const char* src, int16_t size) {
  int i;
  for (i = 0; i < size; i++) {
    ebus.processReceivedChar((uint8_t) src[i]);
  }
}

void ebusQueue(Ebus::Telegram telegram) {
  Ebus::Telegram* copy = (Ebus::Telegram*)malloc(sizeof(Ebus::Telegram));
  memcpy(copy, &telegram, sizeof(Ebus::Telegram));
  telegramHistoryMockQueue.enqueue(copy);
}

void setupEbus() {
  ebus = Ebus::Ebus(0);
  ebus.setUartSendFunction(uartSend);
  ebus.setQueueHistoricFunction(ebusQueue);
  ebus.processReceivedChar(SYN);
}

void discard_telegram() {
  ebus.processReceivedChar(-1);
  ebus.processReceivedChar(SYN);
}

void test_getter() {
  setupEbus();
  SEND(P99_PROTECT({0x00, 0x01, 0x02, 0x03}));
  TEST_ASSERT_EQUAL_CHAR(0x00, ebus.getActiveTelegram().getQQ());
  TEST_ASSERT_EQUAL_CHAR(0x01, ebus.getActiveTelegram().getZZ());
  TEST_ASSERT_EQUAL_CHAR(0x02, ebus.getActiveTelegram().getPB());
  TEST_ASSERT_EQUAL_CHAR(0x03, ebus.getActiveTelegram().getSB());
  discard_telegram();
}

void test_telegram_completion() {
  setupEbus();
  SEND(P99_PROTECT({0x03, 0x64, 0xb5, 0x12}));
  TEST_ASSERT_FALSE(ebus.getActiveTelegram().isRequestComplete());
  SEND(P99_PROTECT({0x02, 0x02, 0x00}));
  TEST_ASSERT_FALSE(ebus.getActiveTelegram().isRequestComplete());
  ebus.processReceivedChar(0x66);
  TEST_ASSERT_TRUE(ebus.getActiveTelegram().isRequestValid());
  discard_telegram();
}

void test_telegram_with_escape() {
  setupEbus();
  SEND(P99_PROTECT({0x03, 0x64, 0xb5, 0x12, 0x02, 0xA9, 0x00}));
  TEST_ASSERT_FALSE(ebus.getActiveTelegram().isRequestComplete());
  ebus.processReceivedChar(0x45);
  TEST_ASSERT_FALSE(ebus.getActiveTelegram().isRequestComplete());
  ebus.processReceivedChar(0x4D);
  TEST_ASSERT_TRUE(ebus.getActiveTelegram().isRequestComplete());
  TEST_ASSERT_TRUE(ebus.getActiveTelegram().isRequestValid());
  TEST_ASSERT_TRUE(ebus.getActiveTelegram().isResponseExpected());
  discard_telegram();
}

void test_telegram_master_master_completed_after_ack() {
  setupEbus();
  SEND(P99_PROTECT({0x03, 0x00, 0xb5, 0x12, 0x00, 0x62}));
  TEST_ASSERT_TRUE(ebus.getActiveTelegram().isRequestComplete());
  TEST_ASSERT_TRUE(ebus.getActiveTelegram().isRequestValid());
  TEST_ASSERT_FALSE(ebus.getActiveTelegram().isResponseExpected());
  ebus.processReceivedChar(ACK);
  TEST_ASSERT_EQUAL_INT(Ebus::TelegramState::endCompleted, ebus.getActiveTelegram().getState());
  discard_telegram();
}

void test_telegram_master_slave_completed_after_response_ack() {
  setupEbus();
  SEND(P99_PROTECT({0x03, 0x02, 0xb5, 0x12, 0x00, 0xDE}));

  TEST_ASSERT_TRUE(ebus.getActiveTelegram().isRequestComplete());
  TEST_ASSERT_TRUE(ebus.getActiveTelegram().isRequestValid());
  TEST_ASSERT_TRUE(ebus.getActiveTelegram().isResponseExpected());
  ebus.processReceivedChar(ACK);

  SEND(P99_PROTECT({0x01, 0xDD, 0x46}));
  TEST_ASSERT_TRUE(ebus.getActiveTelegram().isResponseValid());
  TEST_ASSERT_EQUAL_INT(Ebus::TelegramState::waitForResponseAck, ebus.getActiveTelegram().getState());

  ebus.processReceivedChar(ACK);

  TEST_ASSERT_EQUAL_INT(Ebus::TelegramState::endCompleted, ebus.getActiveTelegram().getState());
  discard_telegram();
}

void test_telegram_identity_response() {
  setupEbus();
  SEND(P99_PROTECT({0x01, 0x05, 0x07, 0x04, 0x00, 0x91}))

  ebus.processReceivedChar(ACK);
  ebus.processReceivedChar(SYN);

  Ebus::Telegram* telegramDequeued;
  Ebus::Telegram* telegram;
  while ((telegramDequeued = (Ebus::Telegram*)telegramHistoryMockQueue.dequeue()) != NULL) {
    telegram = telegramDequeued;
  }

  TEST_ASSERT_TRUE(telegram->isRequestValid());
  TEST_ASSERT_TRUE(telegram->isResponseValid());

  discard_telegram();
}

// not really a test (yet) but hey ;)
void test_multiple() {
  int i;
  for (i = 0; i < 10; i++) {
    ebus.processReceivedChar(SYN);
    ebus.processReceivedChar(i);
    ebus.processReceivedChar(SYN);
    discard_telegram();
  }
  Ebus::Telegram* telegram;
  while ((telegram = (Ebus::Telegram*)telegramHistoryMockQueue.dequeue()) != NULL) {
    printf("QQ: %02x\n", telegram->getQQ());
  }
}

int main(int argc, char** argv) {
  UNITY_BEGIN();

  RUN_TEST(test_crc);
  RUN_TEST(test_is_master);
  RUN_TEST(test_getter);
  RUN_TEST(test_telegram_completion);
  RUN_TEST(test_telegram_with_escape);
  RUN_TEST(test_telegram_master_master_completed_after_ack);
  RUN_TEST(test_telegram_master_slave_completed_after_response_ack);
  RUN_TEST(test_telegram_identity_response);
  RUN_TEST(test_multiple);

  UNITY_END();
}

#endif

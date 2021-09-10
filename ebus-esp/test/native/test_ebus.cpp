#ifdef UNIT_TEST

#include "mock-queue.h"

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#include <Ebus.h>
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
    for (int i = 0; i < sizeof(auc_bytestream); i++) {     \
      ebus.processReceivedChar(auc_bytestream[i]);         \
    }                                                      \
  } while (0);

Queue telegramHistoryMockQueue(5, Queue::OnFull::removeOldest);
Queue telegramSendMockQueue(5, Queue::OnFull::ignore);

ebus_config_t ebus_config = ebus_config_t {
  .master_address = 0,
  .max_tries = 1,
  .max_lock_counter = 2,
};

uint8_t fixedResponse(Ebus::Telegram telegram, uint8_t *buffer) {
  buffer[0] = 'f';
  buffer[1] = 'i';
  buffer[2] = 'x';
  return 3;
}

Ebus::Ebus ebus = Ebus::Ebus(ebus_config);

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
  for (int i = 0; i < size; i++) {
    ebus.processReceivedChar((uint8_t)src[i]);
  }
}

void ebusQueue(Ebus::Telegram telegram) {
  Ebus::Telegram* copy = (Ebus::Telegram*)malloc(sizeof(Ebus::Telegram));
  memcpy(copy, &telegram, sizeof(Ebus::Telegram));
  telegramHistoryMockQueue.enqueue(copy);
}

bool ebusDequeueCommand(void* command) {
  Ebus::SendCommand* commandDequeue;
  if (commandDequeue = (Ebus::SendCommand*)telegramSendMockQueue.dequeue()) {
    memcpy(command, commandDequeue, sizeof(Ebus::SendCommand));
    return true;
  }
  return false;
}

void setupEbus() {
  ebus = Ebus::Ebus(ebus_config);
  ebus.setUartSendFunction(uartSend);
  ebus.setQueueHistoricFunction(ebusQueue);
  ebus.setDeueueCommandFunction(ebusDequeueCommand);
  ebus.processReceivedChar(EBUS_SYN);
  ebus.addSendResponseHandler(fixedResponse);
}

void test_getter() {
  setupEbus();
  SEND(P99_PROTECT({0x00, 0x01, 0x02, 0x03}));
  TEST_ASSERT_EQUAL_CHAR(0x00, ebus.getReceivingTelegram().getQQ());
  TEST_ASSERT_EQUAL_CHAR(0x01, ebus.getReceivingTelegram().getZZ());
  TEST_ASSERT_EQUAL_CHAR(0x02, ebus.getReceivingTelegram().getPB());
  TEST_ASSERT_EQUAL_CHAR(0x03, ebus.getReceivingTelegram().getSB());
}

void test_telegram_completion() {
  setupEbus();
  SEND(P99_PROTECT({0x03, 0x64, 0xb5, 0x12}));
  TEST_ASSERT_FALSE(ebus.getReceivingTelegram().isRequestComplete());
  SEND(P99_PROTECT({0x02, 0x02, 0x00}));
  TEST_ASSERT_FALSE(ebus.getReceivingTelegram().isRequestComplete());
  ebus.processReceivedChar(0x66);
  TEST_ASSERT_TRUE(ebus.getReceivingTelegram().isRequestValid());
}

void test_telegram_with_escape() {
  setupEbus();
  SEND(P99_PROTECT({0x03, 0x64, 0xb5, 0x12, 0x02, 0xA9, 0x00}));
  TEST_ASSERT_FALSE(ebus.getReceivingTelegram().isRequestComplete());
  ebus.processReceivedChar(0x45);
  TEST_ASSERT_FALSE(ebus.getReceivingTelegram().isRequestComplete());
  ebus.processReceivedChar(0x4D);
  TEST_ASSERT_TRUE(ebus.getReceivingTelegram().isRequestComplete());
  TEST_ASSERT_TRUE(ebus.getReceivingTelegram().isRequestValid());
  TEST_ASSERT_TRUE(ebus.getReceivingTelegram().isResponseExpected());
}

void test_telegram_master_master_completed_after_ack() {
  setupEbus();
  SEND(P99_PROTECT({0x03, 0x00, 0xb5, 0x12, 0x00, 0x62}));
  TEST_ASSERT_TRUE(ebus.getReceivingTelegram().isRequestComplete());
  TEST_ASSERT_TRUE(ebus.getReceivingTelegram().isRequestValid());
  TEST_ASSERT_FALSE(ebus.getReceivingTelegram().isResponseExpected());
  ebus.processReceivedChar(EBUS_ACK);
  TEST_ASSERT_EQUAL_INT(Ebus::TelegramState::endCompleted, ebus.getReceivingTelegram().getState());
}

void test_telegram_master_slave_completed_after_response_ack() {
  setupEbus();
  SEND(P99_PROTECT({0x03, 0x02, 0xb5, 0x12, 0x00, 0xDE}));

  TEST_ASSERT_TRUE(ebus.getReceivingTelegram().isRequestComplete());
  TEST_ASSERT_TRUE(ebus.getReceivingTelegram().isRequestValid());
  TEST_ASSERT_TRUE(ebus.getReceivingTelegram().isResponseExpected());
  ebus.processReceivedChar(EBUS_ACK);

  SEND(P99_PROTECT({0x01, 0xDD, 0x46}));
  TEST_ASSERT_TRUE(ebus.getReceivingTelegram().isResponseValid());
  TEST_ASSERT_EQUAL_INT(Ebus::TelegramState::waitForResponseAck, ebus.getReceivingTelegram().getState());

  ebus.processReceivedChar(EBUS_ACK);

  TEST_ASSERT_EQUAL_INT(Ebus::TelegramState::endCompleted, ebus.getReceivingTelegram().getState());
}

void test_telegram_identity_response() {
  setupEbus();
  SEND(P99_PROTECT({0x01, 0x05, 0x07, 0x04, 0x00, 0x91}))

  ebus.processReceivedChar(EBUS_ACK);
  ebus.processReceivedChar(EBUS_SYN);

  Ebus::Telegram* telegramDequeued;
  Ebus::Telegram* telegram;
  while ((telegramDequeued = (Ebus::Telegram*)telegramHistoryMockQueue.dequeue()) != NULL) {
    telegram = telegramDequeued;
  }

  TEST_ASSERT_EQUAL_UINT8(telegram->getResponseByte(0), 'f');
  TEST_ASSERT_EQUAL_UINT8(telegram->getResponseByte(1), 'i');
  TEST_ASSERT_EQUAL_UINT8(telegram->getResponseByte(2), 'x');
  TEST_ASSERT_TRUE(telegram->isRequestValid());
  TEST_ASSERT_TRUE(telegram->isResponseValid());
}

void test_ebus_in_arbitration() {
  setupEbus();
  SEND(P99_PROTECT({0x03, EBUS_SYN}));
  TEST_ASSERT_EQUAL_INT(Ebus::TelegramState::endArbitration, ebus.getReceivingTelegram().getState());

  // next message should be valid
  SEND(P99_PROTECT({0x03, 0x64, 0xb5, 0x12}));
  TEST_ASSERT_FALSE(ebus.getReceivingTelegram().isRequestComplete());
}

// not really a test (yet) but hey ;)
void test_multiple() {
  setupEbus();
  for (int i = 0; i < 10; i++) {
    ebus.processReceivedChar(i);
    ebus.processReceivedChar(EBUS_SYN);
  }
  Ebus::Telegram* telegram;
  while ((telegram = (Ebus::Telegram*)telegramHistoryMockQueue.dequeue()) != NULL) {
    printf("QQ: %02x, state: %d\n", telegram->getQQ(), telegram->getState());
  }
}

void test_ebusDequeueSend() {
  uint8_t payload[2] = {0x5, 0x06};
  Ebus::SendCommand command = Ebus::SendCommand(0x00, 0x24, 0x01, 0x02, sizeof(payload), payload);

  telegramSendMockQueue.enqueue(&command);

  Ebus::SendCommand dequeued;

  TEST_ASSERT_TRUE(ebusDequeueCommand(&dequeued));

  TEST_ASSERT_EQUAL_HEX8(0x00, dequeued.getQQ());
  TEST_ASSERT_EQUAL_HEX8(0x24, dequeued.getZZ());
  TEST_ASSERT_EQUAL_HEX8(0x01, dequeued.getPB());
  TEST_ASSERT_EQUAL_HEX8(0x02, dequeued.getSB());
}

void test_send_command() {
  setupEbus();
  Ebus::SendCommand command = Ebus::SendCommand(0x00, 0x05, 0x07, 0x04, 0, NULL);
  telegramSendMockQueue.enqueue(&command);
  ebus.processReceivedChar(EBUS_SYN);

  Ebus::Telegram* telegram = (Ebus::Telegram*)telegramHistoryMockQueue.dequeue();

  TEST_ASSERT_EQUAL_HEX8(0x00, telegram->getQQ());
  TEST_ASSERT_EQUAL_HEX8(0x07, telegram->getPB());
  TEST_ASSERT_TRUE(telegram->isRequestValid());

  printf("response: ");
  for (int i = 0; i < telegram->getResponseNN(); i++) {
    printf("%02x ", telegram->getResponseByte(i));
  }
  printf("\n");
}

int main(int argc, char** argv) {
  UNITY_BEGIN();

  printf("==== sizeof Ebus::TelegramBase : %d\n", (uint16_t)sizeof(Ebus::TelegramBase));
  printf("==== sizeof Ebus::Telegram     : %d\n", (uint16_t)sizeof(Ebus::Telegram));
  printf("==== sizeof Ebus::SendCommand  : %d\n\n", (uint16_t)sizeof(Ebus::SendCommand));

  RUN_TEST(test_crc);
  RUN_TEST(test_is_master);
  RUN_TEST(test_getter);
  RUN_TEST(test_telegram_completion);
  RUN_TEST(test_telegram_with_escape);
  RUN_TEST(test_telegram_master_master_completed_after_ack);
  RUN_TEST(test_telegram_master_slave_completed_after_response_ack);
  RUN_TEST(test_telegram_identity_response);
  RUN_TEST(test_ebus_in_arbitration);
  RUN_TEST(test_multiple);
  RUN_TEST(test_ebusDequeueSend);
  RUN_TEST(test_send_command);
  UNITY_END();
}

#endif

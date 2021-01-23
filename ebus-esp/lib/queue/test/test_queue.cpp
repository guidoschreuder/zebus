//#ifdef UNIT_TEST

#include <unity.h>
#include <stdlib.h>
#include "mock-queue.h"

struct TestStruct {
  uint8_t int1;
  uint8_t int2;
};

void test_queue() {
  Queue q(5, Queue::OnFull::ignore);
  struct TestStruct *obj1 = (struct TestStruct*)malloc(sizeof(struct TestStruct));
  obj1->int1 = 14;
  obj1->int2 = 32;
  TEST_ASSERT_EQUAL_INT(0, q.enqueue(obj1));

  TestStruct *obj2 = (TestStruct*) q.dequeue();

  TEST_ASSERT_EQUAL_INT(obj1->int1, obj2->int1);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_queue);

  UNITY_END();
}

//#endif

#ifndef __QUEUE
#define __QUEUE

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct Queue {
  struct Node {
    void* data;
    struct Node* next;
  };

  struct Node* head;
  struct Node* tail;

  uint16_t capacity;
  uint16_t size;

  Queue(int max_capacity) {
    capacity = max_capacity;
  }

  bool is_full() {
    return size == capacity;
  }

  int enqueue(void* data) {
    if (is_full()) {
      return -1;
    }
    struct Node* node = (struct Node*)malloc(sizeof(struct Node));
    if (node == NULL) {
      return -1;
    }
    node->data = data;
    node->next = NULL;

    if (head == NULL && tail == NULL) {
      head = tail = node;
    } else {
      tail->next = node;
      tail = node;
    }
    size++;
    return 0;
  }

  void* dequeue() {
    Node* oldHead = head;
    if (head == NULL) {
      return NULL;
    }
    void* data = oldHead->data;
    if (head == tail) {
      head = tail = NULL;
    } else {
      head = head->next;
    }
    size--;
    free(oldHead);
    return data;
  }
};

#endif

#ifndef __QUEUE
#define __QUEUE

#include <stdint.h>

#include <cstdlib>

struct Queue {
  struct Node {
    void* data;
    struct Node* next;
  };

  enum OnFull {
    ignore = 1,
    removeOldest = 2,
    overwriteHead = 3,
    overwriteTail = 4,
  };
  struct Node* head;
  struct Node* tail;

  uint16_t capacity;
  uint16_t size;
  uint8_t on_full;

  Queue(int max_capacity, const int action_on_full) {
    capacity = max_capacity;
    on_full = action_on_full;
  }

  bool is_full() {
    return size == capacity;
  }

  int enqueue(void* data) {
    if (is_full()) {
      switch (on_full) {
      case removeOldest:
        struct Node* node;
        node = head;
        free(node->data);
        node->data = data;
        head = head->next;
        tail->next = node;
        tail = node;
        return 0;
        break;
      case overwriteHead:
        free(head->data);
        head->data = data;
        return 0;
        break;
      case overwriteTail:
        free(tail->data);
        tail->data = data;
        return 0;
        break;
      case ignore:
      default:
        return -1;
      }
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

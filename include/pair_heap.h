#ifndef PAIR_HEAP_H
#define PAIR_HEAP_H

#include <stddef.h>

typedef struct PairEntry {
  int token_left;
  int token_right;
  int count;
  int heap_index;
  int occ_head;
  int next_free;
  int in_use;
} PairEntry;

typedef struct PairHeap {
  int *data;
  int size;
  int capacity;
} PairHeap;

void pair_heap_init(PairHeap *heap, int capacity_hint);
void pair_heap_free(PairHeap *heap);
void pair_heap_update(PairHeap *heap, PairEntry *entries, int pair_index);
int pair_heap_pop_max(PairHeap *heap, PairEntry *entries);
void pair_heap_remove(PairHeap *heap, PairEntry *entries, int pair_index);

#endif  // PAIR_HEAP_H

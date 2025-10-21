#include "pair_heap.h"

#include <stdio.h>
#include <stdlib.h>

static void pair_heap_reserve(PairHeap *heap, int min_capacity) {
  if (heap->capacity >= min_capacity)
    return;

  int new_cap = heap->capacity ? heap->capacity : 16;
  while (new_cap < min_capacity)
    new_cap *= 2;

  int *new_data = realloc(heap->data, sizeof(int) * new_cap);
  if (!new_data) {
    fprintf(stderr, "Failed to grow pair heap\n");
    exit(1);
  }

  heap->data = new_data;
  heap->capacity = new_cap;
}

void pair_heap_init(PairHeap *heap, int capacity_hint) {
  heap->capacity = capacity_hint > 0 ? capacity_hint : 16;
  heap->size = 0;
  heap->data = malloc(sizeof(int) * heap->capacity);
  if (!heap->data && heap->capacity > 0) {
    fprintf(stderr, "Failed to allocate pair heap\n");
    exit(1);
  }
}

void pair_heap_free(PairHeap *heap) {
  free(heap->data);
  heap->data = NULL;
  heap->size = 0;
  heap->capacity = 0;
}

static void heap_swap(PairHeap *heap, PairEntry *entries, int a, int b) {
  int pa = heap->data[a];
  int pb = heap->data[b];
  heap->data[a] = pb;
  heap->data[b] = pa;
  entries[pa].heap_index = b;
  entries[pb].heap_index = a;
}

static void heap_sift_up(PairHeap *heap, PairEntry *entries, int idx) {
  while (idx > 0) {
    int parent = (idx - 1) / 2;
    int current = heap->data[idx];
    int parent_idx = heap->data[parent];
    if (entries[current].count <= entries[parent_idx].count)
      break;
    heap_swap(heap, entries, idx, parent);
    idx = parent;
  }
}

static void heap_sift_down(PairHeap *heap, PairEntry *entries, int idx) {
  while (1) {
    int left = idx * 2 + 1;
    int right = left + 1;
    int largest = idx;

    if (left < heap->size &&
        entries[heap->data[left]].count > entries[heap->data[largest]].count)
      largest = left;
    if (right < heap->size &&
        entries[heap->data[right]].count > entries[heap->data[largest]].count)
      largest = right;
    if (largest == idx)
      break;
    heap_swap(heap, entries, idx, largest);
    idx = largest;
  }
}

static void heap_push(PairHeap *heap, PairEntry *entries, int pair_index) {
  pair_heap_reserve(heap, heap->size + 1);
  heap->data[heap->size] = pair_index;
  entries[pair_index].heap_index = heap->size;
  heap->size++;
  heap_sift_up(heap, entries, heap->size - 1);
}

void pair_heap_remove(PairHeap *heap, PairEntry *entries, int pair_index) {
  int idx = entries[pair_index].heap_index;
  if (idx == -1)
    return;

  int last = heap->size - 1;
  heap->size--;
  if (idx != last) {
    heap->data[idx] = heap->data[last];
    entries[heap->data[idx]].heap_index = idx;
    heap_sift_down(heap, entries, idx);
    heap_sift_up(heap, entries, idx);
  }
  entries[pair_index].heap_index = -1;
}

void pair_heap_update(PairHeap *heap, PairEntry *entries, int pair_index) {
  PairEntry *entry = &entries[pair_index];
  if (!entry->in_use || entry->count <= 0) {
    pair_heap_remove(heap, entries, pair_index);
    return;
  }

  if (entry->heap_index == -1) {
    heap_push(heap, entries, pair_index);
  } else {
    int idx = entry->heap_index;
    heap_sift_up(heap, entries, idx);
    heap_sift_down(heap, entries, idx);
  }
}

int pair_heap_pop_max(PairHeap *heap, PairEntry *entries) {
  if (heap->size == 0)
    return -1;

  int top = heap->data[0];
  pair_heap_remove(heap, entries, top);
  return top;
}


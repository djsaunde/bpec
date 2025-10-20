#include "train.h"
#include "vocab.h"
#include "sequence.h"
#include "merge_rules.h"
#include "token.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SeqNode {
  int token_id;
  int prev;
  int next;
  int occ_index;
  int active;
} SeqNode;

typedef struct PairOccurrence {
  int pair_index;
  int left_node;
  int prev_occ;
  int next_occ;
  int active;
} PairOccurrence;

typedef struct PairEntry {
  int token_left;
  int token_right;
  int count;
  int heap_index;
  int occ_head;
  int next_free;
  int in_use;
} PairEntry;

typedef struct {
  PairOccurrence *items;
  int size;
  int capacity;
  int free_head;
} OccurrencePool;

typedef struct {
  uint64_t *keys;
  int *values;
  int capacity;
  int size;
} PairMap;

typedef struct {
  int *data;
  int size;
  int capacity;
} PairHeap;

typedef struct {
  SeqNode *nodes;
  int node_count;
  int head;
  int live_count;

  PairEntry *pairs;
  int pair_count;
  int pair_capacity;
  int pair_free_head;

  OccurrencePool occ_pool;
  PairMap map;
  PairHeap heap;
} TrainerState;

static void occ_pool_init(OccurrencePool *pool, int capacity) {
  pool->items = malloc(sizeof(PairOccurrence) * capacity);
  if (!pool->items && capacity > 0) {
    fprintf(stderr, "Failed to allocate occurrence pool\n");
    exit(1);
  }
  pool->capacity = pool->items ? capacity : 0;
  pool->size = 0;
  pool->free_head = -1;
}

static void occ_pool_grow(OccurrencePool *pool) {
  int new_cap = pool->capacity ? pool->capacity * 2 : 128;
  PairOccurrence *new_items = realloc(pool->items, sizeof(PairOccurrence) * new_cap);
  if (!new_items) {
    fprintf(stderr, "Failed to grow occurrence pool\n");
    exit(1);
  }
  pool->items = new_items;
  pool->capacity = new_cap;
}

static int occ_pool_acquire(OccurrencePool *pool) {
  if (pool->free_head != -1) {
    int index = pool->free_head;
    PairOccurrence *occ = &pool->items[index];
    pool->free_head = occ->next_occ;
    occ->prev_occ = -1;
    occ->next_occ = -1;
    occ->active = 1;
    return index;
  }

  if (pool->size >= pool->capacity)
    occ_pool_grow(pool);

  int index = pool->size++;
  PairOccurrence *occ = &pool->items[index];
  occ->pair_index = -1;
  occ->left_node = -1;
  occ->prev_occ = -1;
  occ->next_occ = -1;
  occ->active = 1;
  return index;
}

static void occ_pool_release(OccurrencePool *pool, int index) {
  PairOccurrence *occ = &pool->items[index];
  occ->active = 0;
  occ->next_occ = pool->free_head;
  occ->prev_occ = -1;
  pool->free_head = index;
}

static void occ_pool_free(OccurrencePool *pool) {
  free(pool->items);
  pool->items = NULL;
  pool->capacity = 0;
  pool->size = 0;
  pool->free_head = -1;
}

static inline uint64_t make_pair_key(int left, int right) {
  return ((uint64_t)(uint32_t)left << 32) | (uint32_t)right;
}

static uint64_t hash64(uint64_t x) {
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33;
  x *= 0xc4ceb9fe1a85ec53ULL;
  x ^= x >> 33;
  return x;
}

static int next_pow2(int n) {
  int p = 1;
  while (p < n) p <<= 1;
  return p;
}

static void pair_map_init(PairMap *map, int capacity_hint) {
  int cap = next_pow2(capacity_hint > 0 ? capacity_hint : 16);
  map->capacity = cap;
  map->size = 0;
  map->keys = malloc(sizeof(uint64_t) * map->capacity);
  map->values = malloc(sizeof(int) * map->capacity);
  if (!map->keys || !map->values) {
    fprintf(stderr, "Failed to allocate pair map\n");
    exit(1);
  }
  for (int i = 0; i < map->capacity; i++)
    map->values[i] = -1;
}

static void pair_map_free(PairMap *map) {
  free(map->keys);
  free(map->values);
  map->keys = NULL;
  map->values = NULL;
  map->capacity = 0;
  map->size = 0;
}

static int pair_map_find_slot(PairMap *map, uint64_t key) {
  int mask = map->capacity - 1;
  int idx = (int)(hash64(key) & mask);
  while (map->values[idx] != -1 && map->keys[idx] != key)
    idx = (idx + 1) & mask;
  return idx;
}

static void pair_map_rehash(PairMap *map, int new_capacity) {
  PairMap tmp;
  tmp.capacity = next_pow2(new_capacity);
  tmp.size = 0;
  tmp.keys = malloc(sizeof(uint64_t) * tmp.capacity);
  tmp.values = malloc(sizeof(int) * tmp.capacity);
  if (!tmp.keys || !tmp.values) {
    fprintf(stderr, "Failed to grow pair map\n");
    exit(1);
  }
  for (int i = 0; i < tmp.capacity; i++)
    tmp.values[i] = -1;

  int mask = tmp.capacity - 1;
  for (int i = 0; i < map->capacity; i++) {
    if (map->values[i] != -1) {
      uint64_t key = map->keys[i];
      int value = map->values[i];
      int slot = (int)(hash64(key) & mask);
      while (tmp.values[slot] != -1)
        slot = (slot + 1) & mask;
      tmp.keys[slot] = key;
      tmp.values[slot] = value;
      tmp.size++;
    }
  }

  free(map->keys);
  free(map->values);
  *map = tmp;
}

static void pair_map_set(PairMap *map, uint64_t key, int value) {
  if ((map->size + 1) * 4 >= map->capacity * 3)
    pair_map_rehash(map, map->capacity * 2);

  int idx = pair_map_find_slot(map, key);
  if (map->values[idx] == -1)
    map->size++;
  map->keys[idx] = key;
  map->values[idx] = value;
}

static int pair_map_get(PairMap *map, uint64_t key) {
  int idx = pair_map_find_slot(map, key);
  return map->values[idx];
}

static void pair_map_remove(PairMap *map, uint64_t key) {
  int mask = map->capacity - 1;
  int idx = (int)(hash64(key) & mask);
  while (map->values[idx] != -1 && map->keys[idx] != key)
    idx = (idx + 1) & mask;
  if (map->values[idx] == -1)
    return;

  map->values[idx] = -1;
  map->size--;

  int next = (idx + 1) & mask;
  while (map->values[next] != -1) {
    uint64_t rekey = map->keys[next];
    int value = map->values[next];
    map->values[next] = -1;

    int slot = (int)(hash64(rekey) & mask);
    while (map->values[slot] != -1)
      slot = (slot + 1) & mask;
    map->keys[slot] = rekey;
    map->values[slot] = value;

    next = (next + 1) & mask;
  }
}

static void heap_init(PairHeap *heap, int capacity_hint) {
  heap->capacity = capacity_hint > 0 ? capacity_hint : 16;
  heap->size = 0;
  heap->data = malloc(sizeof(int) * heap->capacity);
  if (!heap->data) {
    fprintf(stderr, "Failed to allocate pair heap\n");
    exit(1);
  }
}

static void heap_free(PairHeap *heap) {
  free(heap->data);
  heap->data = NULL;
  heap->capacity = 0;
  heap->size = 0;
}

static void heap_reserve(PairHeap *heap, int min_capacity) {
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

static void heap_swap(PairHeap *heap, TrainerState *state, int a, int b) {
  int pa = heap->data[a];
  int pb = heap->data[b];
  heap->data[a] = pb;
  heap->data[b] = pa;
  state->pairs[pa].heap_index = b;
  state->pairs[pb].heap_index = a;
}

static void heap_sift_up(PairHeap *heap, TrainerState *state, int idx) {
  while (idx > 0) {
    int parent = (idx - 1) / 2;
    int current_pair = heap->data[idx];
    int parent_pair = heap->data[parent];
    if (state->pairs[current_pair].count <= state->pairs[parent_pair].count)
      break;
    heap_swap(heap, state, idx, parent);
    idx = parent;
  }
}

static void heap_sift_down(PairHeap *heap, TrainerState *state, int idx) {
  while (1) {
    int left = idx * 2 + 1;
    int right = left + 1;
    int largest = idx;

    if (left < heap->size &&
        state->pairs[heap->data[left]].count > state->pairs[heap->data[largest]].count)
      largest = left;
    if (right < heap->size &&
        state->pairs[heap->data[right]].count > state->pairs[heap->data[largest]].count)
      largest = right;
    if (largest == idx)
      break;
    heap_swap(heap, state, idx, largest);
    idx = largest;
  }
}

static void heap_push(PairHeap *heap, TrainerState *state, int pair_index) {
  heap_reserve(heap, heap->size + 1);
  heap->data[heap->size] = pair_index;
  state->pairs[pair_index].heap_index = heap->size;
  heap->size++;
  heap_sift_up(heap, state, heap->size - 1);
}

static void heap_remove_at(PairHeap *heap, TrainerState *state, int idx) {
  int last = heap->size - 1;
  int pair_idx = heap->data[idx];
  heap->size--;
  if (idx != last) {
    heap->data[idx] = heap->data[last];
    state->pairs[heap->data[idx]].heap_index = idx;
    heap_sift_down(heap, state, idx);
    heap_sift_up(heap, state, idx);
  }
  state->pairs[pair_idx].heap_index = -1;
}

static void heap_update(PairHeap *heap, TrainerState *state, int pair_index) {
  PairEntry *entry = &state->pairs[pair_index];
  if (entry->count <= 0) {
    if (entry->heap_index != -1)
      heap_remove_at(heap, state, entry->heap_index);
    entry->heap_index = -1;
    return;
  }

  if (entry->heap_index == -1) {
    heap_push(heap, state, pair_index);
  } else {
    int idx = entry->heap_index;
    heap_sift_up(heap, state, idx);
    heap_sift_down(heap, state, idx);
  }
}

static int heap_pop_max(PairHeap *heap, TrainerState *state) {
  if (heap->size == 0)
    return -1;
  int top = heap->data[0];
  heap_remove_at(heap, state, 0);
  return top;
}

static void trainer_pairs_grow(TrainerState *state) {
  int new_cap = state->pair_capacity ? state->pair_capacity * 2 : 32;
  PairEntry *new_pairs = realloc(state->pairs, sizeof(PairEntry) * new_cap);
  if (!new_pairs) {
    fprintf(stderr, "Failed to grow pair entries\n");
    exit(1);
  }
  for (int i = state->pair_capacity; i < new_cap; i++) {
    new_pairs[i].heap_index = -1;
    new_pairs[i].occ_head = -1;
    new_pairs[i].count = 0;
    new_pairs[i].token_left = -1;
    new_pairs[i].token_right = -1;
    new_pairs[i].next_free = -1;
    new_pairs[i].in_use = 0;
  }
  state->pairs = new_pairs;
  state->pair_capacity = new_cap;
}

static void trainer_pairs_init(TrainerState *state, int capacity_hint) {
  state->pair_capacity = capacity_hint > 0 ? capacity_hint : 32;
  state->pair_count = 0;
  state->pair_free_head = -1;
  state->pairs = malloc(sizeof(PairEntry) * state->pair_capacity);
  if (!state->pairs && state->pair_capacity > 0) {
    fprintf(stderr, "Failed to allocate pair entries\n");
    exit(1);
  }
  for (int i = 0; i < state->pair_capacity; i++) {
    state->pairs[i].heap_index = -1;
    state->pairs[i].occ_head = -1;
    state->pairs[i].count = 0;
    state->pairs[i].token_left = -1;
    state->pairs[i].token_right = -1;
    state->pairs[i].next_free = -1;
    state->pairs[i].in_use = 0;
  }
}

static int trainer_acquire_pair_entry(TrainerState *state) {
  int idx;
  if (state->pair_free_head != -1) {
    idx = state->pair_free_head;
    state->pair_free_head = state->pairs[idx].next_free;
  } else {
    if (state->pair_count >= state->pair_capacity)
      trainer_pairs_grow(state);
    idx = state->pair_count++;
  }

  PairEntry *entry = &state->pairs[idx];
  entry->heap_index = -1;
  entry->occ_head = -1;
  entry->count = 0;
  entry->token_left = -1;
  entry->token_right = -1;
  entry->next_free = -1;
  entry->in_use = 1;
  return idx;
}

static void trainer_release_pair_entry(TrainerState *state, int index) {
  PairEntry *entry = &state->pairs[index];
  entry->in_use = 0;
  entry->heap_index = -1;
  entry->occ_head = -1;
  entry->count = 0;
  entry->token_left = -1;
  entry->token_right = -1;
  entry->next_free = state->pair_free_head;
  state->pair_free_head = index;
}

static void trainer_sequence_init(TrainerState *state, TokenSequence *seq) {
  state->node_count = seq->length;
  state->live_count = seq->length;
  if (state->node_count == 0) {
    state->nodes = NULL;
    state->head = -1;
    return;
  }

  state->nodes = malloc(sizeof(SeqNode) * state->node_count);
  if (!state->nodes) {
    fprintf(stderr, "Failed to allocate sequence nodes\n");
    exit(1);
  }

  for (int i = 0; i < state->node_count; i++) {
    SeqNode *node = &state->nodes[i];
    node->token_id = seq->tokens[i];
    node->prev = (i == 0) ? -1 : i - 1;
    node->next = (i == state->node_count - 1) ? -1 : i + 1;
    node->occ_index = -1;
    node->active = 1;
  }

  state->head = state->node_count > 0 ? 0 : -1;
}

static void pair_entry_remove_occurrence(TrainerState *state, int occ_index, int update_heap) {
  PairOccurrence *occ = &state->occ_pool.items[occ_index];
  if (!occ->active)
    return;

  int pair_index = occ->pair_index;
  PairEntry *entry = &state->pairs[pair_index];

  if (occ->prev_occ != -1)
    state->occ_pool.items[occ->prev_occ].next_occ = occ->next_occ;
  else
    entry->occ_head = occ->next_occ;

  if (occ->next_occ != -1)
    state->occ_pool.items[occ->next_occ].prev_occ = occ->prev_occ;

  if (state->nodes[occ->left_node].occ_index == occ_index)
    state->nodes[occ->left_node].occ_index = -1;

  occ->active = 0;
  entry->count--;
  if (entry->count < 0)
    entry->count = 0;

  occ_pool_release(&state->occ_pool, occ_index);

  if (update_heap)
    heap_update(&state->heap, state, pair_index);
}

static void trainer_detach_occurrence_for_node(TrainerState *state, int node_index) {
  if (node_index == -1)
    return;
  SeqNode *node = &state->nodes[node_index];
  if (!node->active)
    return;
  if (node->occ_index != -1)
    pair_entry_remove_occurrence(state, node->occ_index, 1);
}

static void trainer_add_pair_for_node(TrainerState *state, int node_index) {
  if (node_index == -1)
    return;

  SeqNode *left = &state->nodes[node_index];
  if (!left->active) {
    left->occ_index = -1;
    return;
  }

  int right_index = left->next;
  if (right_index == -1) {
    left->occ_index = -1;
    return;
  }

  SeqNode *right = &state->nodes[right_index];
  if (!right->active) {
    left->occ_index = -1;
    return;
  }

  uint64_t key = make_pair_key(left->token_id, right->token_id);
  int pair_index = pair_map_get(&state->map, key);
  if (pair_index == -1) {
    pair_index = trainer_acquire_pair_entry(state);
    PairEntry *entry = &state->pairs[pair_index];
    entry->token_left = left->token_id;
    entry->token_right = right->token_id;
    pair_map_set(&state->map, key, pair_index);
  }

  if (left->occ_index != -1)
    pair_entry_remove_occurrence(state, left->occ_index, 1);

  PairEntry *entry = &state->pairs[pair_index];
  int occ_idx = occ_pool_acquire(&state->occ_pool);
  PairOccurrence *occ = &state->occ_pool.items[occ_idx];
  occ->pair_index = pair_index;
  occ->left_node = node_index;
  occ->prev_occ = -1;
  occ->next_occ = entry->occ_head;
  occ->active = 1;

  if (entry->occ_head != -1)
    state->occ_pool.items[entry->occ_head].prev_occ = occ_idx;

  entry->occ_head = occ_idx;
  entry->count++;
  left->occ_index = occ_idx;

  heap_update(&state->heap, state, pair_index);
}

static void trainer_merge_pair(TrainerState *state, int pair_index, int new_token_id) {
  PairEntry *entry = &state->pairs[pair_index];
  int right_token = entry->token_right;

  while (entry->occ_head != -1) {
    int occ_idx = entry->occ_head;
    PairOccurrence occ = state->occ_pool.items[occ_idx];

    pair_entry_remove_occurrence(state, occ_idx, 0);

    int left_idx = occ.left_node;
    SeqNode *left = &state->nodes[left_idx];
    if (!left->active)
      continue;

    int right_idx = left->next;
    if (right_idx == -1)
      continue;
    SeqNode *right = &state->nodes[right_idx];
    if (!right->active || right->token_id != right_token)
      continue;

    int prev_idx = left->prev;
    int next_idx = right->next;

    if (prev_idx != -1)
      trainer_detach_occurrence_for_node(state, prev_idx);
    trainer_detach_occurrence_for_node(state, right_idx);

    left->token_id = new_token_id;

    left->next = next_idx;
    if (next_idx != -1)
      state->nodes[next_idx].prev = left_idx;
    if (prev_idx != -1)
      state->nodes[prev_idx].next = left_idx;
    else
      state->head = left_idx;

    right->active = 0;
    right->prev = -1;
    right->next = -1;
    right->occ_index = -1;
    state->live_count--;

    if (prev_idx != -1)
      trainer_add_pair_for_node(state, prev_idx);
    trainer_add_pair_for_node(state, left_idx);
  }
}

static void trainer_state_init(TrainerState *state, TokenSequence *seq) {
  memset(state, 0, sizeof(*state));
  trainer_sequence_init(state, seq);

  int hint = seq->length > 0 ? seq->length : 1;
  occ_pool_init(&state->occ_pool, hint);
  pair_map_init(&state->map, hint * 2);
  trainer_pairs_init(state, hint);
  heap_init(&state->heap, hint);

  for (int idx = state->head; idx != -1; idx = state->nodes[idx].next)
    trainer_add_pair_for_node(state, idx);
}

static void trainer_state_free(TrainerState *state) {
  free(state->nodes);
  state->nodes = NULL;
  state->node_count = 0;
  state->head = -1;
  state->live_count = 0;

  occ_pool_free(&state->occ_pool);
  pair_map_free(&state->map);
  heap_free(&state->heap);
  free(state->pairs);
  state->pairs = NULL;
  state->pair_capacity = 0;
  state->pair_count = 0;
  state->pair_free_head = -1;
}

void train_bpe(Vocabulary *vocab, TokenSequence *seq, int target_vocab_size, MergeRules *merge_rules) {
  printf("Starting BPE training...\n");
  printf("Initial vocab size: %d\n", vocab->size);
  printf("Target vocab size: %d\n", target_vocab_size);

  int initial_length = seq->length;
  TrainerState state;
  trainer_state_init(&state, seq);

  while (vocab->size < target_vocab_size) {
    if (state.live_count < 2) {
      printf("No more pairs to merge!\n");
      break;
    }

    int pair_index = heap_pop_max(&state.heap, &state);
    if (pair_index == -1) {
      printf("No more pairs to merge!\n");
      break;
    }

    PairEntry *entry = &state.pairs[pair_index];
    if (!entry->in_use || entry->count == 0) {
      trainer_release_pair_entry(&state, pair_index);
      continue;
    }

    int left_token = entry->token_left;
    int right_token = entry->token_right;
    int occurrence_count = entry->count;

    Token merged = merge_tokens(&vocab->tokens[left_token],
                                 &vocab->tokens[right_token]);

    printf("Merge %d: ", vocab->size - 256);
    print_token(&vocab->tokens[left_token]);
    printf(" + ");
    print_token(&vocab->tokens[right_token]);
    printf(" → ");
    print_token(&merged);
    printf(" (count: %d)\n", occurrence_count);

    int new_idx = add_token(vocab, merged.bytes, merged.length);
    add_merge_rule(merge_rules, left_token, right_token, new_idx);

    trainer_merge_pair(&state, pair_index, new_idx);

    pair_map_remove(&state.map, make_pair_key(left_token, right_token));
    trainer_release_pair_entry(&state, pair_index);
    free_token(&merged);
  }

  int pos = 0;
  for (int idx = state.head; idx != -1; idx = state.nodes[idx].next) {
    if (!state.nodes[idx].active)
      continue;
    seq->tokens[pos++] = state.nodes[idx].token_id;
  }
  seq->length = pos;

  trainer_state_free(&state);

  printf("\nTraining complete!\n");
  printf("Final vocab size: %d\n", vocab->size);
  printf("Final sequence length: %d\n", seq->length);
  printf("Initial sequence length: %d tokens\n", initial_length);

  float compression = (seq->length > 0) ? (float)initial_length / seq->length : 0.0f;
  int reduced = initial_length - seq->length;
  float percent = (initial_length > 0) ? (100.0f * reduced) / initial_length : 0.0f;

  if (seq->length > 0)
    printf("Compression ratio: %.2fx\n", compression);
  else
    printf("Compression ratio: N/A (sequence collapsed)\n");

  printf("Tokens reduced by: %d (%.1f%%)\n", reduced, percent);
}

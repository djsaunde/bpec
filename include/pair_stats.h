#ifndef PAIR_STATS_H
#define PAIR_STATS_H

#include "sequence.h"

typedef struct PairNode {
  int token1;
  int token2;
  int count;
  struct PairNode *next;
} PairNode;

typedef struct {
  PairNode **buckets;
  int num_buckets;
  int size;
} PairHashTable;

PairHashTable create_pair_table(int num_buckets);
void free_pair_table(PairHashTable *table);
void increment_pair(PairHashTable *table, int token1, int token2);
PairNode* find_best_pair_in_table(PairHashTable *table);
void count_pairs_hash(TokenSequence *seq, PairHashTable *table);

#endif  // PAIR_STATS_H

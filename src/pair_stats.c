#include "pair_stats.h"

#include <stdio.h>
#include <stdlib.h>

unsigned int hash_pair(int token1, int token2, int num_buckets) {
  // Simple hash: combine the two token IDs
  unsigned int hash = token1 * 65537 + token2;
  return hash % num_buckets;
}

PairHashTable create_pair_table(int num_buckets) {
  PairHashTable table;
  table.num_buckets = num_buckets;
  table.size = 0;
  table.buckets = calloc(num_buckets, sizeof(PairNode*));
  if (table.buckets == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }
  return table;
}


void clear_pair_table(PairHashTable *table) {
  for (int i = 0; i < table->num_buckets; i++) {
    PairNode *node = table->buckets[i];
    while (node != NULL) {
      PairNode *next = node->next;
      free(node);
      node = next;
    }
    table->buckets[i] = NULL;
  }
  table->size = 0;
}

void free_pair_table(PairHashTable *table) {
  clear_pair_table(table);
  free(table->buckets);
  table->buckets = NULL;
  table->size = 0;
}

void increment_pair(PairHashTable *table, int token1, int token2) {
  unsigned int bucket = hash_pair(token1, token2, table->num_buckets);

  // Search for existing pair in this bucket
  PairNode *node = table->buckets[bucket];
  while (node != NULL) {
    if (node->token1 == token1 && node->token2 == token2) {
      node->count++;
      return;
    }
    node = node->next;
  }

  // Not found - create new node
  PairNode *new_node = malloc(sizeof(PairNode));
  if (new_node == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }
  new_node->token1 = token1;
  new_node->token2 = token2;
  new_node->count = 1;
  new_node->next = table->buckets[bucket];
  table->buckets[bucket] = new_node;
  table->size++;
}

PairNode* find_best_pair_in_table(PairHashTable *table) {
  PairNode *best = NULL;
  int max_count = 0;

  for (int i = 0; i < table->num_buckets; i++) {
    PairNode *node = table->buckets[i];
    while (node != NULL) {
      if (node->count > max_count) {
        max_count = node->count;
        best = node;
      }
      node = node->next;
    }
  }

  return best;
}

void count_pairs_hash(TokenSequence *seq, PairHashTable *table) {
  if (seq->length < 2) return;

  for (int i = 0; i < seq->length - 1; i++) {
    int t1 = seq->tokens[i];
    int t2 = seq->tokens[i + 1];
    increment_pair(table, t1, t2);
  }
}

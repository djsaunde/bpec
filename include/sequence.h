#ifndef SEQUENCE_H
#define SEQUENCE_H

#include <string.h>
#include <stdint.h>
#include "merge_rules.h"
#include "vocab.h"

typedef struct {
  int *tokens;
  int length;
  int capacity;
} TokenSequence;

TokenSequence create_sequence(int capacity);
void free_sequence(TokenSequence *seq);
TokenSequence text_to_sequence(uint8_t *text, int text_len);
void print_sequence(TokenSequence *seq, Vocabulary *vocab);
void merge_pair_in_sequence(TokenSequence *seq, int token1, int token2, int new_token);
TokenSequence encode(uint8_t *text, int text_len, MergeRules *rules);
uint8_t* decode(TokenSequence *seq, Vocabulary *vocab, int *output_len);

#endif  // SEQUENCE_H

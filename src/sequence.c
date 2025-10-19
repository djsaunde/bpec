#include "sequence.h"
#include "token.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TokenSequence create_sequence(int capacity) {
  TokenSequence seq;
  seq.length = 0;
  seq.capacity = capacity;
  seq.tokens = malloc(sizeof(int) * capacity);
  if (seq.tokens == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }
  return seq;
}

void free_sequence(TokenSequence *seq) {
  free(seq->tokens);
  seq->tokens = NULL;
  seq->length = 0;
  seq->capacity = 0;
}

TokenSequence text_to_sequence(uint8_t *text, int text_len) {
  TokenSequence seq = create_sequence(text_len);
  for (int i = 0; i < text_len; i++)
    seq.tokens[i] = (int)text[i];
  seq.length = text_len;
  return seq;
}

void print_sequence(TokenSequence *seq, Vocabulary *vocab) {
  printf("Sequence (%d tokens): ", seq->length);
  for (int i = 0; i < seq->length; i++) {
    print_token(&vocab->tokens[seq->tokens[i]]);
    if (i < seq->length - 1) printf(" ");
  }
  printf("\n");
}

void merge_pair_in_sequence(TokenSequence *seq, int token1, int token2, int new_token) {
  int write_pos = 0;

  for (int read_pos = 0; read_pos < seq->length; read_pos++) {
    // Check if we found the pair to merge
    if (read_pos < seq->length - 1 &&
      seq->tokens[read_pos] == token1 &&
      seq->tokens[read_pos+1] == token2) {
      // Replace pair with new token
      seq->tokens[write_pos] = new_token;
      write_pos++;
      read_pos++;  // Skip the second token of the pair
    } else {
      // Keep token as is
      seq->tokens[write_pos] = seq->tokens[read_pos];
      write_pos++;
    }
  }

  seq->length = write_pos;
}

TokenSequence encode(uint8_t *text, int text_len, MergeRules *rules) {
  // Start with base tokenization
  TokenSequence seq = text_to_sequence(text, text_len);

  // Apply each merge rule in order
  for (int i = 0; i < rules->num_rules; i++) {
    MergeRule *rule = &rules->rules[i];
    merge_pair_in_sequence(&seq, rule->token1, rule->token2, rule->result_token);
  }

  return seq;
}

uint8_t* decode(TokenSequence *seq, Vocabulary *vocab, int *output_len) {
  // First calculate total length needed
  int total_len = 0;
  for (int i = 0; i < seq->length; i++)
    total_len += vocab->tokens[seq->tokens[i]].length;

  // Allocate output buffer
  uint8_t *output = malloc(total_len);
  if (output == NULL) {
    fprintf(stderr, "Memory allocation failed");
    exit(1);
  }

  // Copy token bytes into output
  int pos = 0;
  for (int i = 0; i < seq->length; i++) {
    Token *token = &vocab->tokens[seq->tokens[i]];
    memcpy(output + pos, token->bytes, token->length);
    pos += token->length;
  }

  *output_len = total_len;
  return output;
}

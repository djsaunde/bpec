#ifndef VOCAB_H
#define VOCAB_H

#include "token.h"

typedef struct {
  Token *tokens;
  int size;
  int capacity;
} Vocabulary;

Vocabulary create_vocab(int max_size);
void free_vocab(Vocabulary *vocab);
int add_token(Vocabulary *vocab, uint8_t *bytes, int length);
void init_base_vocab(Vocabulary *vocab);

#endif  // VOCAB_H

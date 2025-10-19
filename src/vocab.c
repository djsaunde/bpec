#include "vocab.h"
#include "token.h"

#include <stdio.h>
#include <stdlib.h>

Vocabulary create_vocab(int max_size) {
  Vocabulary vocab;
  vocab.size = 0;
  vocab.capacity = max_size;
  vocab.tokens = malloc(sizeof(Token) * max_size);
  if (vocab.tokens == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }
  return vocab;
}

void free_vocab(Vocabulary *vocab) {
  for (int i = 0; i < vocab->size; i++)
    free_token(&vocab->tokens[i]);
  free(vocab->tokens);
  vocab->tokens = NULL;
  vocab->size = 0;
  vocab->capacity = 0;
}

int add_token(Vocabulary *vocab, uint8_t *bytes, int length) {
  if (vocab->size >= vocab->capacity) {
    fprintf(stderr, "Vocabulary is full! Cannot add more tokens.\n");
    exit(1);
  }

  vocab->tokens[vocab->size] = create_token(bytes, length);
  vocab->size++;
  return vocab->size - 1;
}

void init_base_vocab(Vocabulary *vocab) {
  for (int i = 0; i < 256; i++) {
    uint8_t byte = (uint8_t)i;
    add_token(vocab, &byte, 1);
  }
  printf("Initialized vocabulary with %d base tokens\n", vocab->size);
}

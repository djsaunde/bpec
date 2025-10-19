#include "token.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Token create_token(uint8_t *bytes, int length) {
  Token t;
  t.length = length;
  t.bytes = malloc(length);
  if (t.bytes == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }
  memcpy(t.bytes, bytes, length);
  return t;
}

void free_token(Token *t) {
  free(t->bytes);
  t->bytes = NULL;
  t->length = 0;  
}

Token merge_tokens(Token *t1, Token *t2) {
  int new_length = t1->length + t2->length;
  Token merged;
  merged.length = new_length;
  merged.bytes = malloc(new_length);
  if (merged.bytes == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }

  // Copy first token
  memcpy(merged.bytes, t1->bytes, t1->length);
  // Copy second token
  memcpy(merged.bytes + t1->length, t2->bytes, t2->length);

  return merged;
}

void print_token(Token *t) {
  printf("[");
  for (int i = 0; i < t->length; i++) {
    uint8_t byte = t->bytes[i];
    if (byte >= 32 && byte < 127) {
      // Printable ASCII
      printf("%c", byte);
    } else {
      // Non-printable - show as hex
      printf("\\x%02x", byte);
    }
  }
  printf("]");
}

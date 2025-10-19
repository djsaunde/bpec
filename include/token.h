#ifndef TOKEN_H
#define TOKEN_H

#include <stdint.h>

typedef struct {
  uint8_t *bytes;
  int length;
} Token;

Token create_token(uint8_t *bytes, int length);
void free_token(Token *t);
void print_token(Token *t);
Token merge_tokens(Token *t1, Token *t2);

#endif  // TOKEN_H

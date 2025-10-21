#include "tokenizer_io.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_u32(FILE *fp, uint32_t value) {
  return fwrite(&value, sizeof(uint32_t), 1, fp) == 1 ? 0 : -1;
}

static int read_u32(FILE *fp, uint32_t *value) {
  return fread(value, sizeof(uint32_t), 1, fp) == 1 ? 0 : -1;
}

int save_tokenizer(const char *path, const Vocabulary *vocab, const MergeRules *rules) {
  FILE *fp = fopen(path, "wb");
  if (!fp) {
    perror("fopen");
    return -1;
  }

  const uint8_t magic[4] = {'B', 'P', 'E', 'C'};
  if (fwrite(magic, 1, 4, fp) != 4) {
    fclose(fp);
    return -1;
  }

  if (write_u32(fp, 1) != 0) {  // format version
    fclose(fp);
    return -1;
  }

  if (write_u32(fp, (uint32_t)vocab->size) != 0) {
    fclose(fp);
    return -1;
  }

  for (int i = 0; i < vocab->size; ++i) {
    const Token *token = &vocab->tokens[i];
    if (write_u32(fp, (uint32_t)token->length) != 0) {
      fclose(fp);
      return -1;
    }
    if (token->length > 0) {
      if (fwrite(token->bytes, 1, token->length, fp) != (size_t)token->length) {
        fclose(fp);
        return -1;
      }
    }
  }

  if (write_u32(fp, (uint32_t)rules->num_rules) != 0) {
    fclose(fp);
    return -1;
  }

  for (int i = 0; i < rules->num_rules; ++i) {
    const MergeRule *rule = &rules->rules[i];
    if (write_u32(fp, (uint32_t)rule->token1) != 0 ||
        write_u32(fp, (uint32_t)rule->token2) != 0 ||
        write_u32(fp, (uint32_t)rule->result_token) != 0) {
      fclose(fp);
      return -1;
    }
  }

  fclose(fp);
  return 0;
}

static void free_partial_vocab(Vocabulary *vocab) {
  if (vocab->tokens == NULL)
    return;
  for (int i = 0; i < vocab->size; ++i)
    free_token(&vocab->tokens[i]);
  free(vocab->tokens);
  vocab->tokens = NULL;
  vocab->size = 0;
  vocab->capacity = 0;
}

int load_tokenizer(const char *path, Vocabulary *vocab_out, MergeRules *rules_out) {
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    perror("fopen");
    return -1;
  }

  uint8_t magic[4];
  if (fread(magic, 1, 4, fp) != 4 || memcmp(magic, "BPEC", 4) != 0) {
    fprintf(stderr, "Invalid tokenizer file header\n");
    fclose(fp);
    return -1;
  }

  uint32_t version;
  if (read_u32(fp, &version) != 0 || version != 1) {
    fprintf(stderr, "Unsupported tokenizer format version\n");
    fclose(fp);
    return -1;
  }

  uint32_t token_count;
  if (read_u32(fp, &token_count) != 0) {
    fclose(fp);
    return -1;
  }

  Vocabulary vocab = create_vocab((int)token_count);

  for (uint32_t i = 0; i < token_count; ++i) {
    uint32_t length;
    if (read_u32(fp, &length) != 0) {
      free_partial_vocab(&vocab);
      fclose(fp);
      return -1;
    }
    uint8_t *buffer = NULL;
    if (length > 0) {
      buffer = malloc(length);
      if (!buffer) {
        free_partial_vocab(&vocab);
        fclose(fp);
        return -1;
      }
      if (fread(buffer, 1, length, fp) != length) {
        free(buffer);
        free_partial_vocab(&vocab);
        fclose(fp);
        return -1;
      }
    }
    add_token(&vocab, buffer, (int)length);
    free(buffer);
  }

  uint32_t num_rules;
  if (read_u32(fp, &num_rules) != 0) {
    free_partial_vocab(&vocab);
    fclose(fp);
    return -1;
  }

  MergeRules rules = create_merge_rules(num_rules > 0 ? (int)num_rules : 0);
  if (num_rules > 0 && rules.rules == NULL) {
    // Should not happen unless allocation failed.
    free_partial_vocab(&vocab);
    fclose(fp);
    return -1;
  }

  for (uint32_t i = 0; i < num_rules; ++i) {
    uint32_t t1, t2, res;
    if (read_u32(fp, &t1) != 0 || read_u32(fp, &t2) != 0 || read_u32(fp, &res) != 0) {
      free_partial_vocab(&vocab);
      if (rules.rules)
        free(rules.rules);
      fclose(fp);
      return -1;
    }
    rules.rules[i].token1 = (int)t1;
    rules.rules[i].token2 = (int)t2;
    rules.rules[i].result_token = (int)res;
  }
  rules.num_rules = (int)num_rules;
  fclose(fp);

  *vocab_out = vocab;
  *rules_out = rules;
  return 0;
}

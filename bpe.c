#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
  uint8_t *bytes;
  int length;
} Token;

typedef struct {
  int token1;
  int token2;
  int count;
} Pair;

typedef struct {
  Token *tokens;
  int size;
  int capacity;
} Vocabulary;

typedef struct {
  int *tokens;
  int length;
  int capacity;
} TokenSequence;

typedef struct {
  int token1;
  int token2;
  int result_token;
} MergeRule;

typedef struct {
  MergeRule *rules;
  int num_rules;
  int capacity;
} MergeRules;

typedef struct {
  int token1;
  int token2;
  int count;
} PairCount;

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

void count_pairs(TokenSequence *seq, PairCount *pairs, int *num_pairs) {
  *num_pairs = 0;

  for (int i = 0; i < seq->length - 1; i++) {
    int t1 = seq->tokens[i];
    int t2 = seq->tokens[i + 1];

    // Check if we've seen this pair before
    int found = -1;
    for (int j = 0; j < *num_pairs; j++) {
      if (pairs[j].token1 == t1 && pairs[j].token2 == t2) {
        found = j;
        break;
      }
    }

    if (found >= 0) {
      // Increment existing pair
      pairs[found].count++;
    } else {
      // Add new pair
      pairs[*num_pairs].token1 = t1;
      pairs[*num_pairs].token2 = t2;
      pairs[*num_pairs].count = 1;
      (*num_pairs)++;
    }
  }
}

int find_best_pair(PairCount *pairs, int num_pairs) {
  if (num_pairs == 0) return -1;

  int best_idx = 0;
  int max_count = pairs[0].count;

  for (int i = 0; i < num_pairs; i++) {
    if (pairs[i].count > max_count) {
      max_count = pairs[i].count;
      best_idx = i;
    }
  }

  return best_idx;
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

uint8_t* read_file(const char *filename, int *file_size) {
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file: %s\n", filename);
    return NULL;
  }
  
  // Get file size
  fseek(file, 0, SEEK_END);
  *file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  // Allocate memory and read
  uint8_t *buffer = malloc(*file_size);
  if (buffer == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    fclose(file);
    return NULL;
  }
  
  fread(buffer, 1, *file_size, file);
  fclose(file);
  
  return buffer;
}

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


void free_pair_table(PairHashTable *table) {
  for (int i = 0; i < table->num_buckets; i++) {
    PairNode *node = table->buckets[i];
    while (node != NULL) {
      PairNode *next = node->next;
      free(node);
      node = next;
    }
  }
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
  for (int i = 0; i < seq->length; i++) {
    int t1 = seq->tokens[i];
    int t2 = seq->tokens[i+1];
    increment_pair(table, t1, t2);
  }
}

MergeRules create_merge_rules(int capacity) {
  MergeRules rules;
  rules.num_rules = 0;
  rules.capacity = capacity;
  rules.rules = malloc(sizeof(MergeRule) * capacity);
  if (rules.rules == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }
  return rules;
}

void free_merge_rules(MergeRules *rules) {
  free(rules->rules);
  rules->rules = NULL;
  rules->num_rules = 0;
}

void add_merge_rule(MergeRules *rules, int token1, int token2, int result) {
  if (rules->num_rules >= rules->capacity) {
    fprintf(stderr, "Merge rules capacity exceeded\n");
    exit(1);
  }
  rules->rules[rules->num_rules].token1 = token1;
  rules->rules[rules->num_rules].token2 = token2;
  rules->rules[rules->num_rules].result_token = result;
  rules->num_rules++;
}

void train_bpe(Vocabulary *vocab, TokenSequence *seq, int target_vocab_size, MergeRules *merge_rules) {
  printf("Starting BPE training...\n");
  printf("Initial vocab size: %d\n", vocab->size);
  printf("Target vocab size: %d\n", target_vocab_size);

  int initial_length = seq->length;

  while (vocab->size < target_vocab_size) {
    // Count all pairs using hash table
    PairHashTable table = create_pair_table(10000);
    count_pairs_hash(seq, &table);

    if (table.size == 0) {
      printf("No more pairs to merge!\n");
      free_pair_table(&table);
      break;
    }

    // Find best pair
    PairNode* best = find_best_pair_in_table(&table);
    if (best == NULL) {
      free_pair_table(&table);
      break;
    }

    // Create merged tokens
    Token merged = merge_tokens(&vocab->tokens[best->token1],
                                 &vocab->tokens[best->token2]);

    printf("Merge %d: ", vocab->size - 256);
    print_token(&vocab->tokens[best->token1]);
    printf(" + ");
    print_token(&vocab->tokens[best->token2]);
    printf(" → ");
    print_token(&merged);
    printf(" (count: %d)\n", best->count);

    // Add to vocabulary
    int new_idx = add_token(vocab, merged.bytes, merged.length);

    // Record the merge rule
    add_merge_rule(merge_rules, best->token1, best->token2, new_idx);

    // Update sequence
    merge_pair_in_sequence(seq, best->token1, best->token2, new_idx);

    // Clean up hash table
    free_pair_table(&table);
    free_token(&merged);
  }

  printf("\nTraining complete!\n");
  printf("Final vocab size: %d\n", vocab->size);
  printf("Final sequence length: %d\n", seq->length);
  printf("Initial sequence length: %d tokens\n", initial_length);
  printf("Compression ratio: %.2fx\n", (float)initial_length / seq->length);
  printf("Tokens reduced by: %d (%.1f%%)\n", 
         initial_length - seq->length,
         100.0 * (initial_length - seq->length) / initial_length);
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

int main() {
  printf("BPE Tokenizer\n\n");
  
  // Download the file first using curl or wget
  printf("Make sure you've downloaded tiny shakespeare:\n");
  printf("curl -O https://raw.githubusercontent.com/karpathy/char-rnn/master/data/tinyshakespeare/input.txt\n\n");
  
  // Initialize vocabulary
  int target_vocab_size = 4096;
  Vocabulary vocab = create_vocab(target_vocab_size);
  init_base_vocab(&vocab);
  
  // Read training data
  int text_len;
  uint8_t *text = read_file("input.txt", &text_len);
  if (text == NULL) {
    return 1;
  }
  
  printf("Loaded training text\n");
  printf("Text length: %d bytes\n\n", text_len);

  // From uint8_t to int
  TokenSequence seq = text_to_sequence(text, text_len);

  // Train BPE with target vocab size (+ record merge rules)
  MergeRules merge_rules = create_merge_rules(target_vocab_size - 256);
  train_bpe(&vocab, &seq, target_vocab_size, &merge_rules);
  
  printf("\n\nSome learned tokens:\n");
  for (int i = 256; i < vocab.size && i < 280; i++) {
    printf("Token %d: ", i);
    print_token(&vocab.tokens[i]);
    printf("\n");
  }

  printf("\nLearned %d merge rules\n", merge_rules.num_rules);
  
  // Test encoding new text
  printf("\n--- Testing Encode ---\n");
  uint8_t test_text[] = "To be or not to be, that is the question.";
  int test_len = strlen((char*)test_text);
  
  printf("Input text: %s\n", test_text);
  printf("Input length: %d bytes\n", test_len);
  
  TokenSequence encoded = encode(test_text, test_len, &merge_rules);
  
  printf("Encoded length: %d tokens\n", encoded.length);
  printf("Compression: %.2fx\n", (float)test_len / encoded.length);
  printf("\nEncoded tokens:\n");
  print_sequence(&encoded, &vocab);

  printf("\n--- Testing Decode ---\n");
  int decoded_len;
  uint8_t *decoded = decode(&encoded, &vocab, &decoded_len);

  printf("Decoded length: %d bytes\n", decoded_len);
  printf("Decoded text: ");
  for (int i = 0; i < decoded_len; i++) {
    printf("%c", decoded[i]);
  }
  printf("\n");

  // Verify it matches
  if (decoded_len == test_len && memcmp(decoded, test_text, test_len) == 0) {
    printf("✓ Decode successful - matches original!\n");
  } else {
    printf("✗ Decode mismatch!\n");
  }
  
  // Cleanup
  free(text);
  free_sequence(&seq);
  free_sequence(&encoded);
  free(decoded);
  free_merge_rules(&merge_rules);
  free_vocab(&vocab);

  return 0;
}


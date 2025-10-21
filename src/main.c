#include "cli.h"
#include "vocab.h"
#include "sequence.h"
#include "merge_rules.h"
#include "train.h"
#include "io.h"
#include "token.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  int target_vocab_size;
  const char *input_path;
  int parse_result = parse_cli_args(argc, argv, &target_vocab_size, &input_path);
  if (parse_result != 0) {
    return parse_result > 0 ? 0 : 1;
  }

  printf("BPE Tokenizer\n\n");
  printf("Training corpus: %s\n", input_path);
  printf("Target vocabulary size: %d\n\n", target_vocab_size);

  if (target_vocab_size < 256) {
    fprintf(stderr, "Target vocabulary must be at least 256 (base tokens).\n");
    return 1;
  }

  Vocabulary vocab = create_vocab(target_vocab_size);
  init_base_vocab(&vocab);

  // Read training data
  int text_len;
  uint8_t *text = read_file(input_path, &text_len);
  if (text == NULL) {
    fprintf(stderr, "Failed to load training data from %s\n", input_path);
    free_vocab(&vocab);
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

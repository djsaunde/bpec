#include "train.h"
#include "vocab.h"
#include "sequence.h"
#include "merge_rules.h"
#include "pair_stats.h"
#include "token.h"

#include <stdio.h>

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

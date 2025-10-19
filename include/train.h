#ifndef TRAIN_H
#define TRAIN_H

#include "vocab.h"
#include "sequence.h"
#include "merge_rules.h"

void train_bpe(Vocabulary *vocab, TokenSequence *seq, int target_vocab_size, MergeRules *merge_rules);

#endif  // TRAIN_H

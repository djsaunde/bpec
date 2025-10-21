#ifndef TOKENIZER_IO_H
#define TOKENIZER_IO_H

#include "merge_rules.h"
#include "vocab.h"

int save_tokenizer(const char *path, const Vocabulary *vocab, const MergeRules *rules);
int load_tokenizer(const char *path, Vocabulary *vocab_out, MergeRules *rules_out);

#endif  // TOKENIZER_IO_H

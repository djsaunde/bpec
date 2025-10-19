#ifndef MERGE_RULES_H
#define MERGE_RULES_H

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

MergeRules create_merge_rules(int capacity);
void free_merge_rules(MergeRules *rules);
void add_merge_rule(MergeRules *rules, int token1, int token2, int result);

#endif  // MERGE_RULES_H

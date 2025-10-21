#include "merge_rules.h"

#include <stdio.h>
#include <stdlib.h>

MergeRules create_merge_rules(int capacity) {
  MergeRules rules;
  rules.num_rules = 0;
  rules.capacity = capacity;
  if (capacity <= 0) {
    rules.rules = NULL;
    return rules;
  }
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
  if (rules->rules == NULL) {
    fprintf(stderr, "Merge rules storage not initialised\n");
    exit(1);
  }
  rules->rules[rules->num_rules].token1 = token1;
  rules->rules[rules->num_rules].token2 = token2;
  rules->rules[rules->num_rules].result_token = result;
  rules->num_rules++;
}

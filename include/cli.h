#ifndef CLI_H
#define CLI_H

typedef struct {
  int target_vocab_size;
  const char *input_path;
  const char *load_path;
  const char *save_path;
} CliOptions;

void print_usage(const char *progname);
int parse_cli_args(int argc, char **argv, CliOptions *options);

#endif  // CLI_H

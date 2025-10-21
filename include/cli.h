#ifndef CLI_H
#define CLI_H

int parse_cli_args(int argc, char **argv, int *target_vocab_size, const char **input_path);
void print_usage(const char *progname);

#endif  // CLI_H

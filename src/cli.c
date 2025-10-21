#include "cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage(const char *progname) {
  fprintf(stderr,
          "Usage: %s [options]\n\n"
          "Options:\n"
          "  -v, --vocab-size <N>   Target vocabulary size (default 512)\n"
          "  -i, --input <PATH>     Training text file (default input.txt)\n"
          "  -h, --help             Show this help message\n",
          progname);
}

static int parse_int(const char *value, int *out) {
  char *end;
  long v = strtol(value, &end, 10);
  if (*end != '\0' || v <= 0 || v > 1 << 20)
    return -1;
  *out = (int)v;
  return 0;
}

int parse_cli_args(int argc, char **argv, int *target_vocab_size, const char **input_path) {
  *target_vocab_size = 512;
  *input_path = "input.txt";

  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
      print_usage(argv[0]);
      return 1;
    } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--vocab-size") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: missing value for %s\n", arg);
        print_usage(argv[0]);
        return -1;
      }
      if (parse_int(argv[++i], target_vocab_size) != 0) {
        fprintf(stderr, "Error: invalid vocab size '%s'\n", argv[i]);
        print_usage(argv[0]);
        return -1;
      }
    } else if (strcmp(arg, "-i") == 0 || strcmp(arg, "--input") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: missing value for %s\n", arg);
        print_usage(argv[0]);
        return -1;
      }
      *input_path = argv[++i];
    } else if (strncmp(arg, "-", 1) == 0) {
      fprintf(stderr, "Error: unknown option '%s'\n", arg);
      print_usage(argv[0]);
      return -1;
    } else {
      *input_path = arg;
    }
  }

  if (*target_vocab_size < 256) {
    fprintf(stderr, "Error: target vocabulary size must be at least 256\n");
    return -1;
  }

  return 0;
}

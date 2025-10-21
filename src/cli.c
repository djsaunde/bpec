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
          "  -l, --load <FILE>      Load tokenizer (vocab + merges) from file\n"
          "  -s, --save <FILE>      Save tokenizer (vocab + merges) after training\n"
          "  -h, --help             Show this help message\n",
          progname);
}

static int parse_int(const char *value, int *out) {
  char *end;
  long v = strtol(value, &end, 10);
  if (*end != '\0' || v <= 0 || v > (1L << 24))
    return -1;
  *out = (int)v;
  return 0;
}

int parse_cli_args(int argc, char **argv, CliOptions *options) {
  options->target_vocab_size = 512;
  options->input_path = "input.txt";
  options->load_path = NULL;
  options->save_path = NULL;

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
      if (parse_int(argv[++i], &options->target_vocab_size) != 0) {
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
      options->input_path = argv[++i];
    } else if (strcmp(arg, "-l") == 0 || strcmp(arg, "--load") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: missing value for %s\n", arg);
        print_usage(argv[0]);
        return -1;
      }
      options->load_path = argv[++i];
    } else if (strcmp(arg, "-s") == 0 || strcmp(arg, "--save") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: missing value for %s\n", arg);
        print_usage(argv[0]);
        return -1;
      }
      options->save_path = argv[++i];
    } else if (strncmp(arg, "-", 1) == 0) {
      fprintf(stderr, "Error: unknown option '%s'\n", arg);
      print_usage(argv[0]);
      return -1;
    } else {
      options->input_path = arg;
    }
  }

  if (options->target_vocab_size < 256) {
    fprintf(stderr, "Error: target vocabulary size must be at least 256\n");
    return -1;
  }

  return 0;
}

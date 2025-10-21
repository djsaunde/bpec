#define _POSIX_C_SOURCE 200809L
#include "merge_rules.h"
#include "sequence.h"
#include "token.h"
#include "tokenizer_io.h"
#include "vocab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void trim_newline(char *line) {
  if (!line) return;
  size_t len = strlen(line);
  while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
    line[len - 1] = '\0';
    len--;
  }
}

static double elapsed_ms(struct timespec start, struct timespec end) {
  return (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1e6;
}

static void print_usage(const char *progname) {
  fprintf(stderr,
          "Usage: %s --load <tokenizer.bin>\n"
          "Starts an interactive tokenizer REPL.\n"
          "Commands:\n"
          "  quit/exit    Leave the session\n"
          "  :help        Show this message\n",
          progname);
}

int main(int argc, char **argv) {
  const char *load_path = NULL;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--load") == 0 && i + 1 < argc) {
      load_path = argv[++i];
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      print_usage(argv[0]);
      return 0;
    } else {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      print_usage(argv[0]);
      return 1;
    }
  }

  if (load_path == NULL) {
    fprintf(stderr, "Error: --load <file> is required.\n");
    print_usage(argv[0]);
    return 1;
  }

  Vocabulary vocab;
  MergeRules rules;
  if (load_tokenizer(load_path, &vocab, &rules) != 0) {
    fprintf(stderr, "Failed to load tokenizer from %s\n", load_path);
    return 1;
  }

  printf("Interactive tokenizer\n");
  printf("Loaded vocabulary size: %d\n", vocab.size);
  printf("Loaded merge rules: %d\n\n", rules.num_rules);
  printf("Type text to tokenize. Commands: quit, exit, :help.\n\n");

  char *line = NULL;
  size_t cap = 0;
  while (1) {
    printf("> ");
    fflush(stdout);
    ssize_t read = getline(&line, &cap, stdin);
    if (read < 0) {
      printf("\nEOF encountered, exiting.\n");
      break;
    }
    trim_newline(line);

    if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0)
      break;
    if (strcmp(line, ":help") == 0) {
      print_usage(argv[0]);
      continue;
    }
    if (line[0] == '\0')
      continue;

    size_t input_len = strlen(line);
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    TokenSequence encoded = encode((uint8_t *)line, (int)input_len, &rules);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    double encode_ms = elapsed_ms(t0, t1);

    printf("Tokens (%d): ", encoded.length);
    print_sequence(&encoded, &vocab);

    double compression = encoded.length > 0 ? ((double)input_len / encoded.length) : 0.0;
    printf("Length bytes: %zu\n", input_len);
    printf("Token count: %d\n", encoded.length);
    if (encoded.length > 0)
      printf("Compression ratio: %.3fx\n", compression);
    else
      printf("Compression ratio: N/A\n");

    printf("Encode time: %.3f ms\n", encode_ms);

    int decoded_len = 0;
    uint8_t *decoded = decode(&encoded, &vocab, &decoded_len);
    if (decoded) {
      int match = (decoded_len == (int)input_len) && memcmp(decoded, line, input_len) == 0;
      printf("Round-trip match: %s\n", match ? "yes" : "no");
      free(decoded);
    }

    free_sequence(&encoded);
    printf("\n");
  }

  free(line);
  free_merge_rules(&rules);
  free_vocab(&vocab);
  return 0;
}

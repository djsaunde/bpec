#include "io.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint8_t* read_file(const char *filename, int *file_size) {
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file: %s\n", filename);
    return NULL;
  }
  
  // Get file size
  fseek(file, 0, SEEK_END);
  *file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  // Allocate memory and read
  uint8_t *buffer = malloc(*file_size);
  if (buffer == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    fclose(file);
    return NULL;
  }
  
  fread(buffer, 1, *file_size, file);
  fclose(file);
  
  return buffer;
}
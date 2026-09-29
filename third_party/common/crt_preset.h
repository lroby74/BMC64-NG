/*
 * crt_preset.h
 *
 * This file is part of BMC64.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef BMC64_CRT_PRESET_H
#define BMC64_CRT_PRESET_H

#include <stddef.h>
#include <stdio.h>

#define CRT_PRESET_FORMAT_VERSION 1
#define CRT_PRESET_MAX_FIELDS 96
#define CRT_PRESET_RESULT_KEY_LEN 64

struct crt_preset_field {
  const char *key;
  int min;
  int max;
};

enum crt_preset_status {
  CRT_PRESET_OK = 0,
  CRT_PRESET_INVALID_ARGUMENT,
  CRT_PRESET_IO_ERROR,
  CRT_PRESET_LINE_TOO_LONG,
  CRT_PRESET_SYNTAX_ERROR,
  CRT_PRESET_INVALID_VALUE,
  CRT_PRESET_DUPLICATE_KEY,
  CRT_PRESET_MISSING_VERSION,
  CRT_PRESET_UNSUPPORTED_VERSION,
  CRT_PRESET_MISSING_FIELD
};

struct crt_preset_result {
  unsigned int line;
  unsigned int clamped_count;
  unsigned int unknown_count;
  char key[CRT_PRESET_RESULT_KEY_LEN];
  char first_clamped_key[CRT_PRESET_RESULT_KEY_LEN];
};

enum crt_preset_status crt_preset_parse(
    FILE *fp,
    const struct crt_preset_field *fields,
    size_t field_count,
    int *values,
    struct crt_preset_result *result);

const char *crt_preset_status_name(enum crt_preset_status status);

#endif

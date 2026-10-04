/*
 * crt_preset.c
 *
 * This file is part of BMC64.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "crt_preset.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define CRT_PRESET_LINE_LEN 512

static char *trim_left(char *text) {
  while (*text != '\0' && isspace((unsigned char)*text)) {
    ++text;
  }
  return text;
}

static void trim_right(char *text) {
  size_t length = strlen(text);
  while (length > 0 && isspace((unsigned char)text[length - 1])) {
    text[--length] = '\0';
  }
}

static char *trim(char *text) {
  text = trim_left(text);
  trim_right(text);
  return text;
}

static void copy_key(char *destination, const char *key) {
  if (key == NULL) {
    destination[0] = '\0';
    return;
  }

  strncpy(destination, key, CRT_PRESET_RESULT_KEY_LEN - 1);
  destination[CRT_PRESET_RESULT_KEY_LEN - 1] = '\0';
}

static enum crt_preset_status fail(
    struct crt_preset_result *result,
    enum crt_preset_status status,
    unsigned int line,
    const char *key) {
  result->line = line;
  copy_key(result->key, key);
  return status;
}

static int parse_integer(const char *text, long *value) {
  char *end = NULL;
  errno = 0;
  long parsed = strtol(text, &end, 10);
  if (end == text) {
    return 0;
  }

  while (*end != '\0' && isspace((unsigned char)*end)) {
    ++end;
  }
  if (*end != '\0') {
    return 0;
  }

  if (errno == ERANGE) {
    parsed = text[0] == '-' ? LONG_MIN : LONG_MAX;
  }
  *value = parsed;
  return 1;
}

static int find_field(const struct crt_preset_field *fields,
                      size_t field_count,
                      const char *key) {
  for (size_t i = 0; i < field_count; ++i) {
    if (strcmp(fields[i].key, key) == 0) {
      return (int)i;
    }
  }
  return -1;
}

enum crt_preset_status crt_preset_parse(
    FILE *fp,
    const struct crt_preset_field *fields,
    size_t field_count,
    int *values,
    struct crt_preset_result *result) {
  struct crt_preset_result local_result;
  int parsed_values[CRT_PRESET_MAX_FIELDS];
  unsigned char seen[CRT_PRESET_MAX_FIELDS];
  char line[CRT_PRESET_LINE_LEN];
  unsigned int line_number = 0;
  int version_seen = 0;

  if (result == NULL) {
    result = &local_result;
  }
  memset(result, 0, sizeof(*result));

  if (fp == NULL || fields == NULL || values == NULL || field_count == 0 ||
      field_count > CRT_PRESET_MAX_FIELDS) {
    return fail(result, CRT_PRESET_INVALID_ARGUMENT, 0, NULL);
  }

  memset(parsed_values, 0, sizeof(parsed_values));
  memset(seen, 0, sizeof(seen));

  while (fgets(line, sizeof(line), fp) != NULL) {
    ++line_number;

    if (strchr(line, '\n') == NULL && !feof(fp)) {
      int ch;
      while ((ch = fgetc(fp)) != '\n' && ch != EOF) {
      }
      return fail(result, CRT_PRESET_LINE_TOO_LONG, line_number, NULL);
    }

    char *text = line;
    if (line_number == 1 && strlen(text) >= 3 &&
        (unsigned char)text[0] == 0xef &&
        (unsigned char)text[1] == 0xbb &&
        (unsigned char)text[2] == 0xbf) {
      text += 3;
    }
    text = trim(text);
    if (*text == '\0' || *text == '#' || *text == ';') {
      continue;
    }

    char *comment = strpbrk(text, "#;");
    if (comment != NULL) {
      *comment = '\0';
      trim_right(text);
    }

    char *separator = strchr(text, '=');
    if (separator == NULL) {
      return fail(result, CRT_PRESET_SYNTAX_ERROR, line_number, text);
    }
    *separator = '\0';
    char *key = trim(text);
    char *value_text = trim(separator + 1);
    if (*key == '\0' || *value_text == '\0') {
      return fail(result, CRT_PRESET_SYNTAX_ERROR, line_number, key);
    }

    if (strcmp(key, "version") == 0) {
      if (version_seen) {
        return fail(result, CRT_PRESET_DUPLICATE_KEY, line_number, key);
      }
      long parsed_value;
      if (!parse_integer(value_text, &parsed_value)) {
        return fail(result, CRT_PRESET_INVALID_VALUE, line_number, key);
      }
      version_seen = 1;
      if (parsed_value != CRT_PRESET_FORMAT_VERSION) {
        return fail(result, CRT_PRESET_UNSUPPORTED_VERSION, line_number, key);
      }
      continue;
    }

    int field_index = find_field(fields, field_count, key);
    if (field_index < 0) {
      ++result->unknown_count;
      continue;
    }
    if (seen[field_index]) {
      return fail(result, CRT_PRESET_DUPLICATE_KEY, line_number, key);
    }
    seen[field_index] = 1;

    long parsed_value;
    if (!parse_integer(value_text, &parsed_value)) {
      return fail(result, CRT_PRESET_INVALID_VALUE, line_number, key);
    }

    int clamped_value;
    if (parsed_value < fields[field_index].min) {
      clamped_value = fields[field_index].min;
    } else if (parsed_value > fields[field_index].max) {
      clamped_value = fields[field_index].max;
    } else {
      clamped_value = (int)parsed_value;
    }
    if (parsed_value != clamped_value) {
      if (result->clamped_count == 0) {
        copy_key(result->first_clamped_key, key);
      }
      ++result->clamped_count;
    }
    parsed_values[field_index] = clamped_value;
  }

  if (ferror(fp)) {
    return fail(result, CRT_PRESET_IO_ERROR, line_number, NULL);
  }
  if (!version_seen) {
    return fail(result, CRT_PRESET_MISSING_VERSION, 0, "version");
  }
  for (size_t i = 0; i < field_count; ++i) {
    if (!seen[i]) {
      return fail(result, CRT_PRESET_MISSING_FIELD, 0, fields[i].key);
    }
  }

  memcpy(values, parsed_values, field_count * sizeof(values[0]));
  return CRT_PRESET_OK;
}

const char *crt_preset_status_name(enum crt_preset_status status) {
  switch (status) {
    case CRT_PRESET_OK:
      return "ok";
    case CRT_PRESET_INVALID_ARGUMENT:
      return "invalid-argument";
    case CRT_PRESET_IO_ERROR:
      return "io-error";
    case CRT_PRESET_LINE_TOO_LONG:
      return "line-too-long";
    case CRT_PRESET_SYNTAX_ERROR:
      return "syntax-error";
    case CRT_PRESET_INVALID_VALUE:
      return "invalid-value";
    case CRT_PRESET_DUPLICATE_KEY:
      return "duplicate-key";
    case CRT_PRESET_MISSING_VERSION:
      return "missing-version";
    case CRT_PRESET_UNSUPPORTED_VERSION:
      return "unsupported-version";
    case CRT_PRESET_MISSING_FIELD:
      return "missing-field";
    default:
      return "unknown";
  }
}

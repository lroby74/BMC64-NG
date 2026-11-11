#include "keymap_editor.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "keycodes.h"

static int normalized_target_flags(int flags) {
  return flags & (KEYMAP_EDITOR_VIRTUAL_SHIFT | KEYMAP_EDITOR_VIRTUAL_CBM |
                  KEYMAP_EDITOR_VIRTUAL_CTRL);
}

static int target_mapping_flags(int flags) {
  return flags & ~(KEYMAP_EDITOR_ALLOW_OTHER | KEYMAP_EDITOR_HOST_FLAG_MASK);
}

static struct keymap_editor_target target_for_binding(
    const struct keymap_editor_binding *binding) {
  struct keymap_editor_target target;
  target.row = binding->row;
  target.column = binding->column;
  target.flags = normalized_target_flags(binding->flags);
  target.mapping_flags = target_mapping_flags(binding->flags);
  return target;
}

int keymap_editor_find_target(const struct keymap_editor_model *model,
                              int row, int column, int flags) {
  struct keymap_editor_target target;
  size_t i;
  if (model == NULL) {
    return -1;
  }
  target.row = row;
  target.column = column;
  target.flags = normalized_target_flags(flags);
  target.mapping_flags = target_mapping_flags(flags);
  for (i = 0; i < model->target_count; ++i) {
    if (keymap_editor_target_equals(&model->targets[i], &target)) {
      return (int)i;
    }
  }
  return -1;
}

int keymap_editor_model_add_target(struct keymap_editor_model *model,
                                   int row, int column, int flags) {
  struct keymap_editor_target target;
  if (model == NULL) {
    return 0;
  }
  target.row = row;
  target.column = column;
  target.flags = normalized_target_flags(flags);
  target.mapping_flags = target_mapping_flags(flags);
  if (keymap_editor_find_target(model, row, column, flags) >= 0) {
    return 1;
  }
  if (model->target_count >= KEYMAP_EDITOR_MAX_TARGETS) {
    return 0;
  }
  model->targets[model->target_count++] = target;
  return 1;
}

int keymap_editor_has_target_position(const struct keymap_editor_model *model,
                                      int row, int column) {
  size_t i;
  if (model == NULL) {
    return 0;
  }
  for (i = 0; i < model->target_count; ++i) {
    if (model->targets[i].row == row && model->targets[i].column == column &&
        (model->targets[i].flags &
         (KEYMAP_EDITOR_VIRTUAL_SHIFT | KEYMAP_EDITOR_VIRTUAL_CBM |
          KEYMAP_EDITOR_VIRTUAL_CTRL)) == 0) {
      return 1;
    }
  }
  return 0;
}

int keymap_editor_target_equals(const struct keymap_editor_target *a,
                                const struct keymap_editor_target *b) {
  return a != NULL && b != NULL && a->row == b->row &&
         a->column == b->column && a->flags == b->flags;
}

int keymap_editor_binding_targets(
    const struct keymap_editor_binding *binding,
    const struct keymap_editor_target *target) {
  struct keymap_editor_target actual;
  if (binding == NULL || target == NULL) {
    return 0;
  }
  actual = target_for_binding(binding);
  return keymap_editor_target_equals(&actual, target);
}

static int appendf(char *buffer, size_t buffer_size, size_t *used,
                   const char *format, ...) {
  int result;
  va_list args;
  if (*used >= buffer_size) {
    return 0;
  }
  va_start(args, format);
  result = vsnprintf(buffer + *used, buffer_size - *used, format, args);
  va_end(args);
  if (result < 0 || (size_t)result >= buffer_size - *used) {
    return 0;
  }
  *used += (size_t)result;
  return 1;
}

static int append_undef_key(struct keymap_editor_model *model, long keycode) {
  size_t i;
  for (i = 0; i < model->undef_count; ++i) {
    if (model->undef_keys[i] == keycode) {
      return 1;
    }
  }
  if (model->undef_count >= KEYMAP_EDITOR_MAX_UNDEF_KEYS) {
    return 0;
  }
  model->undef_keys[model->undef_count++] = keycode;
  return 1;
}

int keymap_editor_model_init(
    struct keymap_editor_model *model,
    const struct keymap_editor_binding *bindings,
    size_t binding_count) {
  size_t i;
  if (model == NULL || (bindings == NULL && binding_count != 0) ||
      binding_count > KEYMAP_EDITOR_MAX_BINDINGS) {
    return 0;
  }
  memset(model, 0, sizeof *model);
  for (i = 0; i < binding_count; ++i) {
    if (bindings[i].keycode <= 0 ||
        !append_undef_key(model, bindings[i].keycode)) {
      return 0;
    }
    model->bindings[model->binding_count++] = bindings[i];
    if (!keymap_editor_model_add_target(
            model, bindings[i].row, bindings[i].column, bindings[i].flags)) {
      return 0;
    }
  }
  return 1;
}

static int target_list_contains(const struct keymap_editor_target *targets,
                                size_t count,
                                const struct keymap_editor_target *target) {
  size_t i;
  for (i = 0; i < count; ++i) {
    if (keymap_editor_target_equals(&targets[i], target)) {
      return 1;
    }
  }
  return 0;
}

static int target_sort_before(const struct keymap_editor_target *a,
                              const struct keymap_editor_target *b) {
  if (a->flags != b->flags) return a->flags < b->flags;
  if (a->row != b->row) return a->row < b->row;
  if (a->column != b->column) return a->column < b->column;
  return a->mapping_flags < b->mapping_flags;
}

static int target_intrinsic_mapping_flags(int flags) {
  return flags & (KEYMAP_EDITOR_LEFT_SHIFT | KEYMAP_EDITOR_RIGHT_SHIFT |
                  KEYMAP_EDITOR_SHIFT_LOCK | KEYMAP_EDITOR_LEFT_CBM |
                  KEYMAP_EDITOR_LEFT_CTRL | KEYMAP_EDITOR_NO_LOCK);
}

int keymap_editor_model_order_targets(
    struct keymap_editor_model *model,
    const struct keymap_editor_target *catalog, size_t catalog_count) {
  struct keymap_editor_target original[KEYMAP_EDITOR_MAX_TARGETS];
  struct keymap_editor_target ordered[KEYMAP_EDITOR_MAX_TARGETS];
  size_t original_count;
  size_t ordered_count = 0;
  size_t i;

  if (model == NULL || (catalog == NULL && catalog_count != 0) ||
      catalog_count > KEYMAP_EDITOR_MAX_TARGETS) {
    return 0;
  }
  original_count = model->target_count;
  memcpy(original, model->targets,
         original_count * sizeof original[0]);

  for (i = 0; i < catalog_count; ++i) {
    size_t existing;
    struct keymap_editor_target target = catalog[i];
    target.flags = normalized_target_flags(target.flags);
    target.mapping_flags = target_mapping_flags(target.mapping_flags);
    if (target_list_contains(ordered, ordered_count, &target)) {
      continue;
    }
    for (existing = 0; existing < original_count; ++existing) {
      if (keymap_editor_target_equals(&original[existing], &target)) {
        /* The machine catalog owns the default behavior of a logical key.
           Source-specific DESHIFT/ALLOW_OTHER flags from the first active
           binding must not become defaults for future assignments.  Real
           modifier and special targets are not fully described by the
           matrix catalog, so retain their intrinsic behavior. */
        if ((target.flags & (KEYMAP_EDITOR_VIRTUAL_SHIFT |
                             KEYMAP_EDITOR_VIRTUAL_CBM |
                             KEYMAP_EDITOR_VIRTUAL_CTRL)) == 0 &&
            target_intrinsic_mapping_flags(
                original[existing].mapping_flags) != 0) {
          target.mapping_flags = original[existing].mapping_flags &
                                 ~KEYMAP_EDITOR_DESHIFT;
        }
        break;
      }
    }
    if (ordered_count >= KEYMAP_EDITOR_MAX_TARGETS) {
      return 0;
    }
    ordered[ordered_count++] = target;
  }

  for (;;) {
    size_t best = original_count;
    for (i = 0; i < original_count; ++i) {
      if (target_list_contains(ordered, ordered_count, &original[i])) {
        continue;
      }
      if (best == original_count ||
          target_sort_before(&original[i], &original[best])) {
        best = i;
      }
    }
    if (best == original_count) {
      break;
    }
    if (ordered_count >= KEYMAP_EDITOR_MAX_TARGETS) {
      return 0;
    }
    ordered[ordered_count++] = original[best];
  }

  memcpy(model->targets, ordered, ordered_count * sizeof ordered[0]);
  model->target_count = ordered_count;
  return 1;
}

size_t keymap_editor_target_binding_count(
    const struct keymap_editor_model *model, size_t target_index) {
  size_t count = 0;
  size_t i;
  if (model == NULL || target_index >= model->target_count) {
    return 0;
  }
  for (i = 0; i < model->binding_count; ++i) {
    if (keymap_editor_binding_targets(&model->bindings[i],
                                      &model->targets[target_index])) {
      ++count;
    }
  }
  return count;
}

const struct keymap_editor_binding *keymap_editor_target_binding(
    const struct keymap_editor_model *model, size_t target_index,
    size_t binding_index) {
  size_t found = 0;
  size_t i;
  if (model == NULL || target_index >= model->target_count) {
    return NULL;
  }
  for (i = 0; i < model->binding_count; ++i) {
    if (keymap_editor_binding_targets(&model->bindings[i],
                                      &model->targets[target_index])) {
      if (found++ == binding_index) {
        return &model->bindings[i];
      }
    }
  }
  return NULL;
}

static int source_flags(int flags) {
  return flags & KEYMAP_EDITOR_HOST_FLAG_MASK;
}

int keymap_editor_find_conflict(const struct keymap_editor_model *model,
                                size_t target_index, long keycode,
                                int host_flags) {
  size_t i;
  if (model == NULL || target_index >= model->target_count) {
    return -1;
  }
  host_flags &= KEYMAP_EDITOR_HOST_FLAG_MASK;
  for (i = 0; i < model->binding_count; ++i) {
    size_t target;
    if (model->bindings[i].keycode != keycode ||
        source_flags(model->bindings[i].flags) != host_flags) {
      continue;
    }
    for (target = 0; target < model->target_count; ++target) {
      if (target != target_index && keymap_editor_binding_targets(
              &model->bindings[i], &model->targets[target])) {
        return (int)target;
      }
    }
  }
  return -1;
}

static void remove_binding(struct keymap_editor_model *model, size_t index) {
  if (index + 1 < model->binding_count) {
    memmove(&model->bindings[index], &model->bindings[index + 1],
            (model->binding_count - index - 1) * sizeof model->bindings[0]);
  }
  --model->binding_count;
}

static size_t target_binding_model_index(
    const struct keymap_editor_model *model, size_t target_index,
    size_t binding_index) {
  size_t found = 0;
  size_t i;
  if (model == NULL || target_index >= model->target_count) {
    return model != NULL ? model->binding_count : 0;
  }
  for (i = 0; i < model->binding_count; ++i) {
    if (keymap_editor_binding_targets(&model->bindings[i],
                                      &model->targets[target_index]) &&
        found++ == binding_index) {
      return i;
    }
  }
  return model->binding_count;
}

static int binding_source_equals(const struct keymap_editor_binding *binding,
                                 long keycode, int host_flags) {
  return binding->keycode == keycode &&
         source_flags(binding->flags) == host_flags;
}

static struct keymap_editor_binding make_binding(
    const struct keymap_editor_target *target, long keycode, int host_flags) {
  struct keymap_editor_binding binding;
  binding.keycode = keycode;
  binding.row = target->row;
  binding.column = target->column;
  binding.flags = target->mapping_flags;
  if ((host_flags & KEYMAP_EDITOR_MAP_MOD_SHIFT) != 0 &&
      (target->flags & KEYMAP_EDITOR_VIRTUAL_SHIFT) == 0 &&
      (binding.flags & (KEYMAP_EDITOR_LEFT_SHIFT |
                        KEYMAP_EDITOR_RIGHT_SHIFT |
                        KEYMAP_EDITOR_SHIFT_LOCK)) == 0) {
    binding.flags &= ~KEYMAP_EDITOR_ALLOW_SHIFT;
    binding.flags |= KEYMAP_EDITOR_DESHIFT;
  }
  binding.flags |= host_flags;
  return binding;
}

int keymap_editor_add_binding(struct keymap_editor_model *model,
                              size_t target_index, long keycode,
                              int host_flags) {
  size_t i = 0;
  size_t removable = 0;
  if (model == NULL || target_index >= model->target_count || keycode <= 0 ||
      (host_flags & ~KEYMAP_EDITOR_HOST_FLAG_MASK) != 0) {
    return 0;
  }
  host_flags &= KEYMAP_EDITOR_HOST_FLAG_MASK;
  for (i = 0; i < model->binding_count; ++i) {
    if (binding_source_equals(&model->bindings[i], keycode, host_flags)) {
      ++removable;
    }
  }
  if (model->binding_count - removable >= KEYMAP_EDITOR_MAX_BINDINGS ||
      !append_undef_key(model, keycode)) {
    return 0;
  }
  i = 0;
  while (i < model->binding_count) {
    if (binding_source_equals(&model->bindings[i], keycode, host_flags)) {
      remove_binding(model, i);
    } else {
      ++i;
    }
  }
  model->bindings[model->binding_count++] = make_binding(
      &model->targets[target_index], keycode, host_flags);
  return 1;
}

int keymap_editor_replace_binding(struct keymap_editor_model *model,
                                  size_t target_index, size_t binding_index,
                                  long keycode, int host_flags) {
  size_t selected;
  size_t i = 0;
  if (model == NULL || target_index >= model->target_count || keycode <= 0 ||
      (host_flags & ~KEYMAP_EDITOR_HOST_FLAG_MASK) != 0) {
    return 0;
  }
  selected = target_binding_model_index(model, target_index, binding_index);
  if (selected >= model->binding_count || !append_undef_key(model, keycode)) {
    return 0;
  }
  host_flags &= KEYMAP_EDITOR_HOST_FLAG_MASK;
  while (i < model->binding_count) {
    if (i != selected &&
        binding_source_equals(&model->bindings[i], keycode, host_flags)) {
      remove_binding(model, i);
      if (i < selected) {
        --selected;
      }
    } else {
      ++i;
    }
  }
  model->bindings[selected] = make_binding(
      &model->targets[target_index], keycode, host_flags);
  return 1;
}

int keymap_editor_remove_binding(struct keymap_editor_model *model,
                                 size_t target_index, size_t binding_index) {
  size_t index = target_binding_model_index(model, target_index,
                                            binding_index);
  if (model == NULL || index >= model->binding_count) {
    return 0;
  }
  remove_binding(model, index);
  return 1;
}

int keymap_editor_assign(struct keymap_editor_model *model,
                         size_t target_index, long keycode, int host_flags) {
  size_t i = 0;
  size_t removable = 0;
  if (model == NULL || target_index >= model->target_count || keycode <= 0 ||
      (host_flags & ~KEYMAP_EDITOR_HOST_FLAG_MASK) != 0) {
    return 0;
  }
  for (i = 0; i < model->binding_count; ++i) {
    if (keymap_editor_binding_targets(&model->bindings[i],
                                      &model->targets[target_index]) ||
        (model->bindings[i].keycode == keycode &&
         source_flags(model->bindings[i].flags) == host_flags)) {
      ++removable;
    }
  }
  if (model->binding_count - removable >= KEYMAP_EDITOR_MAX_BINDINGS ||
      !append_undef_key(model, keycode)) {
    return 0;
  }
  i = 0;
  while (i < model->binding_count) {
    if (keymap_editor_binding_targets(&model->bindings[i],
                                      &model->targets[target_index]) ||
        (model->bindings[i].keycode == keycode &&
         source_flags(model->bindings[i].flags) == host_flags)) {
      remove_binding(model, i);
    } else {
      ++i;
    }
  }
  model->bindings[model->binding_count++] = make_binding(
      &model->targets[target_index], keycode, host_flags);
  return 1;
}

int keymap_editor_clear(struct keymap_editor_model *model,
                        size_t target_index) {
  size_t i = 0;
  int removed = 0;
  if (model == NULL || target_index >= model->target_count) {
    return 0;
  }
  while (i < model->binding_count) {
    if (keymap_editor_binding_targets(&model->bindings[i],
                                      &model->targets[target_index])) {
      remove_binding(model, i);
      removed = 1;
    } else {
      ++i;
    }
  }
  return removed;
}

int keymap_editor_format_host_binding(long keycode, int flags,
                                      char *buffer, size_t buffer_size) {
  const char *key;
  size_t used = 0;
  if (buffer == NULL || buffer_size == 0) {
    return 0;
  }
  buffer[0] = '\0';
  if (flags & KEYMAP_EDITOR_MAP_MOD_CTRL) {
    if (!appendf(buffer, buffer_size, &used, "Ctrl+")) return 0;
  }
  if (flags & KEYMAP_EDITOR_MAP_MOD_SHIFT) {
    if (!appendf(buffer, buffer_size, &used, "Shift+")) return 0;
  }
  if (flags & KEYMAP_EDITOR_MAP_MOD_RIGHT_ALT) {
    if (!appendf(buffer, buffer_size, &used, "AltGr+")) return 0;
  }
  if (flags & KEYMAP_EDITOR_ALT_MAP) {
    if (!appendf(buffer, buffer_size, &used, "AltMap+")) return 0;
  }
  key = keycode_to_string(keycode);
  if (key == NULL || *key == '\0' || strcmp(key, "undefined") == 0) {
    char token[24];
    if (!keycode_format_vkm_token(keycode, token, sizeof token)) return 0;
    return appendf(buffer, buffer_size, &used, "%s", token);
  }
  return appendf(buffer, buffer_size, &used, "%s", key);
}

int keymap_editor_format_host_binding_for_layout(
    long keycode, int flags, int german_layout,
    char *buffer, size_t buffer_size) {
  char key[32];
  size_t used = 0;
  if (buffer == NULL || buffer_size == 0) {
    return 0;
  }
  buffer[0] = '\0';
  if (flags & KEYMAP_EDITOR_MAP_MOD_CTRL) {
    if (!appendf(buffer, buffer_size, &used, "Ctrl+")) return 0;
  }
  if (flags & KEYMAP_EDITOR_MAP_MOD_SHIFT) {
    if (!appendf(buffer, buffer_size, &used, "Shift+")) return 0;
  }
  if (flags & KEYMAP_EDITOR_MAP_MOD_RIGHT_ALT) {
    if (!appendf(buffer, buffer_size, &used, "AltGr+")) return 0;
  }
  if (flags & KEYMAP_EDITOR_ALT_MAP) {
    if (!appendf(buffer, buffer_size, &used, "AltMap+")) return 0;
  }
  if (!keycode_format_keycap(keycode, german_layout, key, sizeof key)) {
    return 0;
  }
  return appendf(buffer, buffer_size, &used, "%s", key);
}

static unsigned source_specificity(const struct keymap_editor_binding *binding) {
  unsigned flags = (unsigned)source_flags(binding->flags);
  unsigned count = 0;
  while (flags != 0) {
    count += flags & 1U;
    flags >>= 1;
  }
  return count;
}

int keymap_editor_serialize_block(const struct keymap_editor_model *model,
                                  char *buffer, size_t buffer_size,
                                  size_t *written) {
  size_t used = 0;
  size_t i;
  unsigned char emitted[KEYMAP_EDITOR_MAX_BINDINGS];
  if (model == NULL || buffer == NULL || buffer_size == 0) return 0;
  memset(emitted, 0, sizeof emitted);
  if (!appendf(buffer, buffer_size, &used, "%s\n", KEYMAP_EDITOR_BLOCK_BEGIN) ||
      !appendf(buffer, buffer_size, &used,
               "# Generated by BMX. Manual edits outside this block are preserved.\n") ||
      !appendf(buffer, buffer_size, &used, "%s%d\n",
               KEYMAP_EDITOR_BLOCK_VERSION_PREFIX,
               KEYMAP_EDITOR_BLOCK_VERSION)) {
    return 0;
  }
  for (i = 0; i < model->target_count; ++i) {
    if (!appendf(buffer, buffer_size, &used, "# BMX-TARGET %d %d %d\n",
                 model->targets[i].row, model->targets[i].column,
                 model->targets[i].mapping_flags)) {
      return 0;
    }
  }
  if (!appendf(buffer, buffer_size, &used, "\n")) return 0;
  for (i = 0; i < model->undef_count; ++i) {
    char token[24];
    if (!keycode_format_vkm_token(model->undef_keys[i], token, sizeof token) ||
        !appendf(buffer, buffer_size, &used, "!UNDEF %s\n", token)) return 0;
  }
  if (!appendf(buffer, buffer_size, &used, "\n")) return 0;
  for (i = 0; i < model->binding_count; ++i) {
    size_t j;
    long keycode;
    if (emitted[i]) continue;
    keycode = model->bindings[i].keycode;
    for (;;) {
      size_t best = model->binding_count;
      unsigned best_specificity = ~0U;
      int later = 0;
      for (j = 0; j < model->binding_count; ++j) {
        unsigned specificity;
        if (emitted[j] || model->bindings[j].keycode != keycode) continue;
        specificity = source_specificity(&model->bindings[j]);
        if (best == model->binding_count || specificity < best_specificity) {
          best = j;
          best_specificity = specificity;
        }
      }
      if (best == model->binding_count) break;
      for (j = 0; j < model->binding_count; ++j) {
        if (!emitted[j] && j != best && model->bindings[j].keycode == keycode) {
          later = 1;
          break;
        }
      }
      {
        char token[24];
        int flags = model->bindings[best].flags;
        if (later) flags |= KEYMAP_EDITOR_ALLOW_OTHER;
        else flags &= ~KEYMAP_EDITOR_ALLOW_OTHER;
        if (!keycode_format_vkm_token(keycode, token, sizeof token) ||
            !appendf(buffer, buffer_size, &used, "%s %d %d %d\n", token,
                     model->bindings[best].row, model->bindings[best].column,
                     flags)) return 0;
      }
      emitted[best] = 1;
    }
  }
  if (!appendf(buffer, buffer_size, &used, "%s\n", KEYMAP_EDITOR_BLOCK_END)) {
    return 0;
  }
  if (written != NULL) *written = used;
  return 1;
}

static const char *find_marker_line(const char *start, const char *end,
                                    const char *marker) {
  const char *found = start;
  size_t marker_size = strlen(marker);
  while ((found = strstr(found, marker)) != NULL && found < end) {
    const char *after = found + marker_size;
    if ((found == start || found[-1] == '\n') &&
        (after == end || *after == '\r' || *after == '\n')) {
      return found;
    }
    found += marker_size;
  }
  return NULL;
}

static const char *find_after_line(const char *start, const char *end,
                                   const char *marker) {
  const char *found = find_marker_line(start, end, marker);
  const char *line_end;
  if (found == NULL) return NULL;
  line_end = strchr(found, '\n');
  return line_end == NULL || line_end >= end ? end : line_end + 1;
}

int keymap_editor_replace_block(const char *input, size_t input_size,
                                const char *block, size_t block_size,
                                char *output, size_t output_size,
                                size_t *written) {
  const char *end;
  const char *begin;
  const char *suffix;
  size_t prefix_size;
  size_t suffix_size;
  size_t used = 0;
  if (input == NULL || block == NULL || output == NULL ||
      input[input_size] != '\0') return 0;
  end = input + input_size;
  begin = find_marker_line(input, end, KEYMAP_EDITOR_BLOCK_BEGIN);
  suffix = begin == NULL ? end : find_after_line(begin, end, KEYMAP_EDITOR_BLOCK_END);
  if (begin != NULL && suffix == NULL) return 0;
  prefix_size = begin == NULL ? input_size : (size_t)(begin - input);
  suffix_size = begin == NULL ? 0 : (size_t)(end - suffix);
  while (prefix_size > 0 && (input[prefix_size - 1] == '\n' ||
                             input[prefix_size - 1] == '\r')) --prefix_size;
  while (suffix_size > 0 && (*suffix == '\n' || *suffix == '\r')) {
    ++suffix;
    --suffix_size;
  }
  if (prefix_size + (prefix_size ? 2 : 0) + suffix_size +
      (suffix_size ? 2 : 0) + block_size + 1 > output_size) return 0;
  if (prefix_size) {
    memcpy(output + used, input, prefix_size);
    used += prefix_size;
    output[used++] = '\n';
    output[used++] = '\n';
  }
  if (suffix_size) {
    memcpy(output + used, suffix, suffix_size);
    used += suffix_size;
    output[used++] = '\n';
    output[used++] = '\n';
  }
  memcpy(output + used, block, block_size);
  used += block_size;
  output[used] = '\0';
  if (written != NULL) *written = used;
  return 1;
}

int keymap_editor_merge_target_catalog(struct keymap_editor_model *model,
                                       const char *input, size_t input_size) {
  static const char target_prefix[] = "# BMX-TARGET ";
  const char *input_end;
  const char *block_begin;
  const char *block_end;
  const char *line;
  int block_version = 1;
  int version_seen = 0;
  if (model == NULL || input == NULL || input[input_size] != '\0') {
    return 0;
  }
  input_end = input + input_size;
  block_begin = find_marker_line(input, input_end, KEYMAP_EDITOR_BLOCK_BEGIN);
  if (block_begin == NULL) {
    return 1;
  }
  block_end = find_marker_line(block_begin, input_end,
                               KEYMAP_EDITOR_BLOCK_END);
  line = find_after_line(block_begin, input_end, KEYMAP_EDITOR_BLOCK_BEGIN);
  if (block_end == NULL || line == NULL) {
    return 0;
  }
  {
    const char *version_line = line;
    while (version_line < block_end) {
      const char *version_line_end = strchr(version_line, '\n');
      int version;
      int consumed = 0;
      const char *tail;
      if (version_line_end == NULL || version_line_end > block_end) {
        version_line_end = block_end;
      }
      if ((size_t)(version_line_end - version_line) >=
              sizeof KEYMAP_EDITOR_BLOCK_VERSION_PREFIX - 1 &&
          strncmp(version_line, KEYMAP_EDITOR_BLOCK_VERSION_PREFIX,
                  sizeof KEYMAP_EDITOR_BLOCK_VERSION_PREFIX - 1) == 0) {
        if (version_seen ||
            sscanf(version_line +
                       sizeof KEYMAP_EDITOR_BLOCK_VERSION_PREFIX - 1,
                   "%d%n", &version, &consumed) != 1 ||
            version < 1 || version > KEYMAP_EDITOR_BLOCK_VERSION) {
          return 0;
        }
        tail = version_line +
               sizeof KEYMAP_EDITOR_BLOCK_VERSION_PREFIX - 1 + consumed;
        while (tail < version_line_end &&
               (*tail == ' ' || *tail == '\t' || *tail == '\r')) {
          ++tail;
        }
        if (tail != version_line_end) {
          return 0;
        }
        block_version = version;
        version_seen = 1;
      }
      version_line = version_line_end < block_end
                         ? version_line_end + 1
                         : block_end;
    }
  }
  while (line < block_end) {
    const char *line_end = strchr(line, '\n');
    int row;
    int column;
    int flags;
    int consumed = 0;
    const char *tail;
    if (line_end == NULL || line_end > block_end) {
      line_end = block_end;
    }
    if ((size_t)(line_end - line) >= sizeof target_prefix - 1 &&
        strncmp(line, target_prefix, sizeof target_prefix - 1) == 0) {
      if (sscanf(line + sizeof target_prefix - 1, "%d %d %d%n",
                 &row, &column, &flags, &consumed) != 3) {
        return 0;
      }
      tail = line + sizeof target_prefix - 1 + consumed;
      while (tail < line_end && (*tail == ' ' || *tail == '\t' ||
                                 *tail == '\r')) {
        ++tail;
      }
      if (tail != line_end ||
          !keymap_editor_model_add_target(model, row, column, flags)) {
        return 0;
      }
    } else if (block_version < KEYMAP_EDITOR_BLOCK_VERSION &&
               line < line_end && *line != '#' && *line != '!') {
      char entry[128];
      char token[24];
      size_t entry_size = (size_t)(line_end - line);
      int parsed;
      int entry_consumed = 0;
      long keycode;
      size_t binding;
      if (entry_size >= sizeof entry) {
        return 0;
      }
      memcpy(entry, line, entry_size);
      entry[entry_size] = '\0';
      parsed = sscanf(entry, "%23s %d %d %d%n", token, &row, &column,
                      &flags, &entry_consumed);
      while (entry[entry_consumed] == ' ' ||
             entry[entry_consumed] == '\t' ||
             entry[entry_consumed] == '\r') {
        ++entry_consumed;
      }
      keycode = parsed == 4 && entry[entry_consumed] == '\0'
                    ? keycode_from_vkm_token(token)
                    : -1;
      /* Version 1 could accidentally copy DESHIFT from a stock shifted
         binding into an ordinary unmodified target.  This exact shape was
         not expressible intentionally in the editor; migrate only matching
         generated entries and leave manual/compound DESHIFT mappings alone. */
      if (keycode > 0 && row >= 0 &&
          (flags & ~KEYMAP_EDITOR_ALLOW_OTHER) == KEYMAP_EDITOR_DESHIFT) {
        for (binding = 0; binding < model->binding_count; ++binding) {
          if (model->bindings[binding].keycode == keycode &&
              model->bindings[binding].row == row &&
              model->bindings[binding].column == column &&
              (model->bindings[binding].flags &
               ~KEYMAP_EDITOR_ALLOW_OTHER) == KEYMAP_EDITOR_DESHIFT) {
            model->bindings[binding].flags &= ~KEYMAP_EDITOR_DESHIFT;
            model->bindings[binding].flags |= KEYMAP_EDITOR_ALLOW_SHIFT;
            break;
          }
        }
      }
    }
    line = line_end < block_end ? line_end + 1 : block_end;
  }
  return 1;
}

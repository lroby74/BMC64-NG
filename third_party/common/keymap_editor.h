#ifndef BMX_KEYMAP_EDITOR_H
#define BMX_KEYMAP_EDITOR_H

#include <stddef.h>

#define KEYMAP_EDITOR_MAX_BINDINGS 192
#define KEYMAP_EDITOR_MAX_TARGETS 128
#define KEYMAP_EDITOR_MAX_UNDEF_KEYS 160

#define KEYMAP_EDITOR_VIRTUAL_SHIFT 1
#define KEYMAP_EDITOR_LEFT_SHIFT 2
#define KEYMAP_EDITOR_RIGHT_SHIFT 4
#define KEYMAP_EDITOR_ALLOW_SHIFT 8
#define KEYMAP_EDITOR_DESHIFT 16
#define KEYMAP_EDITOR_ALLOW_OTHER 32
#define KEYMAP_EDITOR_SHIFT_LOCK 64
#define KEYMAP_EDITOR_MAP_MOD_SHIFT 128
#define KEYMAP_EDITOR_ALT_MAP 256
#define KEYMAP_EDITOR_MAP_MOD_RIGHT_ALT 512
#define KEYMAP_EDITOR_MAP_MOD_CTRL 1024
#define KEYMAP_EDITOR_VIRTUAL_CBM 2048
#define KEYMAP_EDITOR_VIRTUAL_CTRL 4096
#define KEYMAP_EDITOR_LEFT_CBM 8192
#define KEYMAP_EDITOR_LEFT_CTRL 16384
#define KEYMAP_EDITOR_NO_LOCK 32768
#define KEYMAP_EDITOR_HOST_FLAG_MASK \
  (KEYMAP_EDITOR_MAP_MOD_SHIFT | KEYMAP_EDITOR_ALT_MAP | \
   KEYMAP_EDITOR_MAP_MOD_RIGHT_ALT | KEYMAP_EDITOR_MAP_MOD_CTRL)

#define KEYMAP_EDITOR_BLOCK_BEGIN "# BMX-KEYMAP-EDITOR-BEGIN"
#define KEYMAP_EDITOR_BLOCK_VERSION 2
#define KEYMAP_EDITOR_BLOCK_VERSION_PREFIX "# BMX-KEYMAP-EDITOR-VERSION "
#define KEYMAP_EDITOR_BLOCK_END "# BMX-KEYMAP-EDITOR-END"

struct keymap_editor_binding {
  long keycode;
  int row;
  int column;
  int flags;
};

struct keymap_editor_target {
  int row;
  int column;
  /* Flags that change the logical emulated key (virtual Shift/CBM/Ctrl). */
  int flags;
  /* VICE behavior flags used when creating a mapping for this target. */
  int mapping_flags;
};

struct keymap_editor_model {
  struct keymap_editor_binding bindings[KEYMAP_EDITOR_MAX_BINDINGS];
  struct keymap_editor_target targets[KEYMAP_EDITOR_MAX_TARGETS];
  long undef_keys[KEYMAP_EDITOR_MAX_UNDEF_KEYS];
  size_t binding_count;
  size_t target_count;
  size_t undef_count;
};

int keymap_editor_model_init(
    struct keymap_editor_model *model,
    const struct keymap_editor_binding *bindings,
    size_t binding_count);
int keymap_editor_model_add_target(struct keymap_editor_model *model,
                                   int row, int column, int flags);
int keymap_editor_model_order_targets(
    struct keymap_editor_model *model,
    const struct keymap_editor_target *catalog, size_t catalog_count);
int keymap_editor_find_target(const struct keymap_editor_model *model,
                              int row, int column, int flags);
int keymap_editor_has_target_position(const struct keymap_editor_model *model,
                                      int row, int column);
int keymap_editor_target_equals(const struct keymap_editor_target *a,
                                const struct keymap_editor_target *b);
int keymap_editor_binding_targets(
    const struct keymap_editor_binding *binding,
    const struct keymap_editor_target *target);
size_t keymap_editor_target_binding_count(
    const struct keymap_editor_model *model, size_t target_index);
const struct keymap_editor_binding *keymap_editor_target_binding(
    const struct keymap_editor_model *model, size_t target_index,
    size_t binding_index);
int keymap_editor_find_conflict(const struct keymap_editor_model *model,
                                size_t target_index, long keycode,
                                int host_flags);
int keymap_editor_assign(struct keymap_editor_model *model,
                         size_t target_index, long keycode, int host_flags);
int keymap_editor_add_binding(struct keymap_editor_model *model,
                              size_t target_index, long keycode,
                              int host_flags);
int keymap_editor_replace_binding(struct keymap_editor_model *model,
                                  size_t target_index, size_t binding_index,
                                  long keycode, int host_flags);
int keymap_editor_remove_binding(struct keymap_editor_model *model,
                                 size_t target_index, size_t binding_index);
int keymap_editor_clear(struct keymap_editor_model *model,
                        size_t target_index);

int keymap_editor_format_host_binding(long keycode, int flags,
                                      char *buffer, size_t buffer_size);
int keymap_editor_format_host_binding_for_layout(
    long keycode, int flags, int german_layout,
    char *buffer, size_t buffer_size);
int keymap_editor_serialize_block(const struct keymap_editor_model *model,
                                  char *buffer, size_t buffer_size,
                                  size_t *written);
int keymap_editor_replace_block(const char *input, size_t input_size,
                                const char *block, size_t block_size,
                                char *output, size_t output_size,
                                size_t *written);
int keymap_editor_merge_target_catalog(struct keymap_editor_model *model,
                                       const char *input, size_t input_size);

#endif

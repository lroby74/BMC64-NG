/*
 * Human-readable names for keys addressed by VICE keyboard matrix entries.
 */

#ifndef BMX_KEYBOARD_MATRIX_H
#define BMX_KEYBOARD_MATRIX_H

#include <stddef.h>

typedef enum {
  BMX_KEYBOARD_MATRIX_C64,
  BMX_KEYBOARD_MATRIX_C128,
  BMX_KEYBOARD_MATRIX_VIC20,
  BMX_KEYBOARD_MATRIX_PLUS4,
  BMX_KEYBOARD_MATRIX_PET_BUSINESS_UK,
  BMX_KEYBOARD_MATRIX_PET_BUSINESS_US,
  BMX_KEYBOARD_MATRIX_PET_BUSINESS_DE,
  BMX_KEYBOARD_MATRIX_PET_GRAPHICS
} BmxKeyboardMatrix;

/*
 * Format the emulated key selected by a VICE .vkm row, column and flags.
 * Forced emulated modifiers are included (for example "Shift+F1").
 * Returns 1 when the matrix position is known, otherwise 0.
 */
int keyboard_matrix_format_emulated_key(BmxKeyboardMatrix matrix,
                                        int row, int column, int flags,
                                        char *buffer, size_t buffer_size);

/* Enumerate every physical key position in a machine keyboard matrix. */
size_t keyboard_matrix_key_count(BmxKeyboardMatrix matrix);
int keyboard_matrix_key_at(BmxKeyboardMatrix matrix, size_t index,
                           int *row, int *column);

/*
 * Enumerate the stable, human-oriented target order used by the keymap
 * editor.  flags contains the VICE mapping behavior required when a new
 * binding is created for the target.  The catalog includes physical matrix
 * keys, machine-specific special keys and the virtual modifier targets used
 * by the shipped US and DE positional maps.
 */
size_t keyboard_matrix_editor_target_count(BmxKeyboardMatrix matrix);
int keyboard_matrix_editor_target_at(BmxKeyboardMatrix matrix, size_t index,
                                     int *row, int *column, int *flags);

#endif

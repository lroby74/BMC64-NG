





#include "gamepad_button_capture.h"

#include <limits.h>
#include <string.h>

#define CAPTURE_STATE_BITS 3U
#define CAPTURE_STATE_MASK ((UINT32_C(1) << CAPTURE_STATE_BITS) - UINT32_C(1))
#define CAPTURE_DEVICE_BITS 2U
#define CAPTURE_DEVICE_SHIFT CAPTURE_STATE_BITS
#define CAPTURE_DEVICE_MASK \
  ((UINT32_C(1) << CAPTURE_DEVICE_BITS) - UINT32_C(1))
#define CAPTURE_BUTTON_BITS 5U
#define CAPTURE_BUTTON_SHIFT (CAPTURE_DEVICE_SHIFT + CAPTURE_DEVICE_BITS)
#define CAPTURE_BUTTON_MASK \
  ((UINT32_C(1) << CAPTURE_BUTTON_BITS) - UINT32_C(1))
#define CAPTURE_GENERATION_SHIFT \
  (CAPTURE_BUTTON_SHIFT + CAPTURE_BUTTON_BITS)
#define CAPTURE_NO_BUTTON CAPTURE_BUTTON_MASK

static uint32_t capture_control(uint32_t generation,
                                BmxGamepadCaptureState state,
                                unsigned device, unsigned button) {
  return (generation << CAPTURE_GENERATION_SHIFT) |
         ((uint32_t)(button & CAPTURE_BUTTON_MASK) << CAPTURE_BUTTON_SHIFT) |
         ((uint32_t)(device & CAPTURE_DEVICE_MASK) << CAPTURE_DEVICE_SHIFT) |
         (uint32_t)state;
}

static uint32_t capture_generation(uint32_t control) {
  return control >> CAPTURE_GENERATION_SHIFT;
}

static BmxGamepadCaptureState capture_state(uint32_t control) {
  return (BmxGamepadCaptureState)(control & CAPTURE_STATE_MASK);
}

static unsigned capture_device(uint32_t control) {
  return (unsigned)((control >> CAPTURE_DEVICE_SHIFT) & CAPTURE_DEVICE_MASK);
}

static unsigned capture_button(uint32_t control) {
  return (unsigned)((control >> CAPTURE_BUTTON_SHIFT) & CAPTURE_BUTTON_MASK);
}

static uint32_t next_generation(uint32_t control) {
  return capture_generation(control) + UINT32_C(1);
}

static uint32_t valid_button_mask(unsigned button_count) {
  if (button_count >= sizeof(uint32_t) * CHAR_BIT) {
    return UINT32_MAX;
  }
  if (button_count == 0U) {
    return 0U;
  }
  return (UINT32_C(1) << button_count) - UINT32_C(1);
}

static int exactly_one_bit(uint32_t value) {
  return value != 0U && (value & (value - UINT32_C(1))) == 0U;
}

static unsigned button_index(uint32_t value) {
  return (unsigned)__builtin_ctz(value);
}

void bmx_gamepad_button_capture_init(BmxGamepadButtonCapture *capture) {
  memset(capture, 0, sizeof(*capture));
}

int bmx_gamepad_button_capture_begin(BmxGamepadButtonCapture *capture,
                                     unsigned device, int assignment) {
  uint32_t control;
  uint32_t generation;
  uint32_t buttons;
  BmxGamepadCaptureState initial_state;

  if (device >= BMX_GAMEPAD_CAPTURE_MAX_DEVICES) {
    return 0;
  }

  control = __atomic_load_n(&capture->control, __ATOMIC_ACQUIRE);
  generation = next_generation(control);
  __atomic_store_n(&capture->assignment, (uint32_t)assignment,
                   __ATOMIC_RELAXED);
  buttons = __atomic_load_n(&capture->latest_buttons[device], __ATOMIC_ACQUIRE);
  initial_state = buttons == 0U ? BMX_GAMEPAD_CAPTURE_WAIT_PRESS
                                : BMX_GAMEPAD_CAPTURE_WAIT_NEUTRAL;
  __atomic_store_n(
      &capture->control,
      capture_control(generation, initial_state, device, CAPTURE_NO_BUTTON),
      __ATOMIC_RELEASE);
  return 1;
}

void bmx_gamepad_button_capture_cancel(BmxGamepadButtonCapture *capture) {
  uint32_t control = __atomic_load_n(&capture->control, __ATOMIC_ACQUIRE);
  __atomic_store_n(&capture->control,
                   capture_control(next_generation(control),
                                   BMX_GAMEPAD_CAPTURE_IDLE, 0,
                                   CAPTURE_NO_BUTTON),
                   __ATOMIC_RELEASE);
}

BmxGamepadCaptureState bmx_gamepad_button_capture_state(
    const BmxGamepadButtonCapture *capture) {
  return capture_state(__atomic_load_n(&capture->control, __ATOMIC_ACQUIRE));
}

int bmx_gamepad_button_capture_report(BmxGamepadButtonCapture *capture,
                                      unsigned device, unsigned buttons,
                                      unsigned button_count) {
  uint32_t control;
  uint32_t expected;
  uint32_t generation;
  uint32_t filtered_buttons;
  unsigned target_device;
  BmxGamepadCaptureState state;

  if (device >= BMX_GAMEPAD_CAPTURE_MAX_DEVICES) {
    return 0;
  }

  filtered_buttons = (uint32_t)buttons & valid_button_mask(button_count);
  __atomic_store_n(&capture->latest_buttons[device], filtered_buttons,
                   __ATOMIC_RELEASE);

  control = __atomic_load_n(&capture->control, __ATOMIC_ACQUIRE);
  state = capture_state(control);
  target_device = capture_device(control);
  if (state == BMX_GAMEPAD_CAPTURE_IDLE || target_device != device) {
    return 0;
  }

  generation = capture_generation(control);
  switch (state) {
  case BMX_GAMEPAD_CAPTURE_WAIT_NEUTRAL:
    if (filtered_buttons == 0U) {
      expected = control;
      (void)__atomic_compare_exchange_n(
          &capture->control, &expected,
          capture_control(generation, BMX_GAMEPAD_CAPTURE_WAIT_PRESS,
                          target_device, CAPTURE_NO_BUTTON),
          0,
          __ATOMIC_RELEASE, __ATOMIC_RELAXED);
    }
    break;

  case BMX_GAMEPAD_CAPTURE_WAIT_PRESS:
    if (exactly_one_bit(filtered_buttons)) {
      expected = control;
      (void)__atomic_compare_exchange_n(
          &capture->control, &expected,
          capture_control(generation, BMX_GAMEPAD_CAPTURE_WAIT_RELEASE,
                          target_device, button_index(filtered_buttons)),
          0,
          __ATOMIC_RELEASE, __ATOMIC_RELAXED);
    } else if (filtered_buttons != 0U) {
      expected = control;
      (void)__atomic_compare_exchange_n(
          &capture->control, &expected,
          capture_control(generation, BMX_GAMEPAD_CAPTURE_WAIT_NEUTRAL,
                          target_device, CAPTURE_NO_BUTTON),
          0,
          __ATOMIC_RELEASE, __ATOMIC_RELAXED);
    }
    break;

  case BMX_GAMEPAD_CAPTURE_WAIT_RELEASE:
    if (filtered_buttons == 0U) {
      expected = control;
      (void)__atomic_compare_exchange_n(
          &capture->control, &expected,
          capture_control(generation, BMX_GAMEPAD_CAPTURE_COMPLETE,
                          target_device, capture_button(control)),
          0,
          __ATOMIC_RELEASE, __ATOMIC_RELAXED);
    }
    break;

  case BMX_GAMEPAD_CAPTURE_COMPLETE:
  case BMX_GAMEPAD_CAPTURE_IDLE:
    break;
  }

  return 1;
}

int bmx_gamepad_button_capture_result(
    const BmxGamepadButtonCapture *capture, unsigned *device,
    unsigned *button, int *assignment) {
  uint32_t control = __atomic_load_n(&capture->control, __ATOMIC_ACQUIRE);

  if (capture_state(control) != BMX_GAMEPAD_CAPTURE_COMPLETE) {
    return 0;
  }
  if (device != NULL) {
    *device = capture_device(control);
  }
  if (button != NULL) {
    *button = capture_button(control);
  }
  if (assignment != NULL) {
    *assignment = (int)__atomic_load_n(&capture->assignment, __ATOMIC_RELAXED);
  }
  return 1;
}

void bmx_gamepad_button_capture_finish(BmxGamepadButtonCapture *capture) {
  bmx_gamepad_button_capture_cancel(capture);
}

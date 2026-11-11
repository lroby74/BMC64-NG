





#ifndef BMX_GAMEPAD_BUTTON_CAPTURE_H
#define BMX_GAMEPAD_BUTTON_CAPTURE_H

#include <stdint.h>

#define BMX_GAMEPAD_CAPTURE_MAX_DEVICES 4

typedef enum {
  BMX_GAMEPAD_CAPTURE_IDLE = 0,
  BMX_GAMEPAD_CAPTURE_WAIT_NEUTRAL,
  BMX_GAMEPAD_CAPTURE_WAIT_PRESS,
  BMX_GAMEPAD_CAPTURE_WAIT_RELEASE,
  BMX_GAMEPAD_CAPTURE_COMPLETE
} BmxGamepadCaptureState;

typedef struct {
  uint32_t control;
  uint32_t latest_buttons[BMX_GAMEPAD_CAPTURE_MAX_DEVICES];
  uint32_t assignment;
} BmxGamepadButtonCapture;

void bmx_gamepad_button_capture_init(BmxGamepadButtonCapture *capture);
int bmx_gamepad_button_capture_begin(BmxGamepadButtonCapture *capture,
                                     unsigned device, int assignment);
void bmx_gamepad_button_capture_cancel(BmxGamepadButtonCapture *capture);
BmxGamepadCaptureState bmx_gamepad_button_capture_state(
    const BmxGamepadButtonCapture *capture);





int bmx_gamepad_button_capture_report(BmxGamepadButtonCapture *capture,
                                      unsigned device, unsigned buttons,
                                      unsigned button_count);





int bmx_gamepad_button_capture_result(
    const BmxGamepadButtonCapture *capture, unsigned *device,
    unsigned *button, int *assignment);
void bmx_gamepad_button_capture_finish(BmxGamepadButtonCapture *capture);

#endif

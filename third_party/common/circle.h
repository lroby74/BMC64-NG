/*
 * circle.h
 *
 * Written by
 *  Randy Rossi <randy.rossi@gmail.com>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#ifndef EMU_COMMON_H
#define EMU_COMMON_H

// This is an interface layer describing both functions
// the kernel needs to invoke on the emulator and for the
// emulator to use some facilities provided by the kernel.

#include <sys/types.h>
#include <stdint.h>

#define MIN(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#define MAX(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })








struct bmx_crt_effect_params {
   int geometry_enabled;
   float curvature_x;
   float curvature_y;
   float skew_x;
   float skew_y;
   float trapezoid;
   float rotation_degrees;
   float overscan_scale;

   int convergence_enabled;
   float red_offset_x;
   float red_offset_y;
   float blue_offset_x;
   float blue_offset_y;
   float convergence_radial_strength;

   int horizontal_filtering_enabled;
   float horizontal_sigma_x;

   int edge_blur_enabled;
   float edge_blur_strength;
   float edge_blur_radius;

   int scanlines_enabled;
   int scanline_multisample;
   float scanline_weight;
   float scanline_gap_brightness;

   int phosphor_mask_enabled;
   int phosphor_mask_type;
   float phosphor_mask_brightness;

   int bloom_enabled;
   float bloom_factor;

   int vignette_enabled;
   float vignette_strength;
   float vignette_scale;
   float vignette_softness;

   int uneven_illumination_enabled;
   float uneven_illumination_strength;
   float uneven_illumination_scale;

   int horizontal_jitter_enabled;
   float horizontal_jitter_strength;
   float horizontal_jitter_frequency;
   float horizontal_jitter_speed;

   int composite_artifacts_enabled;
   float composite_chroma_blur;
   float composite_luma_sharpen;
   float composite_color_bleed;

   int glass_reflection_enabled;
   float glass_reflection_angle;
   float glass_reflection_width;
   float glass_reflection_position;

   int rounded_screen_mask_enabled;
   float rounded_corner_radius;
   float rounded_border_softness;

   int edge_glow_enabled;
   float edge_glow_strength;
   float edge_glow_width;

   int noise_enabled;
   float luminance_noise;
   float chroma_noise;
   float noise_speed;

   int output_response_enabled;
   int output_response_fast;
   int output_level_mapping;
   float input_gamma;
   float output_gamma;
   float output_saturation;
   float black_level;
   float white_clip;

   int bilinear_interpolation;
};

enum bmx_output_level_mapping {
   BMX_OUTPUT_LEVEL_MAPPING_LINEAR = 0,
   BMX_OUTPUT_LEVEL_MAPPING_CUBIC,
   BMX_OUTPUT_LEVEL_MAPPING_TOE_SHOULDER
};

#define MAX_USB_DEVICES 4
#define MAX_JOY_PORTS 4

#define NUM_GPIO_PINS 23

#define MACHINE_TIMING_NTSC_HDMI 0
#define MACHINE_TIMING_PAL_HDMI 1
#define MACHINE_TIMING_NTSC_COMPOSITE 2
#define MACHINE_TIMING_PAL_COMPOSITE 3
#define MACHINE_TIMING_PAL_CUSTOM_HDMI 4
#define MACHINE_TIMING_NTSC_CUSTOM_HDMI 5
#define MACHINE_TIMING_NTSC_DPI 6
#define MACHINE_TIMING_PAL_DPI 7
#define MACHINE_TIMING_PAL_CUSTOM_DPI 8
#define MACHINE_TIMING_NTSC_CUSTOM_DPI 9

#define FB_NUM_LAYERS   4
#define FB_LAYER_VIC    0
#define FB_LAYER_VDC    1
#define FB_LAYER_STATUS 2
#define FB_LAYER_UI     3
#define FB_LAYER_MASK(layer) (1U << (layer))

#define USB_PREF_ANALOG 0
#define USB_PREF_HAT 1
#define USB_PREF_HAT_AND_PADDLES 2

// NOTE: BTN_ASSIGN_* are used as indices into choice
// arrays.
#define BTN_ASSIGN_UNDEF 0
#define BTN_ASSIGN_FIRE 1
#define BTN_ASSIGN_MENU 2
#define BTN_ASSIGN_WARP 3
#define BTN_ASSIGN_STATUS_TOGGLE 4
#define BTN_ASSIGN_SWAP_PORTS 5

// Directions and POTs are not available for hotkeys, only buttons
#define BTN_ASSIGN_UP 6
#define BTN_ASSIGN_DOWN 7
#define BTN_ASSIGN_LEFT 8
#define BTN_ASSIGN_RIGHT 9
#define BTN_ASSIGN_POTX 10
#define BTN_ASSIGN_POTY 11

// Back to functions available to anything
#define BTN_ASSIGN_TAPE_MENU 12
#define BTN_ASSIGN_CART_MENU 13
#define BTN_ASSIGN_CART_FREEZE 14
#define BTN_ASSIGN_RESET_HARD 15
#define BTN_ASSIGN_RESET_SOFT 16

enum circle_network_status {
  CIRCLE_NETWORK_DISABLED,
  CIRCLE_NETWORK_ETHERNET_INITIALIZING,
  CIRCLE_NETWORK_ETHERNET_WAITING_FOR_DHCP,
  CIRCLE_NETWORK_ETHERNET_CONNECTED,
  CIRCLE_NETWORK_ETHERNET_UNAVAILABLE,
  CIRCLE_NETWORK_ETHERNET_INIT_FAILED,
  CIRCLE_NETWORK_WIFI_CONFIG_MISSING,
  CIRCLE_NETWORK_WIFI_DEVICE_INITIALIZING,
  CIRCLE_NETWORK_WIFI_DEVICE_INIT_FAILED,
  CIRCLE_NETWORK_WIFI_NETWORK_INIT_FAILED,
  CIRCLE_NETWORK_WIFI_WPA_INITIALIZING,
  CIRCLE_NETWORK_WIFI_WPA_INIT_FAILED,
  CIRCLE_NETWORK_WIFI_CONNECTING,
  CIRCLE_NETWORK_WIFI_CONNECTED,
  CIRCLE_NETWORK_WIFI_CONNECTION_TIMEOUT,
  CIRCLE_NETWORK_WIFI_UNAVAILABLE,
  CIRCLE_NETWORK_WIFI_DEVICE_NOT_INITIALIZED,
  CIRCLE_NETWORK_WIFI_UNSUPPORTED,
  CIRCLE_NETWORK_WIFI_FIRMWARE_MISSING,
  CIRCLE_NETWORK_STATUS_COUNT
};

#define CIRCLE_ACIA_NETWORK_ADDRESS_DEFAULT 1
#define CIRCLE_ACIA_NETWORK_ADDRESS_VALUES { 0xd700, 0xde00, 0xdf00, 0xdf80 }
#define CIRCLE_ACIA_NETWORK_ADDRESS_LABELS { "D700", "DE00", "DF00", "DF80" }

// Returns nonzero and writes the current address when DHCP is bound.
int circle_get_network_ip_address(char *address, unsigned int address_size);
int circle_get_network_status(void);
typedef void circle_network_status_changed_handler_t(void);
void circle_set_network_status_changed_handler(
  circle_network_status_changed_handler_t *handler);
int circle_get_acia_network_enabled(void);
int circle_get_acia_network_address(void);
int circle_set_acia_network_address(int address);
int circle_set_acia_network_enabled(int enabled);
int circle_has_onboard_ethernet(void);
int circle_has_onboard_wifi(void);
int circle_wifi_is_running(void);
int circle_connect_wifi(void);

#define MAX_WIFI_ACCESS_POINTS 32
struct wifi_access_point {
  char ssid[33];
  int signal;
  int secure;
  int mhz;
  int channel;
};

int circle_get_network_config(int which, char *out, unsigned int size);

extern volatile int bmc_condivisione_attiva;
int circle_smb_start(const char *user, const char *password);
void circle_smb_stop(void);
int circle_smb_active(void);
int circle_smb_open_files(void);
void circle_smb_giro(unsigned microsecondi);

void circle_usb_nota_menu(void);
const char *circle_smb_fase(void);

int circle_scan_wifi_access_points(struct wifi_access_point *access_points,
                                   unsigned int max_access_points);

// More just for usb buttons
#define BTN_ASSIGN_RUN_STOP_BACK 17
#define BTN_ASSIGN_CUSTOM_KEY_1 18
#define BTN_ASSIGN_CUSTOM_KEY_2 19
#define BTN_ASSIGN_CUSTOM_KEY_3 20
#define BTN_ASSIGN_CUSTOM_KEY_4 21
#define BTN_ASSIGN_CUSTOM_KEY_5 22
#define BTN_ASSIGN_CUSTOM_KEY_6 23

// More to functions available to anything
#define BTN_ASSIGN_ACTIVE_DISPLAY 24
#define BTN_ASSIGN_PIP_LOCATION 25
#define BTN_ASSIGN_PIP_SWAP 26
#define BTN_ASSIGN_40_80_COLUMN 27
#define BTN_ASSIGN_VKBD_TOGGLE 28
#define BTN_ASSIGN_RESET_MENU 29
#define BTN_ASSIGN_FLUSH_DISK 30

// These are intermediate values not meant to
// be directly assigned to buttons. Never used as
// an index into anything.
#define BTN_ASSIGN_RESET_HARD2 916
#define BTN_ASSIGN_RESET_SOFT2 918




#define BTN_ASSIGN_TAPE_PLAY2 920
#define BTN_ASSIGN_TAPE_STOP2 921
#define BTN_ASSIGN_TAPE_FF2 922
#define BTN_ASSIGN_TAPE_REW2 923
#define BTN_ASSIGN_TAPE_ZERO2 924





#define BTN_ASSIGN_ATTACH_DISK_8_2 930
#define BTN_ASSIGN_DETACH_DISK_8_2 931
#define BTN_ASSIGN_ATTACH_DISK_9_2 932
#define BTN_ASSIGN_DETACH_DISK_9_2 933
#define BTN_ASSIGN_ATTACH_CART_2   934
#define BTN_ASSIGN_DETACH_CART_2   935
#define BTN_ASSIGN_ATTACH_TAPE_2   936
#define BTN_ASSIGN_DETACH_TAPE_2   937
#define BTN_ASSIGN_AUTOSTART_2     938

#define BTN_ASSIGN_MODELLO8_NONE   940
#define BTN_ASSIGN_MODELLO8_1541   941
#define BTN_ASSIGN_MODELLO8_1541II 942
#define BTN_ASSIGN_MODELLO8_1570   943
#define BTN_ASSIGN_MODELLO8_1571   944
#define BTN_ASSIGN_MODELLO8_1581   945
#define BTN_ASSIGN_MODELLO8_VERO   946

#define BTN_ASSIGN_OVERLAY_2       947




#define BTN_ASSIGN_OVERLAY_INFO_2  975
#define BTN_ASSIGN_VOLUME_SU       948
#define BTN_ASSIGN_VOLUME_GIU      949
#define BTN_ASSIGN_ASPETTO_2       950



#define BTN_ASSIGN_MODELLO9_NONE   951
#define BTN_ASSIGN_MODELLO9_1541   952
#define BTN_ASSIGN_MODELLO9_1541II 953
#define BTN_ASSIGN_MODELLO9_1570   954
#define BTN_ASSIGN_MODELLO9_1571   955
#define BTN_ASSIGN_MODELLO9_1581   956
#define BTN_ASSIGN_MODELLO9_VERO   957


#define BTN_ASSIGN_SCHERMO_2       958


#define BTN_ASSIGN_MUTO            959


#define BTN_ASSIGN_COLONNE_2       960




#define BTN_ASSIGN_ASPETTO_ALTRO   961





#define BTN_ASSIGN_REU_2           962


#define BTN_ASSIGN_SID_2           963
#define BTN_ASSIGN_SOLCO_SU        964
#define BTN_ASSIGN_SOLCO_GIU       965



#define BTN_ASSIGN_TAPE_FF_2X      968
#define BTN_ASSIGN_TAPE_REW_2X     969




extern volatile unsigned long bmc_giri[4];

extern volatile unsigned long bmc_sid3_lavori;

extern volatile unsigned fbl_composizioni;
extern volatile int bmc_dove;
extern volatile int bmc_scheda_in_uso;
extern volatile int bmc_scheda_core0;



extern volatile int bmc_vice_partito;
extern volatile int bmc_dopo_usb_da_scrivere;



extern volatile int bmc_usb_pronta;
extern volatile int bmc_core0_nella_scheda;
extern const char *volatile bmc_core0_nella_scheda_cosa;

extern const char *volatile bmc_core0_dove;
extern volatile int bmc_lucchetto;
extern volatile int bmc_blocco_scritto;



extern const char *volatile bmc_scheda_cosa;
extern const char *volatile bmc_scheda_file;
extern volatile unsigned long bmc_scheda_da;
#define DOVE_EMULA        1
#define DOVE_SINCRONIA    2
#define DOVE_ATTESA       3
#define DOVE_DOPO         4
#define DOVE_MENU         5
#define DOVE_SCHERMO      6
#define DOVE_PRESTITO_USB 7
#define DOVE_PAUSA_SID    8
#define BTN_ASSIGN_SID_FERMA       966
#define BTN_ASSIGN_REU_MISURA      967


#define BTN_ASSIGN_REU_MONTA       970
#define BTN_ASSIGN_REU_SMONTA      971


#define BTN_ASSIGN_SID_PREV        972
#define BTN_ASSIGN_SID_NEXT        973
#define BTN_ASSIGN_SID_PAUSA       974



#define BTN_ASSIGN_SID_VOCE_1      976
#define BTN_ASSIGN_SID_VOCE_2      977
#define BTN_ASSIGN_SID_VOCE_3      978
#define BTN_ASSIGN_SID_VOCE_4      979
#define BTN_ASSIGN_SID_VOCE_5      980
#define BTN_ASSIGN_SID_VOCE_6      981
#define BTN_ASSIGN_SID_VOCE_7      982
#define BTN_ASSIGN_SID_VOCE_8      983
#define BTN_ASSIGN_SID_VOCE_9      984
#define BTN_ASSIGN_SID_MUTA_1      985
#define BTN_ASSIGN_SID_MUTA_2      986
#define BTN_ASSIGN_SID_MUTA_3      987
#define BTN_ASSIGN_SID_VU          988
#define BTN_ASSIGN_SID_TEMPO       989
#define BTN_ASSIGN_SID_INFO        990



#define BTN_ASSIGN_SID_VOCE_10     991
#define BTN_ASSIGN_SID_VOCE_11     992
#define BTN_ASSIGN_SID_VOCE_12     993
#define BTN_ASSIGN_SID_MUTA_4      994



#define BTN_ASSIGN_SID_LATO_1      995
#define BTN_ASSIGN_SID_LATO_2      996
#define BTN_ASSIGN_SID_LATO_3      997
#define BTN_ASSIGN_SID_LATO_4      998


#define BTN_ASSIGN_SID_INDIETRO    999
#define BTN_ASSIGN_SID_AVANTI      1000


#define BTN_ASSIGN_SID_MUTA_5      1001
#define BTN_ASSIGN_SID_MUTA_6      1002
#define BTN_ASSIGN_SID_MUTA_7      1003
#define BTN_ASSIGN_SID_MUTA_8      1004
#define BTN_ASSIGN_SID_LATO_5      1005
#define BTN_ASSIGN_SID_LATO_6      1006
#define BTN_ASSIGN_SID_LATO_7      1007
#define BTN_ASSIGN_SID_LATO_8      1008



#define BTN_ASSIGN_SID_MOTORE      1009

#define JOYDEV_NUM_JOYDEVS 23
#define JOYDEV_NONE 0
#define JOYDEV_NUMPAD 1
#define JOYDEV_KEYSET1 2
#define JOYDEV_KEYSET2 3
#define JOYDEV_ANALOG_0 4
#define JOYDEV_ANALOG_1 5
#define JOYDEV_ANALOG_2 6
#define JOYDEV_ANALOG_3 7
#define JOYDEV_ANALOG_4 8
#define JOYDEV_ANALOG_5 9
#define JOYDEV_DIGITAL_0 10
#define JOYDEV_DIGITAL_1 11
#define JOYDEV_USB_0 12
#define JOYDEV_USB_1 13
#define JOYDEV_GPIO_0 14
#define JOYDEV_GPIO_1 15
#define JOYDEV_CURS_SP 16
#define JOYDEV_NUMS_1 17
#define JOYDEV_NUMS_2 18
#define JOYDEV_CURS_LC 19
#define JOYDEV_MOUSE 20
#define JOYDEV_USB_2 21
#define JOYDEV_USB_3 22


#define JOYDEV_MOUSE_MICROMYS 23




#define JOYDEV_SPINNER 24

#define GPIO_CONFIG_DISABLED -1
#define GPIO_CONFIG_NAV_JOY 0
#define GPIO_CONFIG_KYB_JOY 1
#define GPIO_CONFIG_WAVESHARE 2
#define GPIO_CONFIG_USERPORT 3
#define GPIO_CONFIG_CUSTOM 5

struct axis_config {
  int use;
  int neutral;
  int min;
  int max;
  int dir;
};

struct hat_config {
  int use;
  int dir[9]; // DIR_XX_INDEX
};

// We maintain two joystick devices that can moved
// to different ports.
struct joydev_config {
  // Which port does this device belong to?
  int port;
  int device;

  // Relevant for usb devices
  struct axis_config axes[4];
  struct hat_config hats[2];
};

extern struct joydev_config joydevs[MAX_JOY_PORTS];

extern int custom_gpio_pins[NUM_GPIO_PINS];

// Lower byte is BTN_ASSIGN_ constant. Upper byte can be bank or other arg.
extern unsigned int gpio_bindings[NUM_GPIO_PINS];


// -----------------------------------------------------------------------
// Functions called from emulator layer into kernel layer
// -----------------------------------------------------------------------
extern int circle_get_machine_timing();
extern void circle_sleep(long);
extern unsigned long circle_get_ticks();
extern void circle_yield();
extern void circle_check_gpio();
extern void circle_poweroff(void);

extern int circle_ha_il_tasto_power(void);
extern int circle_power_button_pressed(void);
extern void circle_reset_gpio(int gpio_config);
extern int circle_alloc_fbl(int pixelmode, int layer, uint8_t **pixels,
                            int width, int height, int *pitch);
extern int circle_realloc_fbl(int layer, int shader);
extern void circle_free_fbl(int layer);
extern void circle_clear_fbl(int layer);
extern void circle_show_fbl(int layer);
extern void circle_hide_fbl(int layer);




extern void circle_present_fbl(uint32_t ready_mask, int sync);
extern int circle_shader_backend_available(void);
extern int circle_shader_backend_available_for_layer(int layer);
extern int circle_status_layer_can_coexist_with_ui(void);
extern void circle_set_palette_fbl(int layer, uint8_t index, uint16_t rgb565);
extern void circle_set_palette32_fbl(int layer, uint8_t index, uint32_t argb);
extern void circle_update_palette_fbl(int layer);
extern void circle_set_stretch_fbl(int layer, double hstretch, double vstretch, int hintstr, int vintstr, int use_hintstr, int use_vintstr);
extern void circle_set_src_rect_fbl(int layer, int x, int y, int w, int h);
extern void circle_set_center_offset(int layer, int cx, int cy);
extern void circle_set_valign_fbl(int layer, int align, int padding);
extern void circle_set_halign_fbl(int layer, int align, int padding);
extern void circle_set_padding_fbl(int layer, double lpad, double rpad,
                                   double tpad, double bpad);
extern void circle_set_zlayer_fbl(int layer, int zlayer);
extern int circle_get_zlayer_fbl(int layer);
extern void circle_lock_acquire();
extern void circle_lock_release();
extern void circle_boot_complete();
extern void circle_find_usb(int (*usb)[3]);
extern int circle_mount_usb(int usb);
extern int circle_unmount_usb(int usb);


extern void circle_find_floppy(int (*flp)[2]);
extern int circle_floppy_stato(int n);
extern void circle_set_volume(int value);
extern int circle_get_model();
extern unsigned circle_arm_max_mhz(void);

extern unsigned circle_gpu_mhz(int quale, int massimo);

extern unsigned circle_get_gpu_mhz(void);


extern int circle_get_local_time(unsigned *secs);
extern int circle_board_type(void);
extern unsigned circle_get_arm_clock();
extern void circle_cpu_slow(int slow);





//


extern int circle_xum1541_present(void);





extern int circle_gamepad_descriptor(int device, unsigned char *buf, int max);
extern int circle_xum1541_open(void);
extern void circle_xum1541_close(void);
extern int circle_xum1541_control_in(int request, unsigned char *buf, int len);
extern int circle_xum1541_control_out(int request);
extern void circle_xum1541_clear_halts(void);



extern void circle_xum1541_set_imod(int imodi);
extern int circle_xum1541_get_imod(void);
extern int circle_xum1541_bulk_out(const unsigned char *buf, int len);
extern int circle_xum1541_bulk_in(unsigned char *buf, int len);
extern unsigned circle_get_temperature();
extern int circle_gpio_enabled();
extern int circle_gpio_outputs_enabled();

extern int circle_sound_init(const char *param, int *speed, int *fragsize,
                        int *fragnr, int *channels);
extern int circle_sound_write(int16_t *pbuf, size_t nr);
extern void circle_sound_close(void);
extern int circle_sound_suspend(void);
extern int circle_sound_resume(void);
extern int circle_sound_bufferspace(void);



extern unsigned long circle_memoria_libera(void);



extern int fbl_stato_scalatore(void);
extern int fbl_stato_v3d(void);


extern void circle_diag_schermo(const char *quando);



extern int circle_uscita_audio(void);



extern void circle_sound_banco_blocca(int millisecondi);







extern int circle_usb_presta(void);
extern void circle_usb_restituisci(void);

extern uint8_t circle_get_userport_ddr(void);
extern uint8_t circle_get_userport(void);
extern void circle_set_userport(uint8_t value);
extern void circle_kernel_core_init_complete(int core);
extern void circle_get_fbl_dimensions(int layer,
                                      int *display_w, int *display_h,
                                      int *fb_w, int *fb_h,
                                      int *src_w, int *src_h,
                                      int *dst_w, int *dst_h);
extern void circle_get_scaling_params(int display,
                                      int *fbw, int *fbh,
                                      int *sx, int *sy);
extern void circle_set_interpolation(int enable);
extern void circle_set_use_shader(int enable);
extern void circle_set_shader_params(const struct bmx_crt_effect_params *params);

// -----------------------------------------------------------------------
// Functions called from kernel layer into emulator layer
// -----------------------------------------------------------------------

// Init some common layer stuff about the machine being emulated.
// Must be called before launching emulator's main_program func.
extern void emu_machine_init(int raster_skip_enabled, int raster_skip2_enabled);

// Compares the previous button state for 'button_num' with
// the current state and will return a press or release event
// for that button if the button has a button assignment.
extern int emu_button_function(int device, int button_num, unsigned buttons,
                               int* btn_assignment, int* is_press);

// Ask emulator to logically OR in the joystick latch value associated
// with a USB button assignment.  button_bit is the power of 2 representing
// the list of USB buttons discovered and held by usb_button_bits array.
extern int emu_add_button_values(int device, unsigned button_bit);

// Functions to trigger emulated mouse move and button events
extern void emu_mouse_move(int x, int y);
extern void emu_mouse_button_left(int pressed);
extern void emu_mouse_button_right(int pressed);
extern void emu_mouse_button_middle(int pressed);
extern void emu_mouse_wheel_up(int pressed);
extern void emu_mouse_wheel_down(int pressed);

// Queue a joystick latch event for the main loop. Interrupt safe.
extern void emu_joy_interrupt_abs(int port, int device,
                                  int js_up,
                                  int js_down,
                                  int js_left,
                                  int js_right,
                                  int js_fire,
                                  int pot_x, int pot_y);




extern void emu_spinner_mouse(unsigned buttons, int wheel);


extern void emu_spinner_alza(int j);

// Queue a quick function request for the main loop. Interrupt safe.
extern void emu_quick_func_interrupt(int button_assignment);

// Ask emulator what the current gpio config index is.
extern int emu_get_gpio_config(void);

// Set a joystick latch value from a USB device. Interrupt safe.
extern void emu_set_joy_usb_interrupt(unsigned device, int value);

// Get the keycode binding for the given custom binding index.
extern long emu_get_key_binding(int index);

// Ask emulator to press/release keys by keycode.



extern void emu_modificatori_grezzi(unsigned char modificatori);
extern void emu_key_pressed(long keycode);
extern void emu_key_released(long keycode);

// Get number of virtual joysticks available in this emulator.
extern int emu_get_num_joysticks(void);

// Enable/disable demo mode for this emulator.
extern void emu_set_demo_mode(int is_demo);

// Test whether the UI is currently activated or not.
extern int emu_is_ui_activated(void);




extern int emu_get_keyboard_shiftlock(void);





extern void passo_nota(const char *cosa);

// Send a key press/release to the UI. Should be called only when ui
// is activated.
extern void emu_ui_key_interrupt(long key, int pressed);

// Gets usb preferences set by config for this USB device.
extern void emu_get_usb_pref(int device, int *usb_pref,
                             int *x_axis, int *y_axis,
                             float *x_thresh, float *y_thresh);

// Tell emulator about known gamepad configuration. Used after usb init.
extern void emu_set_gamepad_info(int num_pads,
                                 int num_buttons[2],
                                 int axes[2],
                                 int hats[2]);

extern void emu_set_usb_gamepad_mapping_profile(int device, unsigned profile);
extern void emu_set_usb_gamepad_display_name(int device, const char *display_name);

// Test whether emulator is in a config mode where it wants to receive
// raw usb data.
extern int emu_wants_raw_usb(void);

// Send the emulator raw usb data. Used for configuring usb devices in menu.
extern void emu_set_raw_usb(int device,
                            unsigned buttons,
                            const int hats[6],
                            const int axes[16]);

extern void emu_exit(void);





extern unsigned long raspi_meter_emu_avg_us;
extern unsigned long raspi_meter_emu_max_us;
extern unsigned long raspi_meter_frame_avg_us;
extern unsigned raspi_meter_late_pct;
extern unsigned raspi_meter_speed_pct;
extern unsigned raspi_meter_speed_periodo_pct;
extern unsigned raspi_meter_fps_x10;



extern unsigned raspi_snd_dev;
extern int raspi_snd_play;
extern unsigned long raspi_snd_written;
extern unsigned long raspi_snd_dal_vice;
extern int raspi_snd_space;
extern int raspi_snd_restart;
extern unsigned raspi_snd_restarts;


extern unsigned raspi_snd_picco;
extern unsigned long raspi_snd_vuoti;
extern unsigned long raspi_snd_scartati;
extern unsigned raspi_snd_cambi;
extern unsigned raspi_snd_assenze;
extern unsigned long raspi_snd_tagliati;

extern void circle_snd_azzera_conti(void);
extern unsigned raspi_snd_hz;
extern int raspi_snd_vol_cb;
extern unsigned raspi_snd_vol_pct;


extern unsigned raspi_snd_canali;
extern unsigned raspi_snd_porta;


extern unsigned raspi_snd_picco_sx;
extern unsigned raspi_snd_picco_dx;
extern unsigned long raspi_vchiq_byte;
extern unsigned raspi_vchiq_coda;
extern unsigned raspi_vchiq_inizio;
extern unsigned long raspi_vchiq_attese;

extern void circle_diag_audio(const char *quando);


extern unsigned raspi_disp_w;
extern unsigned raspi_disp_h;

extern unsigned raspi_disp_fisico_milli;
extern unsigned raspi_fb_ask_w;
extern unsigned raspi_fb_ask_h;
extern unsigned raspi_fb_got_w;
extern unsigned raspi_fb_got_h;
extern unsigned raspi_fb_pitch;
extern unsigned raspi_fb_size;
extern unsigned raspi_fb_displays;

extern unsigned raspi_fb2_w;
extern unsigned raspi_fb2_h;
extern int raspi_fb2_strato;




extern int circle_secondo_schermo(int strato, int forza);
extern int circle_secondo_schermo_info(int *w, int *h, int *displays);

extern int circle_doppio_hdmi_all_avvio(void);


extern int circle_fb_display_chiesto(void);

extern int circle_composito_chiesto(void);
extern unsigned raspi_fb_display;
extern unsigned raspi_fb_pages;
extern unsigned raspi_fb_bpp;
extern unsigned raspi_fb_worker;
extern unsigned raspi_fb_dma;

#endif

/*
 * missing.c
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

#include "missing.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/time.h>

#include "bmc64_ui.h"
#include "raspi_machine.h"

// ------------------------------------------------------------------------
// These are stubs to get things compiling. The vast majority of these
// routines will not require an implementation. Once a routine is given
// an implementation, it should be moved out of missing.c and into
// one of the other .c files in this directory (or a new one if it deserves
// it).
// ------------------------------------------------------------------------

char video_canvas_can_resize(struct video_canvas_s *canvas) { return 0; }
int c128ui_init_early(void) { return 0; }
int c128ui_init(void) {
  ui_init_menu();
  return 0;
}
int c64dtvui_init_early(void) { return 0; }
int c64dtvui_init(void) { return 0; }
int c64scui_init_early(void) { return 0; }
int c64scui_init(void) {


  ui_init_menu();
  return 0;
}
int c64ui_init_early(void) { return 0; }
int c64ui_init(void) {
  ui_init_menu();
  return 0;
}
int cbm2ui_init_early(void) { return 0; }
int cbm2ui_init(void) { return 0; }
int cbm5x0ui_init_early(void) { return 0; }
int cbm5x0ui_init(void) { return 0; }
int dthread_ui_init_finish(void) { return 0; }
int dthread_ui_init(int *argc, char **argv) { return 0; }
int joy_arch_cmdline_options_init(void) { return 0; }
int joy_arch_resources_init(void) { return 0; }
int joy_arch_set_device(int port_idx, int new_dev) { return 0; }
int mui_init(void) { return 0; }
int petui_init_early(void) { return 0; }
int petui_init(void) {
  ui_init_menu();
  return 0;
}
int plus4ui_init_early(void) { return 0; }
int plus4ui_init(void) {
  ui_init_menu();
  return 0;
}
int scpu64ui_init_early(void) { return 0; }
int scpu64ui_init(void) {


  ui_init_menu();
  return 0;
}
int ui_init2(int *argc, char **argv) { return 0; }
int ui_init_finish2(void) { return 0; }
int ui_init_finish(void) { return 0; }


int vic20ui_init_early(void) { return 0; }
int vic20ui_init(void) {
  ui_init_menu();
  return 0;
}
int video_arch_cmdline_options_init(void) { return 0; }
int video_arch_resources_init(void) { return 0; }
int video_canvas_refresh_dx9(video_canvas_t *canvas, unsigned int xs,
                             unsigned int ys, unsigned int xi, unsigned int yi,
                             unsigned int w, unsigned int h) {
  return 0;
}
int video_init(void) { return 0; }
video_canvas_t *video_canvas_create_ddraw(video_canvas_t *canvas) { return 0; }
video_canvas_t *video_canvas_create_dx9(video_canvas_t *canvas,
                                        unsigned int *width,
                                        unsigned int *height) {
  return 0;
}
void c128ui_shutdown(void) {}
void c64dtvui_shutdown(void) {}
void c64scui_shutdown(void) {}
void c64ui_shutdown(void) {}
void cbm2ui_shutdown(void) {}
void cbm5x0ui_shutdown(void) {}
void joy_arch_init_default_mapping(int joynum) {}
void petui_shutdown(void) {}
void plus4ui_shutdown(void) {}
void scpu64ui_shutdown(void) {}
void sdl_ui_init_draw_params(void) {}
void sdl_ui_init_finalize(void) {}
void tui_error(const char *format, ...) {}
void tui_init(void) {}
void ui_display_event_time(unsigned int current, unsigned int total) {}
void ui_display_joyport(uint16_t *joyport) {}
void ui_display_playback(int playback_status, char *version) {}
void ui_display_recording(int recording_status) {}
void ui_display_tape_current_image(int port, const char *image) {}
void ui_error_string(const char *text) {}
void ui_init_checkbox_style(void) {}
void ui_init_drive_status_widget(void) {}
void ui_init_joystick_status_widget(void) {}
void ui_set_tape_status(int port, int tape_status) {}
void ui_update_menus(void) {}
void vic20ui_shutdown(void) {}
void video_arch_resources_shutdown(void) {}
void video_canvas_destroy_ddraw(video_canvas_t *canvas) {}
void video_canvas_destroy(struct video_canvas_s *canvas) {}
void video_canvas_refresh_ddraw(video_canvas_t *canvas, unsigned int xs,
                                unsigned int ys, unsigned int xi,
                                unsigned int yi, unsigned int w,
                                unsigned int h) {}
void video_canvas_resize(struct video_canvas_s *canvas, char resize_canvas) {}
void video_canvas_set_palette_ddraw_8bit(video_canvas_t *canvas) {}
void video_shutdown_dx9(void) {}
void video_shutdown(void) {}
void vsyncarch_display_speed(double speed, double fps, int warp_enabled) {}
void ui_display_volume(int vol) {}
void main_exit(void) {}











#include <errno.h>
#include <sys/types.h>



uid_t getuid(void) { return 0; }

struct passwd;
struct passwd *getpwuid(uid_t uid) { (void)uid; return 0; }

unsigned int sleep(unsigned int seconds) { (void)seconds; return 0; }

int execvp(const char *file, char *const argv[])
{
    (void)file; (void)argv; errno = ENOSYS; return -1;
}

int execl(const char *path, const char *arg, ...)
{
    (void)path; (void)arg; errno = ENOSYS; return -1;
}

pid_t waitpid(pid_t pid, int *status, int options)
{
    (void)pid; (void)status; (void)options; errno = ECHILD; return -1;
}

struct sigaction;
int sigaction(int signum, const struct sigaction *act, struct sigaction *old)
{
    (void)signum; (void)act; (void)old; return 0;
}

int dup2(int oldfd, int newfd) { (void)oldfd; (void)newfd; errno = EBADF; return -1; }







typedef struct vice_network_socket_s vice_network_socket_t;
typedef struct vice_network_socket_address_s vice_network_socket_address_t;

int vice_network_send(vice_network_socket_t *s, const void *b, size_t n, int f)
{
    (void)s; (void)b; (void)n; (void)f; return -1;
}

int vice_network_socket_close(vice_network_socket_t *s) { (void)s; return -1; }

vice_network_socket_address_t *vice_network_address_generate(const char *addr,
                                                             unsigned short port)
{
    (void)addr; (void)port; return 0;
}

vice_network_socket_t *vice_network_client(const vice_network_socket_address_t *a)
{
    (void)a; return 0;
}





void joystick_arch_init(void) { }
void joystick_arch_shutdown(void) { }

void ui_display_reset(int device, int mode) { (void)device; (void)mode; }

void ui_actions_init(void) { }
void ui_actions_shutdown(void) { }

int ui_hotkeys_resources_init(void) { return 0; }
int ui_hotkeys_cmdline_options_init(void) { return 0; }
void ui_hotkeys_shutdown(void) { }

void archdep_network_shutdown(void) { }

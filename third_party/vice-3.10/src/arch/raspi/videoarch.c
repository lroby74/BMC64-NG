/*
 * videoarch.c
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

#include "videoarch.h"



static int alza_i_tasti = 0;



static int quanto_aspetto = 0;

void raspi_alza_i_tasti(int fotogrammi)
{
  if (fotogrammi > alza_i_tasti) {
    alza_i_tasti = fotogrammi;
  }
}
#ifdef HAVE_REALDEVICE
#include "opencbm_xum1541.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>

// VICE includes
#include "joyport/joystick.h"
#include "kbdbuf.h"
#include "keyboard.h"
#include "machine.h"
#include "maincpu.h"
#include "mem.h"
#include "monitor.h"
#include "resources.h"
#include "vsync.h"
#include "sid.h"
#include "video.h"
#include "viewport.h"
#include "attach.h"
#include "diskimage.h"
#include "drive.h"
#include "drivetypes.h"
#include "fsimage.h"
#include "iec/wd1770.h"
#include "log.h"

// RASPI includes
#include "emux_api.h"
#include "demo.h"
#include "joy.h"
#include "kbd.h"
#include "menu.h"
#include "menu_usb.h"
#include "menu_tape_osd.h"
#include "overlay.h"
#include "ui.h"
#include "raspi_machine.h"
#include "bmc64_ui.h"

























struct video_canvas_s *vdc_canvas;
struct video_canvas_s *vic_canvas;
struct video_canvas_s *canvases[2];
struct video_draw_buffer_callback_s draw_buffer_callback[2];

// NOTE: For Plus/4, the vic_* variables are actually ted.
// Maybe rename to pri_?

static int vic_first_refresh;
static int vdc_first_refresh;

// We tell vice our clock resolution is the actual vertical
// refresh rate of the machine * some factor. We report our
// tick count when asked for the current time which is incremented
// by that factor after each vertical blank.  Not sure if this
// is really needed anymore since we turned off auto refresh.
// Seems to work fine though.
unsigned long video_ticks = 0;

// So...due to math stuff, whatever value we put for our video tick
// increment here will be how many frames we show before vice decides
// to skip.  Can't really figure out why.  Needs more investigation.
const unsigned long video_tick_inc = 10000;
unsigned long video_freq;
unsigned long video_frame_count;

static int raspi_boot_warp = 1;




static int tela_ridisegnata = 1;

// Should be set only when raster_skip=true is present
// in the kernel args.
int raster_lines;
int raster2_lines;

#define COLOR16(r,g,b) (((r)>>3)<<11 | ((g)>>2)<<5 | (b)>>3)

int is_vic(struct video_canvas_s *canvas) {
  return canvas == vic_canvas;
}

int is_vdc(struct video_canvas_s *canvas) {
  return canvas == vdc_canvas;
}

// Called by menu when palette changes
void emux_change_palette(int display_num, int palette_index) {
  canvas_state[display_num].palette_index = palette_index;
  // This will call set_palette below to get called after color controls
  // have been applied to the palette.

  // Unless we set the filter to something other than NONE, it looks we
  // don't get any updates for color settings changed. Bug in VICE?
  // We will temporarily switch to CRT, then switch back just to get
  // the updates working.
  int current_filter = get_filter(display_num);
  set_filter(display_num, VIDEO_FILTER_CRT);
  video_color_update_palette(canvases[display_num]);
  set_filter(display_num, current_filter);
}

// Called when a color setting has changed
void emux_video_color_setting_changed(int display_num) {
  // This will call set_palette below to get called after color controls
  // have been applied to the palette.
  // See above for temp filter change here.
  int current_filter = get_filter(display_num);
  set_filter(display_num, VIDEO_FILTER_CRT);
  video_color_update_palette(canvases[display_num]);
  set_filter(display_num, current_filter);
}

int video_canvas_set_palette(struct video_canvas_s *canvas, palette_t *p) {
  canvas->palette = p;
  int layer;

  if (is_vic(canvas)) {
    layer = FB_LAYER_VIC;
    for (int i = 0; i < p->num_entries; i++) {
      circle_set_palette_fbl(layer, i,
                     COLOR16(p->entries[i].red, p->entries[i].green,
                             p->entries[i].blue));
    }
  } else {
    layer = FB_LAYER_VDC;
    for (int i = 0; i < 16; i++) {
      circle_set_palette_fbl(layer, i,
                     COLOR16(p->entries[i].red, p->entries[i].green,
                             p->entries[i].blue));
    }
  }

  circle_update_palette_fbl(layer);




  return 0;
}

static void check_dimensions(struct video_canvas_s* canvas,
                             int canvas_index,
                             int fb_width, int fb_height,
                             int rlines) {
   if (canvas_state[canvas_index].fb_width != fb_width ||
       canvas_state[canvas_index].fb_height != fb_height) {
      // width/height has changed
      int tx, ty;
      set_canvas_size(canvas_index, &tx, &ty,
         &canvas_state[canvas_index].gfx_w,
         &canvas_state[canvas_index].gfx_h);

      ty *= canvas->raster_skip;
      canvas_state[canvas_index].gfx_h *= canvas->raster_skip;

      canvas->draw_buffer->canvas_physical_width = tx;
      canvas->draw_buffer->canvas_physical_height = ty;

      set_canvas_borders(canvas_index,
                         &canvas_state[canvas_index].max_border_w,
                         &canvas_state[canvas_index].max_border_h);

      canvas_state[canvas_index].max_border_h *= canvas->raster_skip;
   }
   canvas_state[canvas_index].fb_width = fb_width;
   canvas_state[canvas_index].fb_height = fb_height;

   canvas_state[canvas_index].extra_offscreen_border_left =
     canvas->geometry->extra_offscreen_border_left;
   canvas_state[canvas_index].extra_offscreen_border_right =
     canvas->geometry->extra_offscreen_border_right;
   canvas_state[canvas_index].first_displayed_line =
     canvas->geometry->first_displayed_line;
   canvas_state[canvas_index].last_displayed_line =
     canvas->geometry->last_displayed_line;

   int max_padding_w = MIN(
        canvas_state[canvas_index].extra_offscreen_border_left,
        canvas_state[canvas_index].extra_offscreen_border_right);
   int max_padding_h = canvas_state[canvas_index].first_displayed_line;

   canvas_state[canvas_index].max_padding_w = max_padding_w;
   canvas_state[canvas_index].max_padding_h = max_padding_h;

   // If config says raster lines, do it here.
   canvas->raster_lines |= rlines;
}

// Draw buffer bridge functions back to kernel
static int draw_buffer_alloc(struct video_canvas_s *canvas,
                             uint8_t **draw_buffer,
                             unsigned int fb_width, unsigned int fb_height,
                             unsigned int *fb_pitch) {
   int status;
   if (is_vdc(canvas)) {
      check_dimensions(canvas, VDC_INDEX, fb_width,
                          fb_height * canvas->raster_skip, raster2_lines);
      status = circle_alloc_fbl(FB_LAYER_VDC, 0 /* indexed */, draw_buffer,
                              fb_width, fb_height * canvas->raster_skip,
                              fb_pitch);
      emux_frame_buffer_changed(FB_LAYER_VDC);
   } else {
      check_dimensions(canvas, VIC_INDEX, fb_width,
                          fb_height * canvas->raster_skip, raster_lines);
      status = circle_alloc_fbl(FB_LAYER_VIC, 0 /* indexed */, draw_buffer,
                              fb_width, fb_height * canvas->raster_skip,
                              fb_pitch);
      emux_frame_buffer_changed(FB_LAYER_VIC);
   }









   if (status == 0) {
      canvas->draw_buffer->draw_buffer_non_padded[0] = *draw_buffer;
      canvas->draw_buffer->draw_buffer_non_padded[1] = *draw_buffer;
   }

   return status;
}

static void draw_buffer_free(struct video_canvas_s *canvas, uint8_t *draw_buffer) {
   if (is_vdc(canvas)) {
      circle_free_fbl(FB_LAYER_VDC);
      vdc_showing = 0;
   } else {
      circle_free_fbl(FB_LAYER_VIC);
      vic_showing = 0;
   }
}

static void draw_buffer_clear(struct video_canvas_s *canvas, uint8_t *draw_buffer,
                              uint8_t value, unsigned int fb_width,
                              unsigned int fb_height, unsigned int fb_pitch) {
   if (is_vdc(canvas)) {
      circle_clear_fbl(FB_LAYER_VDC);
   } else {
      circle_clear_fbl(FB_LAYER_VIC);
   }
}

// Called for each canvas VICE wants to create.
// For C128, first will be the VDC, followed by VIC.
// For other machines, only one canvas is initialized.
void video_arch_canvas_init(struct video_canvas_s *canvas) {
  static int canvas_num = 0;
  int canvas_index;
  if (machine_class == VICE_MACHINE_C128 && canvas_num == 1) {
     vdc_canvas = canvas;
     vdc_first_refresh = 1;
     vdc_enabled = 0;
     vdc_showing = 0;
     canvas_index = 1;
  } else {
     set_refresh_rate(canvas);
     vic_first_refresh = 1;
     vic_canvas = canvas;
     video_freq = canvas->refreshrate * video_tick_inc;
     vic_enabled = 1;
     vic_showing = 0;
     canvas_index = 0;
  }
  canvas_num++;

  canvas->raster_skip = canvas_state[canvas_index].raster_skip;

  // Have our fb class allocate draw buffers
  draw_buffer_callback[canvas_index].draw_buffer_alloc =
     draw_buffer_alloc;
  draw_buffer_callback[canvas_index].draw_buffer_free =
     draw_buffer_free;
  draw_buffer_callback[canvas_index].draw_buffer_clear =
     draw_buffer_clear;
  canvas->video_draw_buffer_callback =
     &draw_buffer_callback[canvas_index];
}

static struct video_canvas_s *video_canvas_create_vic(
       struct video_canvas_s *canvas,
       unsigned int *width,
       unsigned int *height, int mapped) {

  *height = *height * canvas_state[VIC_INDEX].raster_skip;

  canvas->draw_buffer->canvas_physical_width = *width;
  canvas->draw_buffer->canvas_physical_height = *height;
  canvas->videoconfig->external_palette = 1;
  canvas->videoconfig->external_palette_name = "RASPI";

  return canvas;
}

static struct video_canvas_s *video_canvas_create_vdc(
       struct video_canvas_s *canvas,
       unsigned int *width,
       unsigned int *height, int mapped) {
  assert(machine_class == VICE_MACHINE_C128);

  *height = *height * canvas_state[VDC_INDEX].raster_skip;

  canvas->draw_buffer->canvas_physical_width = *width;
  canvas->draw_buffer->canvas_physical_height = *height;
  canvas->videoconfig->external_palette = 1;
  canvas->videoconfig->external_palette_name = "RASPI2";

  return canvas;
}

struct video_canvas_s *video_canvas_create(struct video_canvas_s *canvas,
                                           unsigned int *width,
                                           unsigned int *height, int mapped) {
  if (is_vic(canvas)) {
     canvases[0] = canvas;
     return video_canvas_create_vic(canvas, width, height, mapped);
  }
  canvases[1] = canvas;
  return video_canvas_create_vdc(canvas, width, height, mapped);
}

void video_canvas_refresh(struct video_canvas_s *canvas, unsigned int xs,
                          unsigned int ys, unsigned int xi, unsigned int yi,
                          unsigned int w, unsigned int h) {
  // We draw full frames each time so there's little to do here. Just turn on
  // boot warp on first refresh.
  tela_ridisegnata = 1;
  if (is_vic(canvas)) {
     if (vic_first_refresh == 1) {
        vsync_set_warp_mode(1);
        raspi_boot_warp = 1;
        vic_first_refresh = 0;
        set_video_font();
     }
  } else {
     if (vdc_first_refresh == 1) {
        vdc_first_refresh = 0;
     }
  }
}

unsigned long vsyncarch_frequency(void) { return video_freq; }

unsigned long vsyncarch_gettime(void) { return video_ticks; }



//




//





//






#define METER_WINDOW 50



static unsigned long pace_next_us = 0;




static unsigned long pace_period_us = 20000UL;


extern volatile unsigned long fbl_periodo_macchina_us;

static unsigned long meter_emu_start = 0;
static unsigned long meter_frame_start = 0;
static unsigned long meter_emu_sum = 0;
static unsigned long meter_frame_sum = 0;
static unsigned long meter_emu_max = 0;
static unsigned meter_frames = 0;
static unsigned meter_late = 0;




static void meter_frame_done(unsigned long emu_us, unsigned long frame_us) {




  unsigned long budget = pace_period_us ? pace_period_us : 20000UL;

  meter_emu_sum += emu_us;
  meter_frame_sum += frame_us;
  if (emu_us > meter_emu_max) {
    meter_emu_max = emu_us;
  }
  if (emu_us > budget) {
    meter_late++;
  }
  meter_frames++;

  if (meter_frames >= METER_WINDOW) {
    raspi_meter_emu_avg_us = meter_emu_sum / meter_frames;
    raspi_meter_frame_avg_us = meter_frame_sum / meter_frames;
    raspi_meter_emu_max_us = meter_emu_max;
    raspi_meter_late_pct = (meter_late * 100) / meter_frames;



    raspi_meter_speed_pct = raspi_meter_frame_avg_us
                                ? (unsigned)((budget * 100) /
                                             raspi_meter_frame_avg_us)
                                : 0;
    raspi_meter_speed_periodo_pct = raspi_meter_speed_pct;







    {
      static CLOCK clk_prima = 0;
      const CLOCK clk_ora = maincpu_clk;
      const long cps = machine_get_cycles_per_second();
      if (clk_prima != 0 && clk_ora > clk_prima && cps > 0 &&
          meter_frame_sum > 0) {
        raspi_meter_speed_pct =
            (unsigned)(((double)(clk_ora - clk_prima) * 100000000.0) /
                       ((double)cps * (double)meter_frame_sum));
      }
      clk_prima = clk_ora;
    }
    raspi_meter_fps_x10 = raspi_meter_frame_avg_us
                              ? (unsigned)(10000000UL /
                                           raspi_meter_frame_avg_us)
                              : 0;

    meter_emu_sum = 0;
    meter_frame_sum = 0;
    meter_emu_max = 0;
    meter_late = 0;
    meter_frames = 0;
  }
}

void vsyncarch_init(void) {
}




int video_arch_get_active_chip(void) {
  if (machine_class == VICE_MACHINE_C128 && menu_active_display_is_vdc()) {
    return VIDEO_CHIP_VDC;
  }
  return VIDEO_CHIP_VICII;
}





#define JOYPAD_FIRE2_BIT 0x20
#define JOYPAD_FIRE3_BIT 0x40

static int valore_digitale(int v) {
  int d = v & 0x1f;

  if (v & FIRE2_BIT_MASK) {
    d |= JOYPAD_FIRE2_BIT;
  }
  if (v & FIRE3_BIT_MASK) {
    d |= JOYPAD_FIRE3_BIT;
  }

  return d;
}

void vsyncarch_presync(void) {
  bmc_giri[1]++;
  bmc_dove = DOVE_SINCRONIA;


  if (bmc_blocco_scritto) {
    bmc_blocco_scritto = 0;
    passo_nota("ripartito: BLOCCO.TXT era un'attesa lunga");
  }
  kbdbuf_flush();


  emux_real_drive_tick();
}































































#define DISCHETTI_OGNI_FOTOGRAMMI 30

extern int circle_file_da_sincronizzare(int fildes);



static int stdio_in_sospeso(FILE *fd) {
  return (fd->_flags & __SWR) != 0 && fd->_bf._base != NULL &&
         fd->_p > fd->_bf._base;
}

static void dischetti_sulla_scheda(void) {
  int modo = menu_get_drive_flush();
  unsigned int u, d;

  if (modo == DRIVE_FLUSH_ON_DETACH) {
    return;
  }
  for (u = 0; u < NUM_DISK_UNITS; u++) {
    diskunit_context_t *unita = diskunit_context[u];
    if (unita == NULL) {
      continue;
    }
    for (d = 0; d < NUM_DRIVES; d++) {
      drive_t *drive = unita->drives[d];
      disk_image_t *image;
      FILE *fd;
      unsigned long da;
      int fatto = 0;

      if (drive == NULL) {
        continue;
      }


      image = file_system_get_image(u + 8, d);
      if (image == NULL || image->device != DISK_IMAGE_DEVICE_FS ||
          image->read_only) {
        continue;
      }

      if (drive->read_write_mode == 0 || (drive->led_status & 1)) {
        continue;
      }
      da = circle_get_ticks();
      if (drive->image != NULL && drive->GCR_dirty_track) {
        drive_gcr_data_writeback(drive);
        fatto = 1;
      }



      if (d == 0 && unita->wd1770 != NULL) {
        wd1770_flush(unita->wd1770);
      }
      fd = (FILE *)fsimage_fd_get(image);
      if (fd == NULL) {
        continue;
      }
      if (stdio_in_sospeso(fd)) {
        fflush(fd);
        fatto = 1;
      }
      if (circle_file_da_sincronizzare(fileno(fd))) {
        fsync(fileno(fd));
        fatto = 1;
      }
      if (!fatto) {
        continue;
      }




      if (modo == DRIVE_FLUSH_ON_WRITE_LOGGED) {
        log_message(LOG_DEFAULT, "BMC64: disk unit %u drive %u flushed in %lu us",
                    u + 8, d, circle_get_ticks() - da);
      }
    }
  }
}

void vsyncarch_postsync(void) {




  uint32_t ready_mask = 0;
  bmc_dove = DOVE_DOPO;




#ifdef HAVE_REALDEVICE


  raspi_opencbm_tick();
#endif


  if (ui_lettore_sid_tick()) {
    ready_mask |= FB_LAYER_MASK(FB_LAYER_UI);
  }
  emux_ensure_video();

  // This render will handle any OSDs we have. ODSs don't pause emulation.
  if (ui_enabled) {
    // The only way we can be here and have ui_enabled=1
    // is for an osd to be enabled.
    ui_render_now(-1); // only render top most menu
    ready_mask |= FB_LAYER_MASK(FB_LAYER_UI);
    ui_check_key();
  }

  if (statusbar_showing || vkbd_showing) {
    overlay_check();
    if (overlay_dirty) {
       ready_mask |= FB_LAYER_MASK(FB_LAYER_STATUS);
       overlay_dirty = 0;
    }
  }

  video_ticks += video_tick_inc;

  // This yield is important to let the fake kernel 'threads' run.
  circle_yield();

  video_frame_count++;
  if (video_frame_count == 1) {
    passo_nota("primo fotogramma");
  }
  if (raspi_boot_warp && video_frame_count > 120) {
    raspi_boot_warp = 0;
    circle_boot_complete();
    vsync_set_warp_mode(0);
  }




  if (!raspi_boot_warp && video_frame_count == 140) {
    menu_vidtrial_start();
  }
  menu_vidtrial_tick();
  menu_poweroff_tick();


  if (video_frame_count % DISCHETTI_OGNI_FOTOGRAMMI == 0) {
    dischetti_sulla_scheda();
  }

  // Hold for vsync unless warping or in boot warp.
  int raspi_warp;
  raspi_warp = vsync_get_warp_mode();



  unsigned long meter_emu_end = circle_get_ticks();






  {


    int mostra = (!raspi_boot_warp && !raspi_warp) || tela_ridisegnata;
    tela_ridisegnata = 0;
    if (mostra) {
      ready_mask |= FB_LAYER_MASK(FB_LAYER_VIC);
      if (machine_class == VICE_MACHINE_C128) {
        ready_mask |= FB_LAYER_MASK(FB_LAYER_VDC);
      }



    }


    if (ready_mask != 0) {
      int sync = !raspi_boot_warp && !raspi_warp;
      if (ready_mask & (FB_LAYER_MASK(FB_LAYER_UI) |
                        FB_LAYER_MASK(FB_LAYER_STATUS))) {
        sync = 1;
      }
      circle_present_fbl(ready_mask, sync);
    }
  }



  //





  //

  //









  //



  {
    unsigned long period = 20000UL;
    if (vic_canvas != NULL && vic_canvas->refreshrate > 1.0) {
      period = (unsigned long)(1000000.0 / vic_canvas->refreshrate + 0.5);
    }


    pace_period_us = period;


    fbl_periodo_macchina_us = period;
  }

  if (!raspi_boot_warp && !raspi_warp) {
    unsigned long period = pace_period_us;

    if (pace_next_us == 0) {
      pace_next_us = circle_get_ticks() + period;
    } else {
      while ((long)(pace_next_us - circle_get_ticks()) > 0) {

        circle_yield();
      }
      pace_next_us += period;




      if ((long)(circle_get_ticks() - pace_next_us) > (long)(period * 4)) {
        pace_next_us = circle_get_ticks() + period;
      }
    }
  } else {
    pace_next_us = 0;
  }

  {
    unsigned long now = circle_get_ticks();






    if (meter_emu_start != 0 && !raspi_boot_warp) {
      meter_frame_done(meter_emu_end - meter_emu_start, now - meter_frame_start);
    }
    meter_emu_start = now;
    meter_frame_start = now;
  }

  circle_check_gpio();

  int reset_demo = 0;

  // Do key press/releases and joy latches on the main loop.
  circle_lock_acquire();
  while (pending_emu_key.head != pending_emu_key.tail) {
    int i = pending_emu_key.head & PENDING_EMU_KEY_MASK;
    reset_demo = 1;
    if (vkbd_enabled) {
      // Kind of nice to have virtual keyboard's state
      // stay in sync with changes happening from USB
      // key events.
      vkbd_sync_event(pending_emu_key.key[i], pending_emu_key.pressed[i]);
    }
    if (pending_emu_key.pressed[i]) {


      keyboard_key_pressed(pending_emu_key.key[i], 0);
    } else {
      keyboard_key_released(pending_emu_key.key[i], 0);
    }
    pending_emu_key.head++;
  }



















  if (kbd_aspetta_ancora()) {
    if (alza_i_tasti < 3) {
      alza_i_tasti = 3;
    }
    if (++quanto_aspetto == 600) {
      log_message(LOG_DEFAULT,
                  "reset: un modificatore risulta premuto da 10 secondi, "
                  "smetto di tenere pulita la matrice");
    }
    if (quanto_aspetto >= 600) {
      alza_i_tasti = 0;
    }
  } else {
    quanto_aspetto = 0;
  }

  if (alza_i_tasti > 0) {
    alza_i_tasti--;
    keyboard_clear_keymatrix();
  }




  if (pending_emu_key.lost) {
    pending_emu_key.lost = 0;
    keyboard_clear_keymatrix();
    log_message(LOG_DEFAULT,
                "key queue overflowed, all keys lifted");
  }

  while (pending_emu_joy.head != pending_emu_joy.tail) {
    int i = pending_emu_joy.head & 0x7f;
    reset_demo = 1;
    if (vkbd_enabled) {
      int value = pending_emu_joy.value[i];
      int devd = pending_emu_joy.device[i];


      if (devd < 0 || devd >= JOYDEV_NUM_JOYDEVS) {
        pending_emu_joy.head++;
        continue;
      }
      switch (pending_emu_joy.type[i]) {
      case PENDING_EMU_JOY_TYPE_ABSOLUTE:
        if (!vkbd_press[devd]) {
           if (value & 0x1 && !vkbd_up[devd]) {
             vkbd_up[devd] = 1;
             vkbd_nav_up();
           } else if (!(value & 0x1) && vkbd_up[devd]) {
             vkbd_up[devd] = 0;
           }
           if (value & 0x2 && !vkbd_down[devd]) {
             vkbd_down[devd] = 1;
             vkbd_nav_down();
           } else if (!(value & 0x2) && vkbd_down[devd]) {
             vkbd_down[devd] = 0;
           }
           if (value & 0x4 && !vkbd_left[devd]) {
             vkbd_left[devd] = 1;
             vkbd_nav_left();
           } else if (!(value & 0x4) && vkbd_left[devd]) {
             vkbd_left[devd] = 0;
           }
           if (value & 0x8 && !vkbd_right[devd]) {
             vkbd_right[devd] = 1;
             vkbd_nav_right();
           } else if (!(value & 0x8) && vkbd_right[devd]) {
             vkbd_right[devd] = 0;
           }
        }
        if (value & 0x10 && !vkbd_press[devd]) vkbd_nav_press(1, devd);
        else if (!(value & 0x10) && vkbd_press[devd]) vkbd_nav_press(0, devd);
        break;
      }
    } else {






      int jport = pending_emu_joy.port[i] - 1;
      if (jport < 0) {


        pending_emu_joy.head++;
        continue;
      }
      switch (pending_emu_joy.type[i]) {
      case PENDING_EMU_JOY_TYPE_ABSOLUTE:
        joystick_set_value_absolute(jport,
                                  valore_digitale(pending_emu_joy.value[i]));
        joystick_set_potx(jport,
			  (pending_emu_joy.value[i] & POTX_BIT_MASK) >> 5);
        joystick_set_poty(jport,
			  (pending_emu_joy.value[i] & POTY_BIT_MASK) >> 13);
        if (pending_emu_joy.device[i] == JOYDEV_SPINNER) {






          int paddle = (pending_emu_joy.value[i] & POTX_BIT_MASK) >> 5;
          joystick_set_paddle_value(jport, (uint8_t)paddle);
        }
        break;
      case PENDING_EMU_JOY_TYPE_AND:
        joystick_set_value_and(jport,
                             valore_digitale(pending_emu_joy.value[i]));
        joystick_set_potx_and(jport,
			  (pending_emu_joy.value[i] & POTX_BIT_MASK) >> 5);
        joystick_set_poty_and(jport,
			  (pending_emu_joy.value[i] & POTY_BIT_MASK) >> 13);
        break;
      case PENDING_EMU_JOY_TYPE_OR:
        joystick_set_value_or(jport,
                            valore_digitale(pending_emu_joy.value[i]));
        joystick_set_potx_or(jport,
			  (pending_emu_joy.value[i] & POTX_BIT_MASK) >> 5);
        joystick_set_poty_or(jport,
			  (pending_emu_joy.value[i] & POTY_BIT_MASK) >> 13);
        break;
      default:
        break;
      }
    }
    pending_emu_joy.head++;
  }
  circle_lock_release();

  ui_handle_toggle_or_quick_func();

  if (reset_demo) {
    demo_reset_timeout();
  }

  if (raspi_demo_mode) {
    demo_check();
  }
  bmc_dove = DOVE_EMULA;
}

void vsyncarch_sleep(unsigned long delay) {
  bmc_dove = DOVE_ATTESA;
  // We don't sleep here. Instead, our pace is governed by the
  // wait for vertical blank in vsyncarch_postsync above. This
  // times our machine properly.
}

// Called by our special hook in vice to load palettes from
// memory.
palette_t *raspi_video_load_palette(int num_entries, char *name) {
  palette_t *palette = palette_create(num_entries, NULL);
  unsigned int *pal;
  // RASPI2 is for VDC
  if (strcmp(name, "RASPI2") == 0) {
     pal = raspi_get_palette(1, canvas_state[1].palette_index);
  } else {
     pal = raspi_get_palette(0, canvas_state[0].palette_index);
  }
  for (int i = 0; i < num_entries; i++) {
    palette->entries[i].red = pal[i * 3];
    palette->entries[i].green = pal[i * 3 + 1];
    palette->entries[i].blue = pal[i * 3 + 2];

  }
  return palette;
}

void set_raster_lines(int v, int v2) {
  raster_lines = v;
  raster2_lines = v2;
}

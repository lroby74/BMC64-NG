/*
 * menu_switch.c
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

#include "menu_switch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "circle.h"

#define OPTION_SCRATCH_LEN (KEY_LEN+1+VALUE_LEN+1)

#define ERROR_1 1
#define ERROR_2 2
#define ERROR_3 4
#define ERROR_4 8
#define ERROR_5 16
#define ERROR_6 32
#define ERROR_7 64
#define ERROR_8 128
#define ERROR_9 256
#define ERROR_10 512
#define ERROR_11 1024





#define DEFAULT_GPU_MEM 64

struct s_cfg_flags {
  // Have flags for cmdline.txt
  int have_cycles_per_second;
  int have_machine_timing;
  int have_serial;
  int have_demo;
  int have_audio_out;
  int have_disk_partition;
  int have_enable_dpi;
  int have_scaling_params;
  int have_scaling_params2;
  int have_raster_skip;
  int have_raster_skip2;
  int have_fb_size;
  int have_fb_display;
  int have_fb_pages;
  int have_hdmi_group;
  int have_hdmi_mode;

  // Have flags for config.txt
  int have_kernel;
  int have_gpu_mem;
  int have_hdmi_timings;
  int have_cvt;
  int have_dpi_timings;
  int have_enable_dpi_lcd;
  int have_display_default_lcd;
  int have_dpi_group;
  int have_dpi_mode;
  int have_dpi_output_format;

  // Since kernel comes and goes, need this flag
  // to know whether it should be added if not
  // already present to config.txt
  int need_kernel;
};

static int entry_id;

static int next_id() {
   return entry_id++;
}

static void rtrim(char *txt) {
  if (!txt) return;
  int p=strlen(txt)-1;
  while (isspace(txt[p])) { txt[p] = '\0'; p--; }
}

static char* ltrim(char *txt) {
  if (!txt) return NULL;
  int p=0;
  while (isspace(txt[p])) { p++; }
  return txt+p;
}

static char* trim(char *txt) {
  if (!txt) return NULL;
  rtrim(txt);
  return ltrim(txt);
}

static int copy_file(char* from, char* to) {
  FILE *fp = fopen(from,"r");
  if (fp == NULL) {
     return 1;
  }
  FILE *fp2 = fopen(to,"w");
  if (fp2 == NULL) {
     fclose(fp);
     return 1;
  }
  int c = fgetc(fp);
  while (c != EOF) {
    fputc(c, fp2);
    c = fgetc(fp);
  }
  fclose(fp);
  fclose(fp2);
  return 0;
}





static int aggiungi(char *dst, size_t max, const char *src) {
  size_t n = strlen(dst);
  size_t m = strlen(src);
  if (n + m + 1 > max) {
    return 0;
  }
  memcpy(dst + n, src, m + 1);
  return 1;
}




#define VIDTRIAL_MARK "/vidtrial.txt"
#define VIDTRIAL_CONFIG "/config.pre"
#define VIDTRIAL_CMDLINE "/cmdline.pre"



static int vidtrial_enabled = 1;

static int vidtrial_arm(void) {
   if (copy_file("/config.txt", VIDTRIAL_CONFIG)) {
      return ERROR_9;
   }
   if (copy_file("/cmdline.txt", VIDTRIAL_CMDLINE)) {
      return ERROR_9;
   }
   FILE* fp = fopen(VIDTRIAL_MARK, "w");
   if (fp == NULL) {
      return ERROR_8;
   }
   fprintf(fp, "video trial\n");
   fclose(fp);
   return 0;
}

int vidtrial_pending(void) {
   FILE* fp = fopen(VIDTRIAL_MARK, "r");
   if (fp == NULL) {
      return 0;
   }
   fclose(fp);
   return 1;
}

int vidtrial_accept(void) {
   unlink(VIDTRIAL_MARK);
   unlink(VIDTRIAL_CONFIG);
   unlink(VIDTRIAL_CMDLINE);
   return 0;
}

int vidtrial_revert(void) {
   int status = 0;
   if (copy_file(VIDTRIAL_CONFIG, "/config.txt")) {
      status |= ERROR_9;
   }
   if (copy_file(VIDTRIAL_CMDLINE, "/cmdline.txt")) {
      status |= ERROR_9;
   }
   unlink(VIDTRIAL_MARK);
   unlink(VIDTRIAL_CONFIG);
   unlink(VIDTRIAL_CMDLINE);
   return status;
}






#define VIDTRIAL_OFF "/switch-timer-off.txt"

int vidtrial_timer_on(void) {
   FILE* fp = fopen(VIDTRIAL_OFF, "r");
   if (fp == NULL) {
      return 1;
   }
   fclose(fp);
   return 0;
}

int vidtrial_timer_set(int on) {
   if (on) {
      unlink(VIDTRIAL_OFF);
      return vidtrial_timer_on() ? 0 : ERROR_10;
   }
   FILE* fp = fopen(VIDTRIAL_OFF, "w");
   if (fp == NULL) {
      return ERROR_8;
   }
   fprintf(fp, "BMC64-NG: the 15 second safety timer of Switch is OFF.\n"
               "Delete this file, or set Switch > Safety timer to On,\n"
               "to turn it back on.\n");
   fclose(fp);
   return 0;
}

static int new_section(struct machine_entry** new_section, char* line) {
  char* header = &line[1];
  for (int i=0;i<strlen(header);i++) {
    if (header[i]==']') { header[i] = '\0'; break; }
  }
  header = trim(header);

  char *video_nam = trim(strtok(header, "/"));
  if (video_nam == NULL) return 1;
  char *video_std = trim(strtok(NULL, "/"));
  if (video_std == NULL) return 1;
  char *video_out = trim(strtok(NULL, "/"));
  if (video_out == NULL) return 1;
  char *video_res = trim(strtok(NULL, "/"));
  if (video_res == NULL) return 1;

  struct machine_entry* entry =
     (struct machine_entry*) malloc(sizeof(struct machine_entry));

  char desc[DESC_LEN];
  entry->id = next_id();
  entry->class = BMC64_MACHINE_CLASS_UNKNOWN;
  entry->variante = 0;
  entry->custom = 0;
  entry->video_standard = BMC64_VIDEO_STANDARD_UNKNOWN;
  entry->video_out = BMC64_VIDEO_OUT_UNKNOWN;

  strcpy(entry->desc,video_std);
  strcat(entry->desc," ");
  strcat(entry->desc,video_out);
  strcat(entry->desc," ");
  strcat(entry->desc,video_res);

  entry->options = NULL;
  entry->next = NULL;

  if (strcasecmp(video_nam,"vic20") == 0)
     entry->class = BMC64_MACHINE_CLASS_VIC20;
  else if (strcasecmp(video_nam,"c64") == 0)
     entry->class = BMC64_MACHINE_CLASS_C64;
  else if (strcasecmp(video_nam,"c128") == 0)
     entry->class = BMC64_MACHINE_CLASS_C128;
  else if (strcasecmp(video_nam,"plus4") == 0)
     entry->class = BMC64_MACHINE_CLASS_PLUS4;
  else if (strcasecmp(video_nam,"plus4emu") == 0)
     entry->class = BMC64_MACHINE_CLASS_PLUS4EMU;
  else if (strcasecmp(video_nam,"pet") == 0)
     entry->class = BMC64_MACHINE_CLASS_PET;
  else if (strcasecmp(video_nam,"scpu64") == 0)
     entry->class = BMC64_MACHINE_CLASS_SCPU64;
  else if (strcasecmp(video_nam,"x64") == 0) {


     entry->class = BMC64_MACHINE_CLASS_C64;
     entry->variante = 1;
  }

  if (strcasecmp(video_std,"ntsc") == 0)
     entry->video_standard = BMC64_VIDEO_STANDARD_NTSC;
  else if (strcasecmp(video_std,"pal") == 0)
     entry->video_standard = BMC64_VIDEO_STANDARD_PAL;

  if (strcasecmp(video_out,"hdmi") == 0)
     entry->video_out = BMC64_VIDEO_OUT_HDMI;
  else if (strcasecmp(video_out,"composite") == 0)
     entry->video_out = BMC64_VIDEO_OUT_COMPOSITE;
  else if (strcasecmp(video_out,"dpi") == 0)
     entry->video_out = BMC64_VIDEO_OUT_DPI;

  *new_section = entry;
  return 0;
}

static int append_to_section(struct machine_entry* section, char* line) {
  if (!section) return 0;

  line = trim(line);
  if (strlen(line) == 0) return 0;

  char *key = trim(strtok(line, "="));
  char *value = trim(strtok(NULL, "="));

  if (key == NULL || value == NULL) {
    return 1;
  }






  if (strcasecmp(key, "machine_timing") == 0 &&
      strstr(value, "-custom") != NULL) {
    section->custom = 1;
  }

  struct machine_option* option =
     (struct machine_option*) malloc(sizeof(struct machine_option));

  strncpy (option->key, key, KEY_LEN-1);
  strncpy (option->value, value, VALUE_LEN-1);
  option->next = NULL;

  struct machine_option* prev = NULL;
  struct machine_option* ptr = section->options;
  while (ptr) {
     prev = ptr;
     ptr = ptr->next;
  }
  if (prev)
    prev->next = option;
  else
    section->options = option;

  return 0;
}











static const char *const COMPOSITI_INCORPORATI[] = {
  "[VIC20/NTSC/Composite/480p@60Hz]",
  "disable_overscan=1",
  "sdtv_mode=16",
  "hdmi_group=1",
  "hdmi_mode=4",
  "audio_out=analog",
  "machine_timing=ntsc-composite",
  "scaling_params=422,228,720,456",
  "[VIC20/PAL/Composite/576p@50Hz]",
  "disable_overscan=1",
  "sdtv_mode=18",
  "hdmi_group=1",
  "hdmi_mode=19",
  "audio_out=analog",
  "machine_timing=pal-composite",
  "scaling_params=422,240,668,480",
  "[C64/NTSC/Composite/X64SC 480p@60Hz]",
  "disable_overscan=1",
  "sdtv_mode=16",
  "hdmi_group=1",
  "hdmi_mode=4",
  "audio_out=analog",
  "machine_timing=ntsc-composite",
  "scaling_params=364,240,600,480",
  "[C64/PAL/Composite/X64SC 576p@50Hz]",
  "disable_overscan=1",
  "sdtv_mode=18",
  "hdmi_group=1",
  "hdmi_mode=19",
  "audio_out=analog",
  "machine_timing=pal-composite",
  "scaling_params=384,272,648,544",
  "[C128/NTSC/Composite/480p@60Hz]",
  "disable_overscan=1",
  "sdtv_mode=16",
  "hdmi_group=1",
  "hdmi_mode=4",
  "audio_out=analog",
  "machine_timing=ntsc-composite",
  "scaling_params=364,240,640,480",
  "[C128/PAL/Composite/576p@50Hz]",
  "disable_overscan=1",
  "sdtv_mode=18",
  "hdmi_group=1",
  "hdmi_mode=19",
  "audio_out=analog",
  "machine_timing=pal-composite",
  "scaling_params=384,272,648,544",
  "[PLUS4/NTSC/Composite/480p@60Hz]",
  "disable_overscan=1",
  "sdtv_mode=16",
  "hdmi_group=1",
  "hdmi_mode=4",
  "audio_out=analog",
  "machine_timing=ntsc-composite",
  "scaling_params=360,232,640,464",
  "[PLUS4/PAL/Composite/576p@50Hz]",
  "disable_overscan=1",
  "sdtv_mode=18",
  "hdmi_group=1",
  "hdmi_mode=19",
  "audio_out=analog",
  "machine_timing=pal-composite",
  "scaling_params=364,280,640,560",
  "[PLUS4EMU/NTSC/Composite/Plus4Emu 480p@60Hz]",
  "disable_overscan=1",
  "sdtv_mode=16",
  "hdmi_group=1",
  "hdmi_mode=4",
  "audio_out=analog",
  "machine_timing=ntsc-composite",
  "scaling_params=364,240,640,480",
  "[PLUS4EMU/PAL/Composite/Plus4Emu 576p@50Hz]",
  "disable_overscan=1",
  "sdtv_mode=18",
  "hdmi_group=1",
  "hdmi_mode=19",
  "audio_out=analog",
  "machine_timing=pal-composite",
  "scaling_params=364,272,640,544",
  "[PET/NTSC/Composite/480p@60Hz]",
  "disable_overscan=1",
  "sdtv_mode=16",
  "hdmi_group=1",
  "hdmi_mode=4",
  "audio_out=analog",
  "machine_timing=ntsc-composite",
  "[PET/PAL/Composite/576p@50Hz]",
  "disable_overscan=1",
  "sdtv_mode=18",
  "hdmi_group=1",
  "hdmi_mode=19",
  "audio_out=analog",
  "machine_timing=pal-composite",
  "[X64/NTSC/Composite/Std 480p@60Hz]",
  "disable_overscan=1",
  "sdtv_mode=16",
  "hdmi_group=1",
  "hdmi_mode=4",
  "audio_out=analog",
  "machine_timing=ntsc-composite",
  "scaling_params=364,240,600,480",
  "[X64/PAL/Composite/Std 576p@50Hz]",
  "disable_overscan=1",
  "sdtv_mode=18",
  "hdmi_group=1",
  "hdmi_mode=19",
  "audio_out=analog",
  "machine_timing=pal-composite",
  "scaling_params=384,272,648,544",
  "[SCPU64/NTSC/Composite/480p@60Hz]",
  "disable_overscan=1",
  "sdtv_mode=16",
  "hdmi_group=1",
  "hdmi_mode=4",
  "audio_out=analog",
  "machine_timing=ntsc-composite",
  "scaling_params=364,240,600,480",
  "[SCPU64/PAL/Composite/576p@50Hz]",
  "disable_overscan=1",
  "sdtv_mode=18",
  "hdmi_group=1",
  "hdmi_mode=19",
  "audio_out=analog",
  "machine_timing=pal-composite",
  "scaling_params=384,272,648,544",
  NULL
};



static int compositi_della_macchina(struct machine_entry *primo,
                                    struct machine_entry *coda,
                                    int classe, int variante, int *ha_voci) {
  int composito = 0;
  struct machine_entry *p;
  *ha_voci = 0;
  for (p = primo; p != NULL; p = p->next) {
    if (p->class == classe && p->variante == variante) {
      *ha_voci = 1;
      if (p->video_out == BMC64_VIDEO_OUT_COMPOSITE) {
        composito = 1;
      }
    }
    if (p == coda) {
      break;
    }
  }
  return composito;
}

static void aggiungi_compositi(struct machine_entry *primo,
                               struct machine_entry *coda) {
  struct machine_entry *ultima = coda;
  struct machine_entry *nuova = NULL;
  char riga[CONFIG_TXT_LINE_LEN];
  int i, ha_voci, n = 0;

  if (primo == NULL || coda == NULL) {
    return;
  }
  for (i = 0; COMPOSITI_INCORPORATI[i] != NULL; i++) {
    strncpy(riga, COMPOSITI_INCORPORATI[i], sizeof(riga) - 1);
    riga[sizeof(riga) - 1] = '\0';
    if (riga[0] == '[') {
      struct machine_entry *s;
      nuova = NULL;
      if (new_section(&s, riga) != 0) {
        continue;
      }
      if (compositi_della_macchina(primo, coda, s->class, s->variante,
                                   &ha_voci) || !ha_voci) {
        free(s);
        continue;
      }
      ultima->next = s;
      ultima = s;
      nuova = s;
      n++;
    } else if (nuova != NULL) {
      append_to_section(nuova, riga);
    }
  }
  if (n > 0) {
    printf("[SWI] voci Composite aggiunte dal kernel: %d\n", n);
  }
}

// Load and parse the machine config from machines.txt
// Returns non-zero on error.
int load_machines(struct machine_entry** head) {
  FILE* fp = fopen("/machines.txt","r");
  if (fp == NULL) {
     *head = NULL;
     return 1;
  }

  char line[CONFIG_TXT_LINE_LEN];

  struct machine_entry* current_section = NULL;
  struct machine_entry* first_section = NULL;
  entry_id = 0;
  while (fgets(line, CONFIG_TXT_LINE_LEN - 1, fp)) {
    if (strlen(line) == 0)
      continue;
    if (line[0] == '#')
      continue;

    if (line[0] == '[') {
      struct machine_entry* section;
      if (new_section(&section, line) == 0) {
        if (current_section != NULL) {
           current_section->next = section;
        }
        current_section = section;
        if (!first_section) {
           first_section = current_section;
        }
      }
    } else {
      append_to_section(current_section, line);
    }
  }

  fclose(fp);
  aggiungi_compositi(first_section, current_section);
  *head = first_section;
  return 0;
}

void free_machines(struct machine_entry* head) {
  struct machine_entry* ptr = head;
  struct machine_entry* next_ptr;
  while (ptr) {
     struct machine_option* opt = ptr->options;
     struct machine_option* next_opt;
     while (opt) {
        next_opt = opt->next;
        free(opt);
        opt = next_opt;
     }
     next_ptr = ptr->next;
     free(ptr);
     ptr = next_ptr;
  }
}

static struct machine_option* find_option(const char *key,
                                          struct machine_option *head) {
  struct machine_option* opt = head;
  while (opt) {
    if(strcmp(opt->key, key) == 0) {
      return opt;
    }
    opt=opt->next;
  }
  return NULL;
}

// Single line version of apply_override where each param
// is one per line.  This is a non-destructive update in
// that existing parameters will not be altered other
// than those found in the options list for the machine
// entry.  However, the following options will be removed
// unless machine_timing=hdmi-custom or pal-custom is set:
// hdmi_cvt, hdmi_timings, dpi_timings
static void apply_override_s(char *line,
                             struct machine_entry *head, char* kernel_name,
                             struct s_cfg_flags* cfg_flags, int is_custom) {
  if (strlen(line) > 0 && line[0] != '#') {
    char *key = strtok(line, "=");
    char *value = strtok(NULL, "=");
    if (key == NULL || value == NULL) {
       return;
    }

    // If we find a kernal name, overwrite it now.
    if(strcmp(key, "kernel") == 0) {
       if (cfg_flags->need_kernel) {
          snprintf(line, CONFIG_TXT_LINE_LEN, "kernel=%s\n",kernel_name);
          cfg_flags->have_kernel = 1;
       } else {
          line[0] = '\0';
       }
       return;
    }






    if(strcmp(key, "gpu_mem") == 0) {
       cfg_flags->have_gpu_mem = 1;
       struct machine_option* gm = find_option("gpu_mem", head->options);
       int want = DEFAULT_GPU_MEM;
       if (gm) {
          int minimo = atoi(gm->value);
          int adesso = atoi(value);
          want = adesso > minimo ? adesso : minimo;
       }
       snprintf(line, CONFIG_TXT_LINE_LEN, "gpu_mem=%d\n", want);
       return;
    }

    // Make sure we remove custom hdmi stuff unless we are using a custom mode
    if(strcmp(key, "hdmi_timings") == 0) {
       cfg_flags->have_hdmi_timings = 1;
       if (!is_custom || head->video_out != BMC64_VIDEO_OUT_HDMI) {
          cfg_flags->have_hdmi_timings = 0;
          line[0] = '\0';
          return;
       }
    }
    if(strcmp(key, "hdmi_cvt") == 0) {
       cfg_flags->have_cvt = 1;
       if (!is_custom || head->video_out != BMC64_VIDEO_OUT_HDMI) {
          cfg_flags->have_cvt = 0;
          line[0] = '\0';
          return;
       }
    }
    if(strcmp(key, "dpi_timings") == 0) {
       cfg_flags->have_dpi_timings = 1;
       if (!is_custom || head->video_out != BMC64_VIDEO_OUT_DPI) {
          cfg_flags->have_dpi_timings = 0;
          line[0] = '\0';
          return;
       }
    }
    if(strcmp(key, "enable_dpi_lcd") == 0) {
       cfg_flags->have_enable_dpi_lcd = 1;
       if (head->video_out != BMC64_VIDEO_OUT_DPI) {
          cfg_flags->have_enable_dpi_lcd = 0;
          line[0] = '\0';
          return;
       }
    }
    if(strcmp(key, "display_default_lcd") == 0) {
       cfg_flags->have_display_default_lcd = 1;
       if (head->video_out != BMC64_VIDEO_OUT_DPI) {
          cfg_flags->have_display_default_lcd = 0;
          line[0] = '\0';
          return;
       }
    }
    if(strcmp(key, "dpi_group") == 0) {
       cfg_flags->have_dpi_group = 1;
       if (head->video_out != BMC64_VIDEO_OUT_DPI) {
          cfg_flags->have_dpi_group = 0;
          line[0] = '\0';
          return;
       }
    }
    if(strcmp(key, "dpi_mode") == 0) {
       cfg_flags->have_dpi_mode = 1;
       if (head->video_out != BMC64_VIDEO_OUT_DPI) {
          cfg_flags->have_dpi_mode = 0;
          line[0] = '\0';
          return;
       }
    }
    if(strcmp(key, "dpi_output_format") == 0) {
       cfg_flags->have_dpi_output_format = 1;
       if (head->video_out != BMC64_VIDEO_OUT_DPI) {
          cfg_flags->have_dpi_output_format = 0;
          line[0] = '\0';
          return;
       }
    }

    struct machine_option* found = find_option(key, head->options);
    char new_option[OPTION_SCRATCH_LEN];
    if (found) {
       snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s\n", key, found->value);
    } else {
       // Value already has newline since it came from original input.
       snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", key, value);
    }
    strcpy(line, new_option);
  }
}


// Multi param version of apply_override where there are
// multiple parameters on a single line of text. This is
// a non-destructive update in that existing parameters
// will not be altered other than those found in the
// options list for the machine entry.  The following
// options are removed first:
// cycles_per_second, cycles_per_refresh, machine_timing
// serial, demo, audio_out, disk_partition
static int apply_override_m(char *line, struct machine_entry *head,
                            struct s_cfg_flags* cfg_flags) {
  if (strlen(line) > 0 && line[0] != '#') {
    char replacement[CMDLINE_LINE_LEN];
    char new_option[OPTION_SCRATCH_LEN];
    int pieno = 0;

    replacement[0] = '\0';

    line = trim(line);
    int total = strlen(line);
    if (total == 0) return 0;

    char *option = line;
    int pos = 0;
    int need_space = 0;
    while (pos < total) {
      char* next_option = strtok(option, " ");
      if (next_option == NULL) {
        break;
      }

      // Advance past this token for next iteration before
      // we muck with the option via strtok
      pos+= strlen(next_option)+1;

      char *key = strtok(next_option, "=");
      char *value = strtok(NULL, "=");
      if (key == NULL) {
        option = line + pos;
        continue;
      }

      if (strcmp(key,"cycles_per_second") == 0) { cfg_flags->have_cycles_per_second = 1; }
      if (strcmp(key,"machine_timing") == 0) { cfg_flags->have_machine_timing = 1; }
      if (strcmp(key,"serial") == 0) { cfg_flags->have_serial = 1; }
      if (strcmp(key,"demo") == 0) { cfg_flags->have_demo = 1; }
      if (strcmp(key,"audio_out") == 0) { cfg_flags->have_audio_out = 1; }
      if (strcmp(key,"disk_partition") == 0) { cfg_flags->have_disk_partition = 1; }
      if (strcmp(key,"enable_dpi") == 0) { cfg_flags->have_enable_dpi = 1; }
      if (strcmp(key,"scaling_params") == 0) { cfg_flags->have_scaling_params = 1; }
      if (strcmp(key,"scaling_params2") == 0) { cfg_flags->have_scaling_params2 = 1; }
      if (strcmp(key,"raster_skip") == 0) { cfg_flags->have_raster_skip = 1; }
      if (strcmp(key,"raster_skip2") == 0) { cfg_flags->have_raster_skip2 = 1; }
      if (strcmp(key,"fb_size") == 0) { cfg_flags->have_fb_size = 1; }
      if (strcmp(key,"fb_display") == 0) { cfg_flags->have_fb_display = 1; }
      if (strcmp(key,"fb_pages") == 0) { cfg_flags->have_fb_pages = 1; }





      if (strcmp(key,"hdmi_group") == 0) { cfg_flags->have_hdmi_group = 1; }
      if (strcmp(key,"hdmi_mode") == 0) { cfg_flags->have_hdmi_mode = 1; }
      if (strcmp(key,"pi5kms_timings") == 0) {

        option = line + pos;
        continue;
      }








      if (strcmp(key, "audio_out") == 0 && value != NULL && value[0] != 0 &&
          find_option("audio_out", head->options) != NULL) {
         if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " ");}
         snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", key, value);
         pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
         need_space=1;
         option = line + pos;
         continue;
      }

      struct machine_option* found = find_option(key, head->options);
      if (found) {
         if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " ");}
         snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", key, found->value);
         pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
         need_space=1;
      } else {
         // Leave params we don't know about unchanged.
         if (strcmp(key,"cycles_per_refresh")!=0 &&
             strcmp(key,"cycles_per_second")!=0 &&
             strcmp(key,"machine_timing")!=0 &&
             strcmp(key,"serial")!=0 &&
             strcmp(key,"demo")!=0 &&
             strcmp(key,"audio_out")!=0 &&
             strcmp(key,"disk_partition")!=0 &&
             strcmp(key,"enable_dpi")!=0 &&
             strcmp(key,"raster_skip")!=0 &&
             strcmp(key,"raster_skip2")!=0 &&
             strcmp(key,"scaling_params")!=0 &&
             strcmp(key,"scaling_params2")!=0 &&
             strcmp(key,"fb_size")!=0 &&






             strcmp(key,"fb_pages")!=0 &&
             strcmp(key,"hdmi_group")!=0 &&
             strcmp(key,"hdmi_mode")!=0) {
            if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }





            if (value == NULL) {
              snprintf(new_option, OPTION_SCRATCH_LEN, "%s", key);
            } else if (strcmp(value, "(null)") == 0) {

              option = line + pos;
              continue;
            } else {
              snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", key, value);
            }
            pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
            need_space=1;
         }
      }

      option = line + pos;
    }

    // Now add params that should be present according to the entry.
    if (!cfg_flags->have_cycles_per_second) {
       struct machine_option* found = find_option("cycles_per_second", head->options);
       if (found) {
          if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }
          snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", found->key, found->value);
          pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
          need_space=1;
       }
    }
    if (!cfg_flags->have_machine_timing) {
       struct machine_option* found = find_option("machine_timing", head->options);
       if (found) {
          if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }
          snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", found->key, found->value);
          pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
          need_space=1;
       }
    }
    if (!cfg_flags->have_serial) {
       struct machine_option* found = find_option("serial", head->options);
       if (found) {
          if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }
          snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", found->key, found->value);
          pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
          need_space=1;
       }
    }
    if (!cfg_flags->have_demo) {
       struct machine_option* found = find_option("demo", head->options);
       if (found) {
          if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }
          snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", found->key, found->value);
          pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
          need_space=1;
       }
    }
    if (!cfg_flags->have_audio_out) {
       struct machine_option* found = find_option("audio_out", head->options);
       if (found) {
          if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }
          snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", found->key, found->value);
          pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
          need_space=1;
       }
    }
    if (!cfg_flags->have_disk_partition) {
       struct machine_option* found = find_option("disk_partition", head->options);
       if (found) {
          if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }
          snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", found->key, found->value);
          pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
          need_space=1;
       }
    }
    if (!cfg_flags->have_enable_dpi) {
       struct machine_option* found = find_option("enable_dpi", head->options);
       if (found) {
          if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }
          snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", found->key, found->value);
          pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
          need_space=1;
       }
    }
    if (!cfg_flags->have_scaling_params) {
       struct machine_option* found = find_option("scaling_params", head->options);
       if (found) {
          if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }
          snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", found->key, found->value);
          pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
          need_space=1;
       }
    }
    if (!cfg_flags->have_scaling_params2) {
       struct machine_option* found = find_option("scaling_params2", head->options);
       if (found) {
          if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }
          snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", found->key, found->value);
          pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
          need_space=1;
       }
    }
    if (!cfg_flags->have_raster_skip) {
       struct machine_option* found = find_option("raster_skip", head->options);
       if (found) {
          if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }
          snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", found->key, found->value);
          pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
          need_space=1;
       }
    }
    if (!cfg_flags->have_raster_skip2) {
       struct machine_option* found = find_option("raster_skip2", head->options);
       if (found) {
          if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }
          snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", found->key, found->value);
          pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
          need_space=1;
       }
    }
    if (!cfg_flags->have_fb_size) {
       struct machine_option* found = find_option("fb_size", head->options);
       if (found) {
          if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }
          snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", found->key, found->value);
          pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
          need_space=1;
       }
    }
    if (!cfg_flags->have_fb_display) {
       struct machine_option* found = find_option("fb_display", head->options);
       if (found) {
          if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }
          snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", found->key, found->value);
          pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
          need_space=1;
       }
    }
    if (!cfg_flags->have_hdmi_group) {
       struct machine_option* found = find_option("hdmi_group", head->options);
       if (found) {
          if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }
          snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", found->key, found->value);
          pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
          need_space=1;
       }
    }
    if (!cfg_flags->have_hdmi_mode) {
       struct machine_option* found = find_option("hdmi_mode", head->options);
       if (found) {
          if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }
          snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", found->key, found->value);
          pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
          need_space=1;
       }
    }



    {
       struct machine_option* found = find_option("hdmi_timings", head->options);
       if (found && found->value[0] != '\0') {
          char tempi[VALUE_LEN];
          size_t t;
          snprintf(tempi, sizeof(tempi), "%s", found->value);
          for (t = 0; tempi[t] != '\0'; t++) {
             if (isspace((unsigned char)tempi[t])) tempi[t] = ',';
          }
          if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }
          snprintf(new_option, OPTION_SCRATCH_LEN, "pi5kms_timings=%s", tempi);
          pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
          need_space=1;
       }
    }

    if (!cfg_flags->have_fb_pages) {
       struct machine_option* found = find_option("fb_pages", head->options);
       if (found) {
          if (need_space) { pieno |= !aggiungi(replacement, sizeof(replacement), " "); }
          snprintf(new_option, OPTION_SCRATCH_LEN, "%s=%s", found->key, found->value);
          pieno |= !aggiungi(replacement, sizeof(replacement), new_option);
          need_space=1;
       }
    }

    if (pieno || !aggiungi(replacement, sizeof(replacement), "\n")) {


      printf("[CAMBIO] la riga di cmdline.txt non ci sta in %d byte:"
             " lasciata com'era\n", (int)sizeof(replacement));
      return ERROR_5;
    }
    strcpy(line, replacement);
  }
  return 0;
}












static void chiudi_ultima_riga(FILE *in, FILE *out, const char *line) {
  size_t n = strlen(line);
  if (feof(in) && n > 0 && line[n - 1] != '\n') {
    fputc('\n', out);
    printf("[CAMBIO] l'ultima riga non aveva l'a capo: aggiunto\n");
  }
}







static const char *AUDIO_OUT_NOMI[4] = {"auto", "analog", "hdmi", "usb"};

int audio_out_apply(int scelta) {
  if (scelta < 0 || scelta > 3) {
    return ERROR_1;
  }
  FILE *fp = fopen("/cmdline.txt", "r");
  if (fp == NULL) {
    return ERROR_1;
  }
  FILE *fp2 = fopen("/cmdline.new", "w");
  if (fp2 == NULL) {
    fclose(fp);
    return ERROR_2;
  }

  char riga[CMDLINE_LINE_LEN];
  char nuova[CMDLINE_LINE_LEN];
  char coda[32];
  int prima = 1;
  int guaio = 0;

  snprintf(coda, sizeof(coda), "audio_out=%s", AUDIO_OUT_NOMI[scelta]);

  while (fgets(riga, CMDLINE_LINE_LEN, fp)) {





    if (!prima) {
      fputs(riga, fp2);
      chiudi_ultima_riga(fp, fp2, riga);
      continue;
    }
    prima = 0;

    size_t n = strlen(riga);
    if (n > 0 && riga[n - 1] != '\n' && !feof(fp)) {

      guaio = ERROR_5;
      fputs(riga, fp2);
      continue;
    }

    nuova[0] = 0;
    char *p = riga;
    while (*p) {
      while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
      if (!*p) break;
      char *fine = p;
      while (*fine && *fine != ' ' && *fine != '\t' && *fine != '\r' &&
             *fine != '\n') fine++;
      char salva = *fine;
      *fine = 0;





      size_t lp = strlen(p);
      int spazzatura = (lp >= 7 && strcmp(p + lp - 7, "=(null)") == 0);

      if (strncmp(p, "audio_out=", 10) != 0 && !spazzatura) {
        if (nuova[0] && !aggiungi(nuova, sizeof(nuova), " ")) guaio = ERROR_2;
        if (!aggiungi(nuova, sizeof(nuova), p)) guaio = ERROR_2;
      }
      *fine = salva;
      p = fine;
    }
    if (nuova[0] && !aggiungi(nuova, sizeof(nuova), " ")) guaio = ERROR_2;
    if (!aggiungi(nuova, sizeof(nuova), coda)) guaio = ERROR_2;
    if (guaio) {
      fclose(fp);
      fclose(fp2);
      unlink("/cmdline.new");
      return guaio;
    }
    fprintf(fp2, "%s\n", nuova);
  }

  fclose(fp);
  fclose(fp2);
  if (guaio) {
    unlink("/cmdline.new");
    return guaio;
  }
  if (prima) {

    unlink("/cmdline.new");
    return ERROR_6;
  }
  if (copy_file("/cmdline.new", "/cmdline.txt")) {
    return ERROR_3;
  }
  if (unlink("/cmdline.new")) {
    return ERROR_4;
  }
  printf("[AUDIO] cmdline.txt: %s\n", coda);
  return 0;
}






int cmdline_opzione(const char *chiave, const char *valore) {
  if (chiave == NULL || chiave[0] == 0) {
    return ERROR_1;
  }
  FILE *fp = fopen("/cmdline.txt", "r");
  if (fp == NULL) {
    return ERROR_1;
  }
  FILE *fp2 = fopen("/cmdline.new", "w");
  if (fp2 == NULL) {
    fclose(fp);
    return ERROR_2;
  }

  char riga[CMDLINE_LINE_LEN];
  char nuova[CMDLINE_LINE_LEN];
  char coda[96];
  char prefisso[48];
  size_t lpref;
  int prima = 1;
  int guaio = 0;

  snprintf(prefisso, sizeof(prefisso), "%s=", chiave);
  lpref = strlen(prefisso);
  coda[0] = 0;
  if (valore != NULL) {
    snprintf(coda, sizeof(coda), "%s=%s", chiave, valore);
  }

  while (fgets(riga, CMDLINE_LINE_LEN, fp)) {
    if (!prima) {
      fputs(riga, fp2);
      chiudi_ultima_riga(fp, fp2, riga);
      continue;
    }
    prima = 0;

    size_t n = strlen(riga);
    if (n > 0 && riga[n - 1] != '\n' && !feof(fp)) {
      guaio = ERROR_5;
      fputs(riga, fp2);
      continue;
    }

    nuova[0] = 0;
    char *p = riga;
    while (*p) {
      while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
      if (!*p) break;
      char *fine = p;
      while (*fine && *fine != ' ' && *fine != '\t' && *fine != '\r' &&
             *fine != '\n') fine++;
      char salva = *fine;
      *fine = 0;
      size_t lp = strlen(p);
      int spazzatura = (lp >= 7 && strcmp(p + lp - 7, "=(null)") == 0);
      if (strncmp(p, prefisso, lpref) != 0 && !spazzatura) {
        if (nuova[0] && !aggiungi(nuova, sizeof(nuova), " ")) guaio = ERROR_2;
        if (!aggiungi(nuova, sizeof(nuova), p)) guaio = ERROR_2;
      }
      *fine = salva;
      p = fine;
    }
    if (coda[0]) {
      if (nuova[0] && !aggiungi(nuova, sizeof(nuova), " ")) guaio = ERROR_2;
      if (!aggiungi(nuova, sizeof(nuova), coda)) guaio = ERROR_2;
    }
    if (guaio) {
      fclose(fp);
      fclose(fp2);
      unlink("/cmdline.new");
      return guaio;
    }
    fprintf(fp2, "%s\n", nuova);
  }

  fclose(fp);
  fclose(fp2);
  if (guaio) {
    unlink("/cmdline.new");
    return guaio;
  }
  if (prima) {
    unlink("/cmdline.new");
    return ERROR_6;
  }
  if (copy_file("/cmdline.new", "/cmdline.txt")) {
    return ERROR_3;
  }
  if (unlink("/cmdline.new")) {
    return ERROR_4;
  }
  printf("[CMDLINE] %s%s\n", valore != NULL ? coda : prefisso,
         valore != NULL ? "" : " tolta");
  return 0;
}

static int apply_cmdline(struct machine_entry* head, struct s_cfg_flags *cfg_flags) {
  FILE* fp = fopen("/cmdline.txt","r");
  if (fp == NULL) {
     return ERROR_1;
  }

  FILE* fp2 = fopen("/cmdline.new","w");
  if (fp2 == NULL) {
     fclose(fp);
     return ERROR_2;
  }

  char line[CMDLINE_LINE_LEN];
  int troppo_lunga = 0;

  while (fgets(line, CMDLINE_LINE_LEN, fp)) {




    size_t n = strlen(line);
    if (n > 0 && line[n - 1] != '\n' && !feof(fp)) {
      troppo_lunga = 1;
    }
    apply_override_m(line, head, cfg_flags);
    fprintf(fp2,"%s",line);
    chiudi_ultima_riga(fp, fp2, line);
  }
  if (troppo_lunga) {
    printf("[CAMBIO] cmdline.txt ha una riga piu' lunga di %d byte\n",
           CMDLINE_LINE_LEN);
  }

  fclose(fp);
  fclose(fp2);

  if (copy_file("/cmdline.new","/cmdline.txt")) {
     return ERROR_3;
  }
  if (unlink("/cmdline.new")) {
     return ERROR_4;
  }
  return 0;
}











#define KERNEL_BEGIN "# BMC64-KERNEL-BEGIN"
#define KERNEL_END   "# BMC64-KERNEL-END"

static int is_kernel_line(const char *line) {
   const char *p = ltrim((char *)line);
   if (strncmp(p, "kernel", 6) != 0) return 0;
   p += 6;
   while (*p == ' ' || *p == '\t') p++;
   return *p == '=';
}

// Reads config.txt and creates config.txt.new, overwriting
// any config.txt related items from the machine_entry. If
// kernel param is absent, it will be added.
static int apply_config(struct machine_entry* head, int pi_model, struct s_cfg_flags *cfg_flags) {
  char kernel_name[VALUE_LEN];
  const char *suffisso = "";
  switch (pi_model) {
    case 0:
    case 1:
       strcpy(kernel_name,"kernel.img");
       break;
    case 2:
       strcpy(kernel_name,"kernel7.img");
       break;
    case 3:
       strcpy(kernel_name,"kernel8-32.img");
       break;
    case 4:

       //






#if defined(__aarch64__)
       strcpy(kernel_name,"kernel8.img");
#else
       strcpy(kernel_name,"kernel7l.img");
#endif
       break;
    case 5:


       strcpy(kernel_name,"kernel_2712.img");
       break;
    default:
       return ERROR_5;
  }

  switch (head->class) {
     case BMC64_MACHINE_CLASS_C64:


        if (head->variante) {
           suffisso = ".c64-normale";
           strcat(kernel_name,suffisso);
        }
        break;
     case BMC64_MACHINE_CLASS_VIC20:
        suffisso = ".vic20";
        strcat(kernel_name,suffisso);
        break;
     case BMC64_MACHINE_CLASS_C128:
        suffisso = ".c128";
        strcat(kernel_name,suffisso);
        break;
     case BMC64_MACHINE_CLASS_PLUS4:
        suffisso = ".plus4";
        strcat(kernel_name,suffisso);
        break;
     case BMC64_MACHINE_CLASS_PLUS4EMU:
        suffisso = ".plus4emu";
        strcat(kernel_name,suffisso);
        break;
     case BMC64_MACHINE_CLASS_PET:
        suffisso = ".pet";
        strcat(kernel_name,suffisso);
        break;
     case BMC64_MACHINE_CLASS_SCPU64:

        suffisso = ".scpu64";
        strcat(kernel_name,suffisso);
        break;
     default:
        return ERROR_6;
   }

  FILE* fp = fopen("/config.txt","r");
  if (fp == NULL) {
     return ERROR_7;
  }

  FILE* fp2 = fopen("/config.new","w");
  if (fp2 == NULL) {
     fclose(fp);
     return ERROR_8;
  }

  int is_custom = 0;
  struct machine_option* found = find_option("machine_timing", head->options);
  if (found && (strcmp(found->value, "ntsc-custom") == 0 || strcmp(found->value,"pal-custom") == 0)) {
    is_custom = 1;
  }






  {
    struct machine_option* gruppo = find_option("hdmi_group", head->options);
    struct machine_option* modo = find_option("hdmi_mode", head->options);
    if (gruppo && modo && atoi(gruppo->value) == 2 && atoi(modo->value) == 87 &&
        find_option("hdmi_timings", head->options) != NULL) {
      is_custom = 1;
    }
  }

  char line[CONFIG_TXT_LINE_LEN];
  cfg_flags->need_kernel=head->class != BMC64_MACHINE_CLASS_C64 ||
                        head->variante;
#if defined(__aarch64__)
  const int per_famiglia = (pi_model == 4 || pi_model == 5);
#else
  const int per_famiglia = 0;
#endif
  int nel_blocco = 0;
  while (fgets(line, CONFIG_TXT_LINE_LEN - 1, fp)) {
    if (per_famiglia) {
      if (strncmp(ltrim(line), KERNEL_BEGIN, strlen(KERNEL_BEGIN)) == 0) {
        nel_blocco = 1;
        continue;
      }
      if (nel_blocco) {
        if (strncmp(ltrim(line), KERNEL_END, strlen(KERNEL_END)) == 0) {
          nel_blocco = 0;
        }
        continue;
      }
      if (is_kernel_line(line)) {
        continue;
      }
    }
    apply_override_s(line, head, kernel_name, cfg_flags, is_custom);
    fprintf(fp2,"%s",line);
    chiudi_ultima_riga(fp, fp2, line);
  }

  if (per_famiglia) {
    if (cfg_flags->need_kernel) {
      fprintf(fp2, "%s %s\n", KERNEL_BEGIN, suffisso + 1);
      fprintf(fp2, "[pi4]\nkernel=kernel8.img%s\n", suffisso);
      fprintf(fp2, "[pi5]\nkernel=kernel_2712.img%s\n", suffisso);


      fprintf(fp2, "[all]\n%s\n", KERNEL_END);
    }
  } else if (!cfg_flags->have_kernel && cfg_flags->need_kernel) {
    // This may have been overwritten by the pass above. If not present,

    fprintf(fp2,"kernel=%s\n", kernel_name);
  }




  if (!cfg_flags->have_gpu_mem) {
    found = find_option("gpu_mem", head->options);
    if (found) {
       int minimo = atoi(found->value);
       fprintf(fp2,"gpu_mem=%d\n",
               minimo > DEFAULT_GPU_MEM ? minimo : DEFAULT_GPU_MEM);
    }
  }

  // Ensure we add custom timings if present.
  if (is_custom) {
    if (!cfg_flags->have_hdmi_timings) {
       found = find_option("hdmi_timings", head->options);
       if (found) {
          fprintf(fp2,"hdmi_timings=%s\n", found->value);
       }
    }
    if (!cfg_flags->have_dpi_timings) {
       found = find_option("dpi_timings", head->options);
       if (found) {
          fprintf(fp2,"dpi_timings=%s\n", found->value);
       }
    }
    if (!cfg_flags->have_cvt) {
       found = find_option("hdmi_cvt", head->options);
       if (found) {
          fprintf(fp2,"hdmi_cvt=%s\n", found->value);
       }
    }
  }

  // Ensure we add dpi if necessary
  if (head->video_out == BMC64_VIDEO_OUT_DPI) {
     found = find_option("enable_dpi_lcd", head->options);
     if (found) {
        fprintf(fp2,"enable_dpi_lcd=%s\n", found->value);
     }

     found = find_option("display_default_lcd", head->options);
     if (found) {
        fprintf(fp2,"display_default_lcd=%s\n", found->value);
     }

     found = find_option("dpi_group", head->options);
     if (found) {
        fprintf(fp2,"dpi_group=%s\n", found->value);
     }

     found = find_option("dpi_mode", head->options);
     if (found) {
        fprintf(fp2,"dpi_mode=%s\n", found->value);
     }

     found = find_option("dpi_output_format", head->options);
     if (found) {
        fprintf(fp2,"dpi_output_format=%s\n", found->value);
     }
  }

  fclose(fp);
  fclose(fp2);

  if (copy_file("/config.new","/config.txt")) {
    return ERROR_9;
  }
  if (unlink("/config.new")) {
    return ERROR_10;
  }
  return 0;
}



#define OC_BEGIN "# BMC64-OVERCLOCK-BEGIN"
#define OC_END   "# BMC64-OVERCLOCK-END"

static const char *oc_keys[] = {
   "arm_freq", "arm_freq_min", "over_voltage", "over_voltage_delta",
   "force_turbo", "arm_boost", NULL
};

static int is_oc_line(const char *line) {
   const char *p = ltrim((char *)line);
   for (int i = 0; oc_keys[i]; i++) {
      size_t n = strlen(oc_keys[i]);
      if (strncmp(p, oc_keys[i], n) == 0) {
         const char *q = p + n;
         while (*q == ' ' || *q == '\t') q++;
         if (*q == '=') return 1;
      }
   }
   return 0;
}





















struct oc_blocco {
   int c_e;
   int pct;
   int stock;
   int tipo;
   int famiglia;
};


static void oc_leggi_testa(const char *p, struct oc_blocco *b) {
   memset(b, 0, sizeof(*b));
   p += strlen(OC_BEGIN);
   while (*p == ' ' || *p == '\t') p++;
   if (*p == '+') p++;
   b->pct = atoi(p);
   const char *q = strstr(p, "stock=");
   b->stock = q ? atoi(q + 6) : 0;
   q = strstr(p, "board-type=");
   b->tipo = q ? (int)strtol(q + 11, NULL, 0) : -1;
   b->c_e = 1;
}

static int oc_famiglia(int pi_model) {
   return pi_model >= 5 ? 5 : 4;
}



static void oc_trova(int tipo_qui, int famiglia_qui, struct oc_blocco *trovato) {
   struct oc_blocco b, vecchio;
   char line[CONFIG_TXT_LINE_LEN];
   int dentro = 0;

   memset(trovato, 0, sizeof(*trovato));
   memset(&vecchio, 0, sizeof(vecchio));
   memset(&b, 0, sizeof(b));
   FILE* fp = fopen("/config.txt","r");
   if (fp == NULL) {
      return;
   }
   while (fgets(line, CONFIG_TXT_LINE_LEN - 1, fp)) {
      char *p = ltrim(line);
      if (strncmp(p, OC_BEGIN, strlen(OC_BEGIN)) == 0) {
         oc_leggi_testa(p, &b);
         dentro = 1;
         if (b.tipo >= 0 && b.tipo == tipo_qui) {
            *trovato = b;
            break;
         }
         continue;
      }
      if (!dentro) {
         continue;
      }
      if (strncmp(p, OC_END, strlen(OC_END)) == 0) {
         dentro = 0;
         continue;
      }
      if (b.tipo < 0 && b.famiglia == 0) {
         if (strncmp(p, "[pi4]", 5) == 0) b.famiglia = 4;
         else if (strncmp(p, "[pi5]", 5) == 0) b.famiglia = 5;
         if (b.famiglia == famiglia_qui && !vecchio.c_e) {
            vecchio = b;
         }
      }
   }
   fclose(fp);
   if (!trovato->c_e && vecchio.c_e) {
      *trovato = vecchio;
   }
}





int overclock_current(void) {
   struct oc_blocco b;
   oc_trova(circle_board_type(), oc_famiglia(circle_get_model()), &b);
   if (!b.c_e) return 0;
   if (b.pct >= 20) return 2;
   if (b.pct >= 10) return 1;
   if (b.pct <= -20) return -2;
   if (b.pct <= -10) return -1;
   return 0;
}


int overclock_stock_mhz(void) {
   int pi_model = circle_get_model();
   int tipo = circle_board_type();
   struct oc_blocco b;
   int adesso = (int)circle_arm_max_mhz();
   int stock;

   oc_trova(tipo, oc_famiglia(pi_model), &b);
   if (b.c_e && b.stock > 0) {
      stock = b.stock;
   } else if (b.c_e && b.pct != 0 && adesso > 0) {

      stock = adesso * 100 / (100 + b.pct);
   } else {
      stock = adesso;
   }
   if (stock < 600 || stock > 4000) {

      if (pi_model >= 5) stock = 2400;
      else if (tipo == 0x13) stock = 1800;
      else stock = 1500;
   }
   return stock;
}

#define OC_RIGHE_BLOCCO 16


static void oc_scrivi(FILE *fp2, const char *riga) {
   size_t n = strlen(riga);
   fprintf(fp2, "%s", riga);
   if (n == 0 || riga[n - 1] != '\n') {
      fprintf(fp2, "\n");
   }
}




int overclock_apply(int level, int pi_model) {
   const char *volt_key;
   int volt[3];
   char section[24];
   const int tipo = circle_board_type();
   const int famiglia = oc_famiglia(pi_model);

   switch (pi_model) {
      case 4:
         volt_key = "over_voltage";
         volt[0] = 0; volt[1] = 2; volt[2] = 6;
         break;
      case 5:


         volt_key = "over_voltage_delta";
         volt[0] = 0; volt[1] = 25000; volt[2] = 50000;
         break;
      default:
         return ERROR_11;
   }
   if (tipo >= 0) {
      snprintf(section, sizeof(section), "[board-type=0x%02x]", tipo);
   } else {
      strcpy(section, famiglia == 5 ? "[pi5]" : "[pi4]");
   }

   if (level < -2 || level > 2) {
      level = 0;
   }



   const unsigned stock_mhz = (unsigned)overclock_stock_mhz();

   FILE* fp = fopen("/config.txt","r");
   if (fp == NULL) {
      return ERROR_7;
   }
   FILE* fp2 = fopen("/config.new","w");
   if (fp2 == NULL) {
      fclose(fp);
      return ERROR_8;
   }





   static char blocco[OC_RIGHE_BLOCCO][CONFIG_TXT_LINE_LEN];
   int righe = 0;
   int in_block = 0;
   struct oc_blocco b;
   char line[CONFIG_TXT_LINE_LEN];

   memset(&b, 0, sizeof(b));
   while (fgets(line, CONFIG_TXT_LINE_LEN - 1, fp)) {
      char *p = ltrim(line);
      if (!in_block && strncmp(p, OC_BEGIN, strlen(OC_BEGIN)) == 0) {
         oc_leggi_testa(p, &b);
         in_block = 1;
         righe = 0;
         strcpy(blocco[righe++], line);
         continue;
      }
      if (in_block) {
         if (b.tipo < 0 && b.famiglia == 0) {
            if (strncmp(p, "[pi4]", 5) == 0) b.famiglia = 4;
            else if (strncmp(p, "[pi5]", 5) == 0) b.famiglia = 5;
         }
         if (righe < OC_RIGHE_BLOCCO) {
            strcpy(blocco[righe++], line);
         }
         if (strncmp(p, OC_END, strlen(OC_END)) == 0) {
            in_block = 0;
            const int di_questo_pi = b.tipo >= 0
                                     ? b.tipo == tipo
                                     : (b.famiglia == 0 || b.famiglia == famiglia);
            if (!di_questo_pi) {
               for (int i = 0; i < righe; i++) {
                  oc_scrivi(fp2, blocco[i]);
               }
            }
         }
         continue;
      }
      // Also drop a stray arm_freq written by hand, so there is exactly one

      if (is_oc_line(line)) continue;
      fprintf(fp2, "%s", line);
      chiudi_ultima_riga(fp, fp2, line);
   }
   fclose(fp);

   if (in_block) {
      for (int i = 0; i < righe; i++) {
         oc_scrivi(fp2, blocco[i]);
      }
   }

   unsigned mhz = stock_mhz;
   if (level != 0) {
      int pct = level * 10;
      mhz = (unsigned)((int)stock_mhz + ((int)stock_mhz * pct) / 100);
      if (tipo >= 0) {
         fprintf(fp2, "%s %+d%% stock=%u board-type=0x%02x\n", OC_BEGIN, pct,
                 stock_mhz, tipo);
      } else {
         fprintf(fp2, "%s %+d%% stock=%u\n", OC_BEGIN, pct, stock_mhz);
      }
      fprintf(fp2, "%s\n", section);
      fprintf(fp2, "arm_freq=%u\n", mhz);
      if (level > 0) {
         fprintf(fp2, "%s=%d\n", volt_key, volt[level]);
      } else {


         fprintf(fp2, "arm_freq_min=%u\n", mhz);
      }
      fprintf(fp2, "force_turbo=1\n");


      fprintf(fp2, "[all]\n");
      fprintf(fp2, "%s\n", OC_END);
   }
   printf("[OC] scheda 0x%02x, di serie %u MHz, livello %d -> %u MHz\n",
          tipo & 0xFF, stock_mhz, level, mhz);

   fclose(fp2);

   if (copy_file("/config.new","/config.txt")) {
      return ERROR_9;
   }
   if (unlink("/config.new")) {
      return ERROR_10;
   }
   return 0;
}
























#define GPU_BEGIN "# BMC64-GPUCLOCK-BEGIN"
#define GPU_END   "# BMC64-GPUCLOCK-END"

static int is_gpu_line(const char *line) {
   static const char *chiavi[] = { "core_freq", "v3d_freq", "gpu_freq", NULL };
   const char *p = ltrim((char *)line);
   for (int i = 0; chiavi[i]; i++) {
      size_t n = strlen(chiavi[i]);
      if (strncmp(p, chiavi[i], n) == 0) {
         const char *q = p + n;
         while (*q == ' ' || *q == '\t') q++;
         if (*q == '=') return 1;
      }
   }
   return 0;
}


void gpuclock_di_serie(int pi_model, unsigned *core, unsigned *v3d) {
   if (oc_famiglia(pi_model) == 5) {
      *core = 910;
      *v3d = 960;
   } else {
      *core = 500;
      *v3d = 500;
   }
}



static int gpu_blocco_mio(const char *p, int tipo, int famiglia, int *pct) {
   p += strlen(GPU_BEGIN);
   while (*p == ' ' || *p == '\t') p++;
   if (*p == '+') p++;
   *pct = atoi(p);
   const char *q = strstr(p, "board-type=");
   if (q) {
      return tipo >= 0 && (int)strtol(q + 11, NULL, 0) == tipo;
   }
   q = strstr(p, "famiglia=");
   if (q) {
      return tipo < 0 && atoi(q + 9) == famiglia;
   }
   return 0;
}



int gpuclock_current(void) {
   char line[CONFIG_TXT_LINE_LEN];
   const int tipo = circle_board_type();
   const int famiglia = oc_famiglia(circle_get_model());
   int pct = 0, livello = 0;
   FILE* fp = fopen("/config.txt","r");
   if (fp == NULL) {
      return 0;
   }
   while (fgets(line, CONFIG_TXT_LINE_LEN - 1, fp)) {
      char *p = ltrim(line);
      if (strncmp(p, GPU_BEGIN, strlen(GPU_BEGIN)) == 0 &&
          gpu_blocco_mio(p, tipo, famiglia, &pct)) {
         if (pct >= 20) livello = 2;
         else if (pct >= 10) livello = 1;
         else if (pct <= -20) livello = -2;
         else if (pct <= -10) livello = -1;
         break;
      }
   }
   fclose(fp);
   return livello;
}



int gpuclock_apply(int level, int pi_model) {
   char section[24];
   char chi[32];
   const int tipo = circle_board_type();
   const int famiglia = oc_famiglia(pi_model);
   unsigned core, v3d;

   if (pi_model != 4 && pi_model != 5) {
      return ERROR_11;
   }


   if (level < 0 || level > 2) {
      level = 0;
   }
   gpuclock_di_serie(pi_model, &core, &v3d);
   if (tipo >= 0) {
      snprintf(section, sizeof(section), "[board-type=0x%02x]", tipo);
      snprintf(chi, sizeof(chi), "board-type=0x%02x", tipo);
   } else {
      strcpy(section, famiglia == 5 ? "[pi5]" : "[pi4]");
      snprintf(chi, sizeof(chi), "famiglia=%d", famiglia);
   }

   FILE* fp = fopen("/config.txt","r");
   if (fp == NULL) {
      return ERROR_7;
   }
   FILE* fp2 = fopen("/config.new","w");
   if (fp2 == NULL) {
      fclose(fp);
      return ERROR_8;
   }



   static char blocco[OC_RIGHE_BLOCCO][CONFIG_TXT_LINE_LEN];
   int righe = 0, in_block = 0, mio = 0, pct = 0;
   char line[CONFIG_TXT_LINE_LEN];
   while (fgets(line, CONFIG_TXT_LINE_LEN - 1, fp)) {
      char *p = ltrim(line);
      if (!in_block && strncmp(p, GPU_BEGIN, strlen(GPU_BEGIN)) == 0) {
         in_block = 1;
         righe = 0;
         mio = gpu_blocco_mio(p, tipo, famiglia, &pct);
         strcpy(blocco[righe++], line);
         continue;
      }
      if (in_block) {
         if (righe < OC_RIGHE_BLOCCO) {
            strcpy(blocco[righe++], line);
         }
         if (strncmp(p, GPU_END, strlen(GPU_END)) == 0) {
            in_block = 0;
            if (!mio) {
               for (int i = 0; i < righe; i++) {
                  oc_scrivi(fp2, blocco[i]);
               }
            }
         }
         continue;
      }
      if (is_gpu_line(line)) continue;
      fprintf(fp2, "%s", line);
      chiudi_ultima_riga(fp, fp2, line);
   }
   fclose(fp);

   if (in_block) {
      for (int i = 0; i < righe; i++) {
         oc_scrivi(fp2, blocco[i]);
      }
   }

   unsigned core_mhz = core, v3d_mhz = v3d;
   if (level != 0) {
      const int pc = level * 10;
      core_mhz = (unsigned)((int)core + ((int)core * pc) / 100);
      v3d_mhz = (unsigned)((int)v3d + ((int)v3d * pc) / 100);
      fprintf(fp2, "%s %+d%% core=%u v3d=%u %s\n", GPU_BEGIN, pc, core, v3d, chi);
      fprintf(fp2, "%s\n", section);
      fprintf(fp2, "core_freq=%u\n", core_mhz);
      fprintf(fp2, "v3d_freq=%u\n", v3d_mhz);

      fprintf(fp2, "[all]\n");
      fprintf(fp2, "%s\n", GPU_END);
   }
   printf("[GPU] scheda 0x%02x, di serie core %u v3d %u MHz, livello %d -> core %u v3d %u MHz\n",
          tipo & 0xFF, core, v3d, level, core_mhz, v3d_mhz);

   fclose(fp2);

   if (copy_file("/config.new","/config.txt")) {
      return ERROR_9;
   }
   if (unlink("/config.new")) {
      return ERROR_10;
   }
   return 0;
}









#define TV_BEGIN "# BMC64-TVOUT-BEGIN"
#define TV_END   "# BMC64-TVOUT-END"

int tvout_applica(int acceso) {
   FILE* fp = fopen("/config.txt","r");
   if (fp == NULL) {
      return ERROR_7;
   }
   FILE* fp2 = fopen("/config.new","w");
   if (fp2 == NULL) {
      fclose(fp);
      return ERROR_8;
   }
   char line[CONFIG_TXT_LINE_LEN];
   int dentro = 0;
   while (fgets(line, CONFIG_TXT_LINE_LEN - 1, fp)) {
      char *p = ltrim(line);
      if (!dentro && strncmp(p, TV_BEGIN, strlen(TV_BEGIN)) == 0) {
         dentro = 1;
         continue;
      }
      if (dentro) {
         if (strncmp(p, TV_END, strlen(TV_END)) == 0) {
            dentro = 0;
         }
         continue;
      }
      fprintf(fp2, "%s", line);
      chiudi_ultima_riga(fp, fp2, line);
   }
   fclose(fp);
   if (acceso) {


      fprintf(fp2, "%s\n[board-type=0x11]\nenable_tvout=1\n[all]\n%s\n",
              TV_BEGIN, TV_END);
   }
   fclose(fp2);
   printf("[VID] config.txt: composito %s\n", acceso ? "acceso" : "spento");
   if (copy_file("/config.new","/config.txt")) {
      return ERROR_9;
   }
   if (unlink("/config.new")) {
      return ERROR_10;
   }
   return 0;
}

int switch_apply_files(struct machine_entry* head) {
  struct s_cfg_flags cfg_flags;
  memset(&cfg_flags, 0, sizeof(struct s_cfg_flags));



  if (vidtrial_enabled && vidtrial_timer_on()) {
    vidtrial_arm();
  }













  const int composito = head->video_out == BMC64_VIDEO_OUT_COMPOSITE &&
                        circle_board_type() == 0x11;
  int status = 0;
  if (composito) {
    status |= cmdline_opzione("composito", "1");
  } else {
    status |= tvout_applica(0);
  }
  status |= apply_config(head, circle_get_model(), &cfg_flags);
  status |= apply_cmdline(head, &cfg_flags);
  if (composito) {
    status |= tvout_applica(1);
  } else {
    status |= cmdline_opzione("composito", NULL);
  }
  printf("[VID] Switch: %s\n", composito ? "composito del Pi 4 B" : "HDMI");
  return status;
}

void switch_safe() {
  struct machine_entry* entry =
     (struct machine_entry*) malloc(sizeof(struct machine_entry));

  entry->id = 1;
  entry->class = BMC64_MACHINE_CLASS_C64;
  entry->variante = 0;
  entry->custom = 0;
  entry->video_standard = BMC64_VIDEO_STANDARD_PAL;
  entry->video_out = BMC64_VIDEO_OUT_HDMI;
  strcpy(entry->desc,"Safe");
  entry->options = NULL;
  entry->next = NULL;

  char tmp[80];
  strcpy (tmp,"disable_overscan=1");
  append_to_section(entry, tmp);

  strcpy (tmp,"sdtv_mode=18");

  append_to_section(entry, tmp);
  strcpy (tmp,"hdmi_group=1");

  append_to_section(entry, tmp);
  strcpy (tmp,"hdmi_mode=19");

  append_to_section(entry, tmp);




  strcpy (tmp,"machine_timing=pal-hdmi");
  append_to_section(entry, tmp);
  strcpy (tmp,"scaling_params=384,240,1152,720");
  append_to_section(entry, tmp);


  vidtrial_enabled = 0;



  tvout_applica(0);
  cmdline_opzione("composito", NULL);
  switch_apply_files(entry);
  vidtrial_enabled = 1;
}

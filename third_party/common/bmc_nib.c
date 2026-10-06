/*
 * bmc_nib.c - .nib disk images converted to G64 for BMC64-NG.
 *
 * The conversion is the one of nibconv, from nibtools by Pete Rittwage
 * (GPL v3, in nibtools/): its globals and the NIB -> G64 path of its
 * main(), with its default options, made into a function.
 */





#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mnibarch.h"
#include "gcr.h"
#include "nibtools.h"
#include "prot.h"
#include "bmc_nib.h"

#ifndef BMC_NIB_PC


extern void bmc_nib_cartella(void);
#endif


BYTE track_density[MAX_HALFTRACKS_1541 + 2];
BYTE track_alignment[MAX_HALFTRACKS_1541 + 2];
size_t track_length[MAX_HALFTRACKS_1541 + 2];
int file_buffer_size;
int start_track, end_track, track_inc;
int reduce_sync, reduce_badgcr, reduce_gap;
int fix_gcr, align, force_align;
int gap_match_length;
int cap_min_ignore;
int skip_halftracks;
int verbose;
int rpm_real;
int auto_capacity_adjust;
int skew;
int align_disk;
int ihs;
int mode;
int unformat_passes;
int capacity_margin;
int align_delay;
int increase_sync = 0;
int presync = 0;
BYTE fillbyte = 0x55;
BYTE drive = 8;
char * cbm_adapter = "";
int use_floppycode_srq = 0;
int override_srq = 0;
int extra_capacity_margin=5;
int sync_align_buffer=0;
int fattrack=0;
int track_match=0;
int old_g64=0;
int read_killer=1;
int backwards=0;
int nb2cycle=0;


void usage(void) {
}

#define DIM_BUFFER ((size_t)(MAX_HALFTRACKS_1541 + 2) * NIB_TRACK_LENGTH)


static BYTE sector_map_0[MAX_TRACKS_1541 + 1], sector_gap_length_0[MAX_TRACKS_1541 + 1];
static BYTE speed_map_0[MAX_TRACKS_1541 + 1], align_map_0[MAX_TRACKS_1541 + 1];
static BYTE reduce_map_0[MAX_TRACKS_1541 + 1];
static size_t capacity_0[4], capacity_min_0[4], capacity_max_0[4];
static int tabelle_fotografate = 0;

static void come_un_nibconv_appena_lanciato(void) {
  if (!tabelle_fotografate) {
    memcpy(sector_map_0, sector_map, sizeof sector_map_0);
    memcpy(sector_gap_length_0, sector_gap_length, sizeof sector_gap_length_0);
    memcpy(speed_map_0, speed_map, sizeof speed_map_0);
    memcpy(align_map_0, align_map, sizeof align_map_0);
    memcpy(reduce_map_0, reduce_map, sizeof reduce_map_0);
    memcpy(capacity_0, capacity, sizeof capacity_0);
    memcpy(capacity_min_0, capacity_min, sizeof capacity_min_0);
    memcpy(capacity_max_0, capacity_max, sizeof capacity_max_0);
    tabelle_fotografate = 1;
  } else {
    memcpy(sector_map, sector_map_0, sizeof sector_map_0);
    memcpy(sector_gap_length, sector_gap_length_0, sizeof sector_gap_length_0);
    memcpy(speed_map, speed_map_0, sizeof speed_map_0);
    memcpy(align_map, align_map_0, sizeof align_map_0);
    memcpy(reduce_map, reduce_map_0, sizeof reduce_map_0);
    memcpy(capacity, capacity_0, sizeof capacity_0);
    memcpy(capacity_min, capacity_min_0, sizeof capacity_min_0);
    memcpy(capacity_max, capacity_max_0, sizeof capacity_max_0);
  }
  memset(track_density, 0, sizeof track_density);
  memset(track_alignment, 0, sizeof track_alignment);
  memset(track_length, 0, sizeof track_length);
  file_buffer_size = 0;
  start_track = end_track = track_inc = 0;
  reduce_sync = reduce_badgcr = reduce_gap = 0;
  fix_gcr = align = force_align = 0;
  gap_match_length = cap_min_ignore = skip_halftracks = verbose = rpm_real = 0;
  auto_capacity_adjust = skew = align_disk = ihs = mode = 0;
  unformat_passes = capacity_margin = align_delay = 0;
  increase_sync = 0;
  presync = 0;
  fillbyte = 0x55;
  drive = 8;
  cbm_adapter = "";
  use_floppycode_srq = 0;
  override_srq = 0;
  extra_capacity_margin = 5;
  sync_align_buffer = 0;
  fattrack = 0;
  track_match = 0;
  old_g64 = 0;
  read_killer = 1;
  backwards = 0;
  nb2cycle = 0;
}

static long misura_del_file(const char *path) {
  FILE *f = fopen(path, "rb");
  long n;
  if (f == NULL) {
    return -1;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return -1;
  }
  n = ftell(f);
  fclose(f);
  return n;
}


static int intestazione_buona(const BYTE *b, long misura) {
  int h, t = 0;
  if (memcmp(b, "MNIB-1541-RAW", 13) != 0) {
    return 0;
  }
  for (h = 0; 0x10 + h + 1 < 0x100; h += 2) {
    int traccia = b[0x10 + h];
    if (traccia == 0) {
      return t > 0;
    }
    if (traccia > MAX_HALFTRACKS_1541 + 1) {
      return 0;
    }
    if ((long)(t + 1) * NIB_TRACK_LENGTH + 0x100 > misura) {
      return 0;
    }
    t++;
  }
  return 0;
}

int bmc_nib_in_g64(const char *nib_path, const char *g64_path) {
  BYTE *file_buffer = NULL, *track_buffer = NULL;
  char in[256], out[256];
  long misura;
  int t, fatto = 0;

  if (strlen(nib_path) >= sizeof in || strlen(g64_path) >= sizeof out) {
    return 0;
  }
  strcpy(in, nib_path);
  strcpy(out, g64_path);
  misura = misura_del_file(in);
  if (misura < 0x100 || misura > (long)DIM_BUFFER) {
    printf("bmc_nib: %s: misura %ld, non e' un .NIB\n", in, misura);
    return 0;
  }

  come_un_nibconv_appena_lanciato();

  start_track = 1 * 2;
  end_track = 42 * 2;
  track_inc = 1;
  fix_gcr = 1;
  reduce_sync = 4;
  skip_halftracks = 0;
  align = ALIGN_NONE;
  force_align = ALIGN_NONE;
  gap_match_length = 7;
  cap_min_ignore = 0;
  verbose = 0;
  rpm_real = 295;
  memset(reduce_map, REDUCE_SYNC, MAX_TRACKS_1541+1);
  for (t = 0; t < MAX_TRACKS_1541+1; t++) {
    track_length[t] = NIB_TRACK_LENGTH;
  }
  file_buffer = malloc(DIM_BUFFER);
  track_buffer = malloc(DIM_BUFFER);
  if (file_buffer == NULL || track_buffer == NULL) {
    goto fine;
  }
  memset(file_buffer, 0x00, DIM_BUFFER);
  memset(track_buffer, 0x00, DIM_BUFFER);


  if (!(file_buffer_size = load_file(in, file_buffer))) {
    goto fine;
  }
  if (!intestazione_buona(file_buffer, file_buffer_size)) {
    printf("bmc_nib: %s: intestazione del .NIB non valida\n", in);
    goto fine;
  }
  if (!(read_nib(file_buffer, file_buffer_size, track_buffer, track_density, track_length))) {
    goto fine;
  }
  align_tracks(track_buffer, track_density, track_length, track_alignment);
  search_fat_tracks(track_buffer, track_density, track_length);
  if (skip_halftracks) {
    track_inc = 2;
  }
  if (!(write_g64(out, track_buffer, track_density, track_length))) {
    goto fine;
  }
  fatto = 1;

fine:
  free(file_buffer);
  free(track_buffer);
  return fatto;
}

int bmc_nib_e_nib(const char *path) {
  size_t n = strlen(path);
  return n > 4 && path[n - 4] == '.' && tolower((unsigned char)path[n - 3]) == 'n' &&
         tolower((unsigned char)path[n - 2]) == 'i' && tolower((unsigned char)path[n - 1]) == 'b';
}

int bmc_nib_prepara(const char *nib_path, char *out, unsigned int out_len) {
  const char *nome = strrchr(nib_path, '/');
  size_t n;
  nome = nome ? nome + 1 : nib_path;
  if (strchr(nome, ':') != NULL) {
    nome = strchr(nome, ':') + 1;
  }
  n = strlen(nome);
  if (n <= 4 || strlen(BMC_NIB_DIR) + 1 + (n - 4) + 4 + 1 > out_len) {
    return -1;
  }
#ifndef BMC_NIB_PC
  bmc_nib_cartella();
#endif
  snprintf(out, out_len, "%s/%.*s.g64", BMC_NIB_DIR, (int)(n - 4), nome);
  printf("bmc_nib: %s -> %s\n", nib_path, out);
  return bmc_nib_in_g64(nib_path, out) ? 0 : -1;
}

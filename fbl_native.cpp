


// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     https://www.apache.org/licenses/LICENSE-2.0


// distributed under the License is distributed on an "AS IS" BASIS,

// See the License for the specific language governing permissions and
// limitations under the License.

































































#include "fbl.h"

#ifdef BMC64_NATIVE_FB

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <circle/bcmframebuffer.h>
#include <circle/chargenerator.h>
#include <circle/bcmpropertytags.h>
#if RASPPI >= 4
#include <circle/dmachannel.h>
#endif
#include <circle/machineinfo.h>
#include <circle/synchronize.h>

#include "viceoptions.h"

#ifndef ALIGN_UP
#define ALIGN_UP(x, y) ((x + (y)-1) & ~((y)-1))
#endif

#define RGB565(r, g, b) (((r) >> 3) << 11 | ((g) >> 2) << 5 | (b) >> 3)
#define ARGB(a, r, g, b)                                                     \
  ((uint32_t)((uint8_t)(a) << 24 | (uint8_t)(r) << 16 | (uint8_t)(g) << 8 |  \
              (uint8_t)(b)))


static uint16_t pal_565[256] = {
    RGB565(0x00, 0x00, 0x00), RGB565(0xFF, 0xFF, 0xFF),
    RGB565(0xFF, 0x00, 0x00), RGB565(0x70, 0xa4, 0xb2),
    RGB565(0x6f, 0x3d, 0x86), RGB565(0x58, 0x8d, 0x43),
    RGB565(0x35, 0x28, 0x79), RGB565(0xb8, 0xc7, 0x6f),
    RGB565(0x6f, 0x4f, 0x25), RGB565(0x43, 0x39, 0x00),
    RGB565(0x9a, 0x67, 0x59), RGB565(0x44, 0x44, 0x44),
    RGB565(0x6c, 0x6c, 0x6c), RGB565(0x9a, 0xd2, 0x84),
    RGB565(0x6c, 0x5e, 0xb5), RGB565(0x95, 0x95, 0x95),
};

static uint32_t pal_argb[256] = {
    ARGB(0xFF, 0x00, 0x00, 0x00), ARGB(0xFF, 0xFF, 0xFF, 0xFF),
    ARGB(0xFF, 0xFF, 0x00, 0x00), ARGB(0xFF, 0x70, 0xa4, 0xb2),
    ARGB(0xFF, 0x6f, 0x3d, 0x86), ARGB(0xFF, 0x58, 0x8d, 0x43),
    ARGB(0xFF, 0x35, 0x28, 0x79), ARGB(0xFF, 0xb8, 0xc7, 0x6f),
    ARGB(0xFF, 0x6f, 0x4f, 0x25), ARGB(0xFF, 0x43, 0x39, 0x00),
    ARGB(0xFF, 0x9a, 0x67, 0x59), ARGB(0xFF, 0x44, 0x44, 0x44),
    ARGB(0xFF, 0x6c, 0x6c, 0x6c), ARGB(0xFF, 0x9a, 0xd2, 0x84),
    ARGB(0xFF, 0x6c, 0x5e, 0xb5), ARGB(0xFF, 0x95, 0x95, 0x95),

    ARGB(0x00, 0x00, 0x00, 0x00),
};





#define FBL_MAX_LAYERS 4








#define PAL_BANK_MAX 128
static const unsigned s_bank_size[FBL_MAX_LAYERS] = { 128, 32, 32, 64 };
static const unsigned s_bank_base[FBL_MAX_LAYERS] = { 0, 128, 160, 192 };

static CBcmFrameBuffer *s_fb = nullptr;


static CBcmFrameBuffer *s_fb2 = nullptr;
static int s_strato2 = -1;
static uint8_t *s_base = nullptr;
static uint8_t *s_shadow = nullptr;
static unsigned s_pitch = 0;





static unsigned s_bpp = 1;


static uint16_t s_pal16[256];




static uint16_t s_line16[2048];














extern "C" {
void sem_dec(uint32_t *semaphore);
void sem_inc(uint32_t *semaphore);

uint32_t fbl_job = 0;
uint32_t fbl_done = 0;
volatile int fbl_worker_alive = 0;
}

static uint8_t *s_stage = nullptr;
#if RASPPI >= 4
static CDMAChannel *s_dma = nullptr;
#endif
static int s_dma_ok = 0;
static int s_job_in_flight = 0;



static uint8_t *s_job_page = nullptr;
static int s_job_x0 = 0, s_job_y0 = 0, s_job_cols = 0, s_job_rows = 0;



static void do_copy16(uint8_t *page, int x0, int y0, int cols, int rows) {
  const size_t line = (size_t)cols * 2;

  for (int r = 0; r < rows; r++) {
    const uint8_t *src = s_shadow + (size_t)(y0 + r) * s_pitch + x0;
    uint16_t *dst = (uint16_t *)(s_stage + (size_t)r * line);
    int i = 0;
    for (; i + 4 <= cols; i += 4) {
      dst[i + 0] = s_pal16[src[i + 0]];
      dst[i + 1] = s_pal16[src[i + 1]];
      dst[i + 2] = s_pal16[src[i + 2]];
      dst[i + 3] = s_pal16[src[i + 3]];
    }
    for (; i < cols; i++) {
      dst[i] = s_pal16[src[i]];
    }
  }

  uint8_t *dest = page + (size_t)y0 * s_pitch + (size_t)x0 * 2;

#if RASPPI >= 4
  if (s_dma_ok && s_dma != nullptr && rows > 1) {



    s_dma->SetupMemCopy2D(dest, s_stage, line, (unsigned)rows,
                          s_pitch - line, 8);
    s_dma->Start();
    s_dma->Wait();
    return;
  }
#endif

  for (int r = 0; r < rows; r++) {
    memcpy(dest + (size_t)r * s_pitch, s_stage + (size_t)r * line, line);
  }
}

extern "C" void fbl_worker_ready(void) { fbl_worker_alive = 1; }

extern "C" void fbl_worker_run(void) {
  do_copy16(s_job_page, s_job_x0, s_job_y0, s_job_cols, s_job_rows);
}


static void worker_wait(void) {
  if (s_job_in_flight) {
    sem_dec(&fbl_done);
    s_job_in_flight = 0;
  }
}



static void dma_try(void) {
#if RASPPI < 4
  s_dma_ok = 0;
#else
  if (s_dma != nullptr) {
    return;
  }
  s_dma_ok = 0;

  unsigned ch = CMachineInfo::Get()->AllocateDMAChannel(DMA_CHANNEL_EXTENDED);
  if (ch == DMA_CHANNEL_NONE) {
    return;
  }
  CMachineInfo::Get()->FreeDMAChannel(ch);

  s_dma = new CDMAChannel(DMA_CHANNEL_EXTENDED);
  if (s_dma == nullptr) {
    return;
  }


  const size_t BLK = 64, CNT = 4, STR = 16;
  uint8_t *src = (uint8_t *)malloc(BLK * CNT);
  uint8_t *dst = (uint8_t *)malloc((BLK + STR) * CNT);
  if (src != nullptr && dst != nullptr) {
    for (size_t i = 0; i < BLK * CNT; i++) {
      src[i] = (uint8_t)(i * 7 + 1);
    }
    memset(dst, 0, (BLK + STR) * CNT);
    CleanAndInvalidateDataCacheRange((uintptr)dst, (BLK + STR) * CNT);

    s_dma->SetupMemCopy2D(dst, src, BLK, (unsigned)CNT, STR, 0);
    s_dma->Start();
    boolean ok = s_dma->Wait();

    CleanAndInvalidateDataCacheRange((uintptr)dst, (BLK + STR) * CNT);
    if (ok) {
      s_dma_ok = 1;
      for (size_t b = 0; b < CNT; b++) {
        if (memcmp(dst + b * (BLK + STR), src + b * BLK, BLK) != 0) {
          s_dma_ok = 0;
        }
      }
    }
  }
  free(src);
  free(dst);

  if (!s_dma_ok) {
    delete s_dma;
    s_dma = nullptr;
  }
#endif
}
static unsigned s_fb_w = 0;
static unsigned s_fb_h = 0;
static unsigned s_page_bytes = 0;
static int s_visible_page = 0;

static int s_pages = 2;





static FrameBufferLayer *s_layers[FBL_MAX_LAYERS];
static int s_num_layers = 0;




static bool s_need_clear = true;
static int s_full_copies = 2;


static int s_dirty_x = 0;
static int s_dirty_y = 0;
static int s_dirty_w = 0;
static int s_dirty_h = 0;



static int32_t s_xmap[2048];




extern "C" {
unsigned raspi_disp_w = 0, raspi_disp_h = 0;
unsigned raspi_fb_ask_w = 0, raspi_fb_ask_h = 0;
unsigned raspi_fb_got_w = 0, raspi_fb_got_h = 0;
unsigned raspi_fb_pitch = 0, raspi_fb_size = 0;
unsigned raspi_fb_displays = 0;
unsigned raspi_fb_display = 0;
unsigned raspi_fb_pages = 0;
unsigned raspi_fb_bpp = 0;
unsigned raspi_fb_worker = 0;
unsigned raspi_fb_dma = 0;
}

bool FrameBufferLayer::initialized_ = false;
DISPMANX_DISPLAY_HANDLE_T FrameBufferLayer::dispman_display_ = 0;
EGLDisplay FrameBufferLayer::egl_display_ = nullptr;
EGLContext FrameBufferLayer::egl_context_ = nullptr;



static int bank_of_layer(const FrameBufferLayer *l) {
  int b = l->GetBankId();
  if (b < 0 || b >= FBL_MAX_LAYERS) {
    return 0;
  }
  return b;
}






static int s_solco = -1;



static uint16_t scurisci_565(uint16_t c, int centesimi) {
  unsigned r = (c >> 11) & 0x1f;
  unsigned g = (c >> 5) & 0x3f;
  unsigned b = c & 0x1f;
  r = (r * (unsigned)centesimi) / 100u;
  g = (g * (unsigned)centesimi) / 100u;
  b = (b * (unsigned)centesimi) / 100u;
  return (uint16_t)((r << 11) | (g << 5) | b);
}






static int banco_col_solco(int bank, int max_colore) {
  return s_solco >= 0 && s_bank_size[bank] >= 128 && max_colore < 64;
}


static void push_palette_entry(int bank, unsigned index, uint16_t rgb565) {
  if (s_fb == nullptr || index >= s_bank_size[bank]) {
    return;
  }
  s_fb->SetPalette((uint8_t)(s_bank_base[bank] + index), rgb565);
  s_pal16[s_bank_base[bank] + index] = rgb565;
}

static inline uint16_t argb_to_565(uint32_t argb) {
  return (uint16_t)(((argb >> 19) & 0x1F) << 11 | ((argb >> 10) & 0x3F) << 5 |
                    ((argb >> 3) & 0x1F));
}



FrameBufferLayer::FrameBufferLayer()
    : pixels_(nullptr), dispman_element_(0), egl_config_(nullptr),
      egl_surface_(nullptr), fb_width_(0), fb_height_(0), fb_pitch_(0),
      layer_(0), transparency_(false), hstretch_(1.6), vstretch_(1.0),
      hintstr_(0), vintstr_(0), use_hintstr_(0), use_vintstr_(0), valign_(0),
      vpadding_(0), halign_(0), hpadding_(0), h_center_offset_(0),
      v_center_offset_(0), max_pal_(0), rnum_(0), leftPadding_(0),
      rightPadding_(0),
      topPadding_(0), bottomPadding_(0), display_width_(0), display_height_(0),
      src_x_(0), src_y_(0), src_w_(0), src_h_(0), dst_x_(0), dst_y_(0),
      dst_w_(0), dst_h_(0), showing_(false), allocated_(false),
      composed_(false), mode_(VC_IMAGE_8BPP), bytes_per_pixel_(1),
      uses_shader_(false), shader_init_(false), vshader_(0), fshader_(0),
      shader_program_(0), vbo_(0), attr_vertex_(0), attr_texcoord_(0),
      texture_sampler_(0), palette_sampler_(0), tex_(0), pal_(0), mvp_(0),
      input_size_(0), output_size_(0), texture_size_(0), texel_size_(0),
      need_cpu_crop_(false), cropped_pixels_(nullptr), curvature_(false) {
  alpha_.flags = 0;
  alpha_.opacity = 255;
  alpha_.mask = 0;

  memcpy(pal_565_, pal_565, sizeof(pal_565));
  memcpy(pal_argb_, pal_argb, sizeof(pal_argb));

  if (s_num_layers < FBL_MAX_LAYERS) {
    s_layers[s_num_layers++] = this;
  }
}

FrameBufferLayer::~FrameBufferLayer() {
  if (showing_) {
    Hide();
  }
  if (allocated_) {
    Free();
  }
  for (int i = 0; i < s_num_layers; i++) {
    if (s_layers[i] == this) {
      for (int j = i; j < s_num_layers - 1; j++) {
        s_layers[j] = s_layers[j + 1];
      }
      s_num_layers--;
      break;
    }
  }
}

void FrameBufferLayer::Initialize() {
  if (initialized_)
    return;





  unsigned disp_w = 640;
  unsigned disp_h = 480;
  {
    CBcmPropertyTags Tags;
    TPropertyTagDisplayDimensions Dimensions;
    if (Tags.GetTag(PROPTAG_GET_DISPLAY_DIMENSIONS, &Dimensions,
                    sizeof Dimensions)) {




      if (Dimensions.nWidth >= 64 && Dimensions.nHeight >= 64) {
        disp_w = Dimensions.nWidth;
        disp_h = Dimensions.nHeight;
      }
    }
  }

  int want_w = 0, want_h = 0;
  ViceOptions::Get()->GetFBSize(&want_w, &want_h);
  if (want_w > 0 && want_h > 0) {





    s_fb_w = (unsigned)want_w;
    s_fb_h = (unsigned)want_h;
  } else {




    s_fb_w = disp_w;
    s_fb_h = disp_h;
#if RASPPI >= 5





    if (s_fb_w > 2 * s_fb_h) {

      s_fb_w = 320;
      s_fb_h = 240;
    }
    while (s_fb_w > sizeof(s_xmap) / sizeof(s_xmap[0])) {


      s_fb_w /= 2;
      s_fb_h /= 2;
    }
#else
    if (s_fb_w > 2 * s_fb_h) {






      s_fb_w = 320;
      s_fb_h = 240;
    } else if (s_fb_w / 2 >= 320 && s_fb_h / 2 >= 240) {



























      s_fb_w /= 2;
      s_fb_h /= 2;




























      if (disp_w >= 1280 && s_fb_h % 272 != 0) {
        unsigned su = ((s_fb_h / 272) + 1) * 272;
        if (su <= disp_h) {
          s_fb_h = su;
        }
      }
    }
#endif
  }


  if (s_fb_w > sizeof(s_xmap) / sizeof(s_xmap[0])) {
    s_fb_w = sizeof(s_xmap) / sizeof(s_xmap[0]);
  }
  s_fb_w &= ~1u;
  s_fb_h &= ~1u;

  raspi_disp_w = disp_w;
  raspi_disp_h = disp_h;
  raspi_fb_ask_w = s_fb_w;
  raspi_fb_ask_h = s_fb_h;

  unsigned want_disp = (unsigned)ViceOptions::Get()->GetFBDisplay();
  unsigned num_disp = CBcmFrameBuffer::GetNumDisplays();
  if (want_disp >= num_disp) {
    want_disp = 0;
  }
  raspi_fb_displays = num_disp;
  raspi_fb_display = want_disp;

  const boolean dbl = ViceOptions::Get()->GetFBPages() > 1 ? TRUE : FALSE;
  s_fb = new CBcmFrameBuffer(s_fb_w, s_fb_h, 8, s_fb_w,
                             dbl ? s_fb_h * 2 : s_fb_h, want_disp, dbl);
  if (s_fb == nullptr) {
    return;
  }




  for (int bank = 0; bank < FBL_MAX_LAYERS; bank++) {
    for (unsigned i = 0; i < s_bank_size[bank]; i++) {
      uint16_t c0 = i < 16 ? pal_565[i] : (uint16_t)0;
      s_fb->SetPalette((uint8_t)(s_bank_base[bank] + i), c0);
      s_pal16[s_bank_base[bank] + i] = c0;
    }
  }

  if (!s_fb->Initialize()) {
    delete s_fb;
    s_fb = nullptr;
    return;
  }

  raspi_fb_got_w = s_fb->GetWidth();
  raspi_fb_got_h = s_fb->GetHeight();
  raspi_fb_pitch = s_fb->GetPitch();
  raspi_fb_size = s_fb->GetSize();






  if (s_fb->GetWidth() > 0 && s_fb->GetWidth() < s_fb_w) {
    s_fb_w = s_fb->GetWidth();
  }
  if (s_fb->GetHeight() > 0 && s_fb->GetHeight() < s_fb_h) {
    s_fb_h = s_fb->GetHeight();
  }
  s_fb_w &= ~1u;
  s_fb_h &= ~1u;

  s_pitch = s_fb->GetPitch();





  s_bpp = (s_fb_w > 0 && (s_pitch / s_fb_w) >= 2) ? 2 : 1;
  raspi_fb_bpp = s_bpp;
  s_base = (uint8_t *)(uintptr_t)s_fb->GetBuffer();
  s_page_bytes = s_pitch * s_fb_h;
  s_visible_page = 0;


  s_pages = (ViceOptions::Get()->GetFBPages() > 1 &&
             s_fb->GetSize() >= (unsigned)(s_page_bytes * 2)) ? 2 : 1;
  raspi_fb_pages = (unsigned)s_pages;
  if (s_page_bytes == 0 || s_page_bytes > s_fb->GetSize()) {
    delete s_fb;
    s_fb = nullptr;
    return;
  }

  memset(s_base, 0, s_fb->GetSize());




  if (s_pages > 1 && !s_fb->SetVirtualOffset(0, s_fb_h)) {
    s_pages = 1;
    raspi_fb_pages = 1;
  }
  s_fb->SetVirtualOffset(0, 0);

  s_shadow = (uint8_t *)malloc(s_page_bytes);
  if (s_shadow != nullptr) {
    memset(s_shadow, 0, s_page_bytes);
  }




  worker_wait();
  free(s_stage);
  s_stage = nullptr;
  raspi_fb_worker = 0;
  if (s_bpp == 2) {
    s_stage = (uint8_t *)malloc((size_t)s_fb_w * 2 * s_fb_h);
    if (s_stage != nullptr) {
      dma_try();
    }
  }
  raspi_fb_dma = (unsigned)(s_dma_ok ? 1 : 0);

  s_need_clear = true;
  s_full_copies = 2;

  initialized_ = true;
}

void FrameBufferLayer::OGLInit() {}








void FrameBufferLayer::BlitInto(uint8_t *page) {
  if (!showing_ || !allocated_ || pixels_ == nullptr)
    return;
  if (dst_w_ <= 0 || dst_h_ <= 0 || src_w_ <= 0 || src_h_ <= 0)
    return;

  int dx0 = dst_x_ < 0 ? 0 : dst_x_;
  int dy0 = dst_y_ < 0 ? 0 : dst_y_;
  int dx1 = dst_x_ + dst_w_;
  int dy1 = dst_y_ + dst_h_;
  if (dx1 > (int)s_fb_w)
    dx1 = s_fb_w;
  if (dy1 > (int)s_fb_h)
    dy1 = s_fb_h;
  if (dx1 <= dx0 || dy1 <= dy0)
    return;


  const int vx = src_x_ + taglio_x_;
  const int vy = src_y_ + taglio_y_;
  const int vw = src_w_ - 2 * taglio_x_;
  const int vh = src_h_ - 2 * taglio_y_;
  if (vw <= 0 || vh <= 0)
    return;
  const uint32_t x_step = ((uint32_t)vw << 16) / (uint32_t)dst_w_;
  const uint32_t y_step = ((uint32_t)vh << 16) / (uint32_t)dst_h_;

  const int cols = dx1 - dx0;
  if (cols > (int)(sizeof(s_xmap) / sizeof(s_xmap[0])))
    return;

  for (int i = 0; i < cols; i++) {
    uint32_t sx = vx + (((uint32_t)(dx0 - dst_x_ + i) * x_step) >> 16);
    if (sx >= (uint32_t)fb_width_)
      sx = fb_width_ - 1;
    s_xmap[i] = (int32_t)sx;
  }

  const int mio_banco = bank_of_layer(this);
  const uint8_t bank = (uint8_t)s_bank_base[mio_banco];




  const int solco_qui =
      banco_col_solco(mio_banco, max_pal_) && !transparency_ &&
      solco_alto_;
  const uint8_t maschera =
      (uint8_t)((solco_qui ? 64u : s_bank_size[mio_banco]) - 1u);
  uint32_t sy_prima = 0xffffffffu;



  uint8_t skip[PAL_BANK_MAX];
  if (transparency_) {
    for (unsigned i = 0; i < s_bank_size[mio_banco]; i++) {
      skip[i] = (pal_argb_[i] >> 24) < 128;
    }
  }

  for (int dy = dy0; dy < dy1; dy++) {
    uint32_t sy = vy + (((uint32_t)(dy - dst_y_) * y_step) >> 16);
    if (sy >= (uint32_t)fb_height_)
      sy = fb_height_ - 1;

    const uint8_t *srow = pixels_ + (size_t)sy * fb_pitch_;
    uint8_t *drow = page + (size_t)dy * s_pitch + dx0;







    const uint8_t base = (solco_qui && sy == sy_prima)
                             ? (uint8_t)(bank + 64) : bank;
    sy_prima = sy;

    if (!transparency_) {
      for (int i = 0; i < cols; i++) {
        uint8_t idx = srow[s_xmap[i]];
        drow[i] = (uint8_t)(base + (idx & maschera));
      }
    } else {
      for (int i = 0; i < cols; i++) {
        uint8_t idx = (uint8_t)(srow[s_xmap[i]] & maschera);
        if (skip[idx])
          continue;
        drow[i] = (uint8_t)(bank + idx);
      }
    }
  }
}



static void recompute_dirty(void) {
  int x0 = (int)s_fb_w;
  int y0 = (int)s_fb_h;
  int x1 = 0;
  int y1 = 0;

  for (int i = 0; i < s_num_layers; i++) {
    FrameBufferLayer *l = s_layers[i];
    if (!l->Showing())
      continue;
    int dw, dh, lx, ly;
    l->GetDstRect(&lx, &ly, &dw, &dh);
    if (dw <= 0 || dh <= 0)
      continue;
    if (lx < x0) x0 = lx;
    if (ly < y0) y0 = ly;
    if (lx + dw > x1) x1 = lx + dw;
    if (ly + dh > y1) y1 = ly + dh;
  }

  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > (int)s_fb_w) x1 = s_fb_w;
  if (y1 > (int)s_fb_h) y1 = s_fb_h;

  s_dirty_x = x0;
  s_dirty_y = y0;
  s_dirty_w = x1 > x0 ? x1 - x0 : 0;
  s_dirty_h = y1 > y0 ? y1 - y0 : 0;
}

static void Compose(void) {
  if (s_shadow == nullptr)
    return;

  if (s_need_clear) {
    memset(s_shadow, 0, s_page_bytes);
    recompute_dirty();
    s_need_clear = false;
    s_full_copies = 2;
  }



  for (int pass = 0; pass < s_num_layers; pass++) {
    FrameBufferLayer *best = nullptr;
    int best_z = 0;
    for (int i = 0; i < s_num_layers; i++) {
      FrameBufferLayer *l = s_layers[i];
      if (!l->Showing() || l->Composed())
        continue;


      if (s_fb2 != nullptr && l->GetLayer() == s_strato2)
        continue;
      int z = l->GetLayer();
      if (best == nullptr || z < best_z) {
        best = l;
        best_z = z;
      }
    }
    if (best == nullptr)
      break;
    best->BlitInto(s_shadow);
    best->SetComposed(true);
  }

  for (int i = 0; i < s_num_layers; i++) {
    s_layers[i]->SetComposed(false);
  }
}










extern "C" void fbl_scatola_a_schermo(const char *const *righe, int n) {
  if (s_base == nullptr || s_fb_w == 0 || s_fb_h == 0 || righe == nullptr ||
      n <= 0) {
    return;
  }
  if (n > 16) {
    n = 16;
  }
  CCharGenerator font;
  const unsigned cw = font.GetCharWidth();
  const unsigned scala = (s_fb_w >= 1280) ? 2 : 1;
  const unsigned margine = 4 * scala;
  if (s_fb_w <= 2 * margine + 8 * cw * scala || s_fb_h <= 2 * margine) {
    return;
  }







  const unsigned colonne_max = (s_fb_w - 2 * margine) / (cw * scala);
  const char *pezzo[48];
  unsigned lung[48], rientro[48];
  unsigned np = 0, colonne = 0;
  for (int i = 0; i < n && np < 48; i++) {
    const char *s = righe[i] != nullptr ? righe[i] : "";
    const unsigned L = (unsigned)strlen(s);
    unsigned sotto = 2;
    for (unsigned k = 0; k + 1 < L && k < 14; k++) {
      if (s[k] == ':' && s[k + 1] == ' ') {
        sotto = k + 2;
        break;
      }
    }
    if (sotto * 2 > colonne_max) {
      sotto = 2;
    }
    unsigned pos = 0;
    bool primo = true;
    do {
      const unsigned ind = primo ? 0 : sotto;
      const unsigned spazio = colonne_max - ind;
      unsigned taglio = L - pos;
      if (taglio > spazio) {
        taglio = spazio;
        const unsigned minimo = primo ? sotto : 1;
        for (unsigned k = spazio; k > minimo; k--) {
          if (s[pos + k] == ' ') {
            taglio = k;
            break;
          }
        }
      }
      pezzo[np] = s + pos;
      lung[np] = taglio;
      rientro[np] = ind;
      if (ind + taglio > colonne) {
        colonne = ind + taglio;
      }
      np++;
      pos += taglio;
      while (pos < L && s[pos] == ' ') {
        pos++;
      }
      primo = false;
    } while (pos < L && np < 48);
  }

  unsigned passo = font.GetCharHeight();
  if (np * passo * scala + 2 * margine > s_fb_h) {
    passo = font.GetUnderline();
  }
  const unsigned max_righe = (s_fb_h - 2 * margine) / (passo * scala);
  if (np > max_righe) {
    np = max_righe;
  }
  if (np == 0) {
    return;
  }
  unsigned w = colonne * cw * scala + 2 * margine;
  unsigned h = np * passo * scala + 2 * margine;
  if (w > s_fb_w) {
    w = s_fb_w;
  }
  if (h > s_fb_h) {
    h = s_fb_h;
  }

  uint8_t chiaro = 0, scuro = 0;
  unsigned lum_max = 0, lum_min = ~0u;
  for (unsigned i = 0; i < 256; i++) {
    const unsigned c = s_pal16[i];
    const unsigned lum = ((c >> 11) & 31) * 2 + ((c >> 5) & 63) + (c & 31) * 2;
    if (lum > lum_max) {
      lum_max = lum;
      chiaro = (uint8_t)i;
    }
    if (lum < lum_min) {
      lum_min = lum;
      scuro = (uint8_t)i;
    }
  }

  for (int p = 0; p < s_pages; p++) {
    uint8_t *pagina = s_base + (size_t)p * s_page_bytes;
    for (unsigned y = 0; y < h; y++) {
      uint8_t *riga = pagina + (size_t)y * s_pitch;
      for (unsigned x = 0; x < w; x++) {
        bool acceso = false;
        if (x >= margine && y >= margine) {
          const unsigned cx = (x - margine) / scala;
          const unsigned cy = (y - margine) / scala;
          const unsigned r = cy / passo;
          const unsigned col = cx / cw;
          if (r < np && col >= rientro[r] && col - rientro[r] < lung[r]) {
            acceso = font.GetPixel(pezzo[r][col - rientro[r]], cx % cw,
                                   cy % passo);
          }
        }
        if (s_bpp == 2) {
          ((volatile uint16_t *)riga)[x] = acceso ? 0xFFFF : 0x0000;
        } else {
          ((volatile uint8_t *)riga)[x] = acceso ? chiaro : scuro;
        }
      }
    }
  }
}



extern "C" void fbl_scatola_via(void) { s_full_copies = 2; }



static void Present(uint8_t *page) {
  if (s_shadow == nullptr)
    return;

  if (s_bpp == 2) {



    int y0 = 0, y1 = (int)s_fb_h, x0 = 0, x1 = (int)s_fb_w;
    if (s_full_copies > 0) {
      s_full_copies--;
    } else {
      if (s_dirty_w <= 0 || s_dirty_h <= 0)
        return;
      y0 = s_dirty_y; y1 = s_dirty_y + s_dirty_h;
      x0 = s_dirty_x; x1 = s_dirty_x + s_dirty_w;
    }












    x0 &= ~7;
    x1 = (x1 + 7) & ~7;
    if (x1 > (int)s_fb_w)
      x1 = (int)s_fb_w;
    if (x0 > x1)
      x0 = x1;

    int cols = x1 - x0;
    const int cap = (int)(sizeof(s_line16) / sizeof(s_line16[0]));
    if (cols > cap)
      cols = cap;
    if (cols <= 0)
      return;

    const int rows = y1 - y0;
    if (rows <= 0)
      return;

    if (fbl_worker_alive && s_stage != nullptr) {

      s_job_page = page;
      s_job_x0 = x0;
      s_job_y0 = y0;
      s_job_cols = cols;
      s_job_rows = rows;
      s_job_in_flight = 1;
      raspi_fb_worker = 1;
      sem_inc(&fbl_job);
      return;
    }

    if (s_stage != nullptr) {
      do_copy16(page, x0, y0, cols, rows);
      return;
    }


    for (int y = y0; y < y1; y++) {
      const uint8_t *src = s_shadow + (size_t)y * s_pitch + x0;
      uint16_t *dst = (uint16_t *)(page + (size_t)y * s_pitch) + x0;
      int i = 0;
      for (; i + 4 <= cols; i += 4) {
        s_line16[i + 0] = s_pal16[src[i + 0]];
        s_line16[i + 1] = s_pal16[src[i + 1]];
        s_line16[i + 2] = s_pal16[src[i + 2]];
        s_line16[i + 3] = s_pal16[src[i + 3]];
      }
      for (; i < cols; i++) {
        s_line16[i] = s_pal16[src[i]];
      }
      memcpy(dst, s_line16, (size_t)cols * 2);
    }
    return;
  }

  if (s_full_copies > 0) {
    memcpy(page, s_shadow, s_page_bytes);
    s_full_copies--;
    return;
  }

  if (s_dirty_w <= 0 || s_dirty_h <= 0)
    return;

  const size_t run = (size_t)s_dirty_w;
  for (int y = s_dirty_y; y < s_dirty_y + s_dirty_h; y++) {
    size_t off = (size_t)y * s_pitch + s_dirty_x;
    memcpy(page + off, s_shadow + off, run);
  }
}





























static uint8_t *s_base2 = nullptr;
static unsigned s_pitch2 = 0;
static unsigned s_bpp2 = 1;
static unsigned s_fb2_w = 0;
static unsigned s_fb2_h = 0;
static uint16_t s_pal2_16[256];
static int32_t s_xmap2[2048];
static uint16_t s_line2_16[2048];


unsigned raspi_fb2_w = 0;
unsigned raspi_fb2_h = 0;
int raspi_fb2_strato = -1;








static uint64_t s_firma2 = 0;
static int s_secondo_forza = 1;
static int s_secondo_nero = 0;


static int s_d2x = 0;
static int s_d2y = 0;
static int s_d2w = -1;
static int s_d2h = -1;

static uint64_t firma_della_tela(const uint8_t *p, int pitch,
                                 int x, int y, int w, int h) {
  uint64_t f = 14695981039346656037ULL;
  for (int r = 0; r < h; r++) {
    const uint8_t *q = p + (size_t)(y + r) * (size_t)pitch + (size_t)x;
    int n = w;
    while (n >= 8) {
      uint64_t v;
      memcpy(&v, q, sizeof(v));
      f = (f ^ v) * 1099511628211ULL;
      q += 8;
      n -= 8;
    }
    while (n-- > 0) {
      f = (f ^ (uint64_t)*q++) * 1099511628211ULL;
    }
  }
  return f;
}

static void secondo_chiudi(void) {
  if (s_fb2 != nullptr) {
    delete s_fb2;
    s_fb2 = nullptr;
  }
  s_base2 = nullptr;
  s_pitch2 = 0;
  s_bpp2 = 1;
  s_fb2_w = 0;
  s_fb2_h = 0;
  s_d2w = -1;
  s_d2h = -1;
  s_strato2 = -1;
  raspi_fb2_w = 0;
  raspi_fb2_h = 0;
  raspi_fb2_strato = -1;
}


static int secondo_apri(int strato, int forza) {
  if (s_fb == nullptr) {
    return 0;
  }









  if (forza) {
    CBcmFrameBuffer::InvalidateNumDisplays();
  }
  if (CBcmFrameBuffer::GetNumDisplays() < 2) {
    return 0;
  }




  unsigned altra = raspi_fb_display == 0 ? 1 : 0;
  if (altra >= CBcmFrameBuffer::GetNumDisplays()) {
    return 0;
  }

  secondo_chiudi();




  s_fb2 = new CBcmFrameBuffer(s_fb_w, s_fb_h, 8, s_fb_w, s_fb_h, altra,
                              FALSE);
  if (s_fb2 == nullptr) {
    return 0;
  }
  for (unsigned i = 0; i < 256; i++) {
    s_fb2->SetPalette((uint8_t)i, (uint16_t)0);
    s_pal2_16[i] = 0;
  }
  if (!s_fb2->Initialize()) {
    delete s_fb2;
    s_fb2 = nullptr;
    return 0;
  }

  s_base2 = (uint8_t *)(uintptr_t)s_fb2->GetBuffer();
  s_pitch2 = s_fb2->GetPitch();
  s_fb2_w = s_fb2->GetWidth();
  s_fb2_h = s_fb2->GetHeight();


  s_bpp2 = (s_fb2_w > 0 && s_pitch2 / s_fb2_w >= 2) ? 2 : 1;

  if (s_base2 == nullptr || s_fb2_w == 0 || s_fb2_h == 0) {
    secondo_chiudi();
    return 0;
  }
  memset(s_base2, 0, (size_t)s_pitch2 * s_fb2_h);

  s_strato2 = strato;
  s_secondo_forza = 1;
  s_secondo_nero = 0;
  raspi_fb2_w = s_fb2_w;
  raspi_fb2_h = s_fb2_h;
  raspi_fb2_strato = strato;
  return 1;
}



static void secondo_tavolozza(const uint16_t *pal, unsigned quante) {
  if (s_fb2 == nullptr) {
    return;
  }
  for (unsigned i = 0; i < quante && i < 256; i++) {
    s_pal2_16[i] = pal[i];
    s_fb2->SetPalette((uint8_t)i, pal[i]);
  }
  s_fb2->UpdatePalette();



  s_secondo_forza = 1;
}

void FrameBufferLayer::BlitIntoSecondScreen(void) {
  if (s_fb2 == nullptr || s_base2 == nullptr) {
    return;
  }
  if (!allocated_ || pixels_ == nullptr) {
    return;
  }
  if (src_w_ <= 0 || src_h_ <= 0) {
    return;
  }
  if (s_fb2_w > sizeof(s_xmap2) / sizeof(s_xmap2[0])) {
    return;
  }


  if (!Showing()) {
    if (!s_secondo_nero) {
      memset(s_base2, 0, (size_t)s_pitch2 * s_fb2_h);
      s_secondo_nero = 1;
      s_secondo_forza = 1;
    }
    return;
  }
  s_secondo_nero = 0;













  int d2x, d2y, d2w, d2h;
  if (display_width_ > 0 && display_height_ > 0 &&
      dst_w_ > 0 && dst_h_ > 0) {
    d2x = (int)((int64_t)dst_x_ * (int64_t)s_fb2_w / display_width_);
    d2y = (int)((int64_t)dst_y_ * (int64_t)s_fb2_h / display_height_);
    d2w = (int)((int64_t)dst_w_ * (int64_t)s_fb2_w / display_width_);
    d2h = (int)((int64_t)dst_h_ * (int64_t)s_fb2_h / display_height_);
  } else {

    d2x = 0;
    d2y = 0;
    d2w = (int)s_fb2_w;
    d2h = (int)s_fb2_h;
  }
  if (d2x < 0) { d2w += d2x; d2x = 0; }
  if (d2y < 0) { d2h += d2y; d2y = 0; }
  if (d2x + d2w > (int)s_fb2_w) { d2w = (int)s_fb2_w - d2x; }
  if (d2y + d2h > (int)s_fb2_h) { d2h = (int)s_fb2_h - d2y; }
  if (d2w <= 0 || d2h <= 0) {
    return;
  }




  {
    uint64_t firma = firma_della_tela(pixels_, fb_pitch_,
                                      src_x_, src_y_, src_w_, src_h_);
    firma ^= ((uint64_t)(uint32_t)src_x_ << 48) ^
             ((uint64_t)(uint32_t)src_y_ << 32) ^
             ((uint64_t)(uint32_t)src_w_ << 16) ^
              (uint64_t)(uint32_t)src_h_;
    firma ^= ((uint64_t)(uint32_t)d2x << 40) ^
             ((uint64_t)(uint32_t)d2y << 24) ^
             ((uint64_t)(uint32_t)d2w << 8) ^
              (uint64_t)(uint32_t)d2h;
    if (!s_secondo_forza && firma == s_firma2) {
      return;
    }
    s_firma2 = firma;
    s_secondo_forza = 0;
  }



  if (d2x != s_d2x || d2y != s_d2y || d2w != s_d2w || d2h != s_d2h) {
    memset(s_base2, 0, (size_t)s_pitch2 * s_fb2_h);
    s_d2x = d2x;
    s_d2y = d2y;
    s_d2w = d2w;
    s_d2h = d2h;
  }

  const uint32_t x_step = ((uint32_t)src_w_ << 16) / (uint32_t)d2w;
  const uint32_t y_step = ((uint32_t)src_h_ << 16) / (uint32_t)d2h;

  for (int i = 0; i < d2w; i++) {
    uint32_t sx = src_x_ + (((uint32_t)i * x_step) >> 16);
    if (sx >= (uint32_t)fb_width_) {
      sx = fb_width_ - 1;
    }
    s_xmap2[i] = (int32_t)sx;
  }

  for (int dy = 0; dy < d2h; dy++) {
    uint32_t sy = src_y_ + (((uint32_t)dy * y_step) >> 16);
    if (sy >= (uint32_t)fb_height_) {
      sy = fb_height_ - 1;
    }
    const uint8_t *srow = pixels_ + (size_t)sy * fb_pitch_;
    uint8_t *drow = s_base2 + (size_t)(d2y + dy) * s_pitch2;

    if (s_bpp2 == 2) {



      for (int i = 0; i < d2w; i++) {
        s_line2_16[i] = s_pal2_16[srow[s_xmap2[i]]];
      }
      memcpy(drow + (size_t)d2x * 2, s_line2_16, (size_t)d2w * 2);
    } else {
      for (int i = 0; i < d2w; i++) {
        drow[d2x + i] = srow[s_xmap2[i]];
      }
    }
  }
}



extern "C" int circle_secondo_schermo(int strato, int forza) {
  if (strato < 0) {
    secondo_chiudi();
    s_need_clear = true;
    return 0;
  }
  if (forza) {
    CBcmFrameBuffer::InvalidateNumDisplays();
  }
  if (CBcmFrameBuffer::GetNumDisplays() < 2) {
    return -1;
  }
  if (!secondo_apri(strato, forza)) {
    return -1;
  }





  for (int i = 0; i < s_num_layers; i++) {
    if (s_layers[i] != nullptr && s_layers[i]->GetLayer() == strato) {
      s_layers[i]->UpdatePalette();
      break;
    }
  }
  s_need_clear = true;
  return 1;
}






extern "C" void fbl_scanline_set(int centesimi) {



  int nuovo = (centesimi < 0) ? -1 : (centesimi > 100 ? 100 : centesimi);

  if (nuovo == s_solco) {
    return;
  }
  s_solco = nuovo;

  for (int i = 0; i < s_num_layers; i++) {
    if (s_layers[i] != nullptr) {
      s_layers[i]->UpdatePalette();


      if (s_layers[i]->IsShowing()) {
        s_layers[i]->Hide();
        s_layers[i]->Show();
      }
    }
  }


  s_need_clear = true;
  s_full_copies = 2;
}

extern "C" int circle_secondo_schermo_info(int *w, int *h, int *displays) {
  if (w != nullptr) {
    *w = (int)s_fb2_w;
  }
  if (h != nullptr) {
    *h = (int)s_fb2_h;
  }
  if (displays != nullptr) {
    *displays = (int)CBcmFrameBuffer::GetNumDisplays();
  }
  return s_strato2;
}



extern "C" {
volatile unsigned fbl_composizioni = 0;
}

void FrameBufferLayer::SwapResources(bool sync, FrameBufferLayer *fb1,
                                     FrameBufferLayer *fb2) {




  if (s_fb2 != nullptr && s_strato2 >= 0) {
    if (fb1 != nullptr && fb1->GetLayer() == s_strato2) {
      fb1->BlitIntoSecondScreen();
    } else if (fb2 != nullptr && fb2->GetLayer() == s_strato2) {
      fb2->BlitIntoSecondScreen();
    }
  }

  if (!initialized_ || s_fb == nullptr)
    return;



  worker_wait();

  fbl_composizioni++;
  Compose();

  if (!sync) {



    Present(s_base + (size_t)s_visible_page * s_page_bytes);
    return;
  }

  const int back = (s_pages > 1) ? 1 - s_visible_page : 0;
  Present(s_base + (size_t)back * s_page_bytes);

  if (s_pages > 1) {


    worker_wait();
    s_fb->SetVirtualOffset(0, back ? s_fb_h : 0);
  }
  s_fb->WaitForVerticalSync();
  s_visible_page = back;
}





int FrameBufferLayer::Allocate(int pixelmode, uint8_t **pixels, int width,
                               int height, int *pitch) {
  assert(!allocated_);
  allocated_ = true;





  mode_ = VC_IMAGE_8BPP;
  bytes_per_pixel_ = 1;
  (void)pixelmode;

  fb_pitch_ = ALIGN_UP(width * bytes_per_pixel_, 32);
  if (pitch) {
    *pitch = fb_pitch_;
  }

  fb_width_ = width;
  fb_height_ = height;




  display_width_ = s_fb_w;
  display_height_ = s_fb_h;

  if (pixels) {
    pixels_ = (uint8_t *)malloc((size_t)fb_pitch_ * height);
    if (pixels_) {
      memset(pixels_, 0, (size_t)fb_pitch_ * height);
    }
    *pixels = pixels_;

    dst_x_ = 0;
    dst_y_ = 0;
    dst_w_ = width;
    dst_h_ = height;

    src_x_ = 0;
    src_y_ = 0;
    src_w_ = width;
    src_h_ = height;
  }

  s_need_clear = true;
  return 0;
}

int FrameBufferLayer::ReAllocate(bool shader_enable) {

  (void)shader_enable;
  return 0;
}

void FrameBufferLayer::FreeInternal(bool keepPixels) {
  if (showing_) {
    Hide();
  }
  if (!keepPixels && pixels_) {
    free(pixels_);
    pixels_ = nullptr;
  }
  allocated_ = keepPixels;
}

void FrameBufferLayer::Free() {
  if (!allocated_)
    return;
  FreeInternal(false);
  allocated_ = false;
  s_need_clear = true;
}

void FrameBufferLayer::Clear() {
  if (!allocated_ || pixels_ == nullptr)
    return;
  memset(pixels_, 0, (size_t)fb_pitch_ * fb_height_);
}

void *FrameBufferLayer::GetPixels() { return pixels_; }


void FrameBufferLayer::FrameReady(int to_offscreen) { (void)to_offscreen; }





void FrameBufferLayer::Show() {
  if (showing_)
    return;



  if (hstretch_ == 0) {
    hstretch_ = 1.6;
  }
  if (vstretch_ == 0) {
    vstretch_ = 1.0;
  }




  int lpad_abs = display_width_ * leftPadding_;
  int rpad_abs = display_width_ * rightPadding_;
  int tpad_abs = display_height_ * topPadding_;
  int bpad_abs = display_height_ * bottomPadding_;

  int avail_width = display_width_ - lpad_abs - rpad_abs;
  int avail_height = display_height_ - tpad_abs - bpad_abs;

  int dst_w;
  int dst_h;

























  double tela_x = raspi_disp_w > 0
                      ? (double)display_width_ / (double)raspi_disp_w
                      : 1.0;
  double tela_y = raspi_disp_h > 0
                      ? (double)display_height_ / (double)raspi_disp_h
                      : 1.0;
  double forma = tela_y > 0.0 ? tela_x / tela_y : 1.0;

  if (hstretch_ < 0) {
    dst_w = avail_width * vstretch_;
    dst_h = (avail_width / -hstretch_) / forma;
    if (dst_w > avail_width)
      dst_w = avail_width;
    if (dst_h > avail_height)
      dst_h = avail_height;
  } else {
    dst_h = avail_height * vstretch_;
    if (use_vintstr_)
      dst_h = vintstr_ * tela_y;
    dst_w = avail_height * hstretch_ * forma;
    if (use_hintstr_)
      dst_w = hintstr_ * tela_x;
    if (dst_h > avail_height)
      dst_h = avail_height;
    if (dst_w > avail_width)
      dst_w = avail_width;
  }











  taglio_x_ = 0;
  taglio_y_ = 0;
  if (s_solco >= 0 && !transparency_ && src_h_ > 0 && src_w_ > 0 &&
      banco_col_solco(bank_of_layer(this), max_pal_)) {
    int giu = dst_h / src_h_;
    if (giu < 1 || giu * src_h_ != dst_h) {
      int su = giu + 1;
      int righe = dst_h / su;
      int larga = (int)((int64_t)dst_w * su * src_h_ / dst_h);
      int colonne = src_w_;
      if (larga > avail_width) {
        colonne = (int)((int64_t)src_w_ * avail_width / larga);
        larga = avail_width;
      }
      if (su < 2) {






      } else if (righe * 6 >= src_h_ * 5 && colonne * 6 >= src_w_ * 5) {
        taglio_y_ = (src_h_ - righe) / 2;
        taglio_x_ = (src_w_ - colonne) / 2;
        dst_h = righe * su;
        dst_w = larga;
      } else if (giu >= 2 && giu * src_h_ * 10 >= dst_h * 9) {












        int alta = giu * src_h_;
        dst_w = (int)((int64_t)dst_w * alta / dst_h);
        dst_h = alta;
      }
    }

    int vh = src_h_ - 2 * taglio_y_;
    solco_alto_ = (vh > 0 && dst_h >= 2 * vh && dst_h % vh == 0);
  } else {
    solco_alto_ = 0;
  }

  int oy;
  switch (valign_) {
  case 0:
    oy = (avail_height - dst_h) / 2 + v_center_offset_;
    break;
  case -1:
    oy = vpadding_;
    break;
  case 1:
    oy = avail_height - dst_h - vpadding_;
    break;
  default:
    oy = 0;
    break;
  }

  int ox;
  switch (halign_) {
  case 0:
    ox = (avail_width - dst_w) / 2 + h_center_offset_;
    break;
  case -1:
    ox = hpadding_;
    break;
  case 1:
    ox = avail_width - dst_w - hpadding_;
    break;
  default:
    ox = 0;
    break;
  }

  dst_x_ = ox + lpad_abs;
  dst_y_ = oy + tpad_abs;
  dst_w_ = dst_w;
  dst_h_ = dst_h;

  showing_ = true;
  s_need_clear = true;

  UpdatePalette();
  SwapResources(false, this, nullptr);
}

void FrameBufferLayer::Hide() {
  if (!showing_)
    return;
  showing_ = false;
  s_need_clear = true;
  SwapResources(false, this, nullptr);
}





void FrameBufferLayer::SetPalette(uint8_t index, uint16_t rgb565) {
  assert(!transparency_);
  if ((int)index > max_pal_) {
    max_pal_ = index;
  }
  pal_565_[index] = rgb565;
}

void FrameBufferLayer::SetPalette(uint8_t index, uint32_t argb) {
  if ((int)index > max_pal_) {
    max_pal_ = index;
  }
  assert(transparency_);
  pal_argb_[index] = argb;
}


void FrameBufferLayer::UpdatePalette() {
  if (s_fb == nullptr)
    return;

  const int bank = bank_of_layer(this);
  const int solco = banco_col_solco(bank, max_pal_);
  for (unsigned i = 0; i < s_bank_size[bank]; i++) {
    uint16_t c;
    if (transparency_) {
      uint32_t argb = pal_argb_[i];
      // Transparent entries are never written by the blit, so their colour

      c = (argb >> 24) < 128 ? 0 : argb_to_565(argb);
    } else {
      c = pal_565_[i];
    }




    if (solco && i >= 64) {
      uint16_t chiaro;
      if (transparency_) {
        uint32_t argb = pal_argb_[i - 64];
        chiaro = (argb >> 24) < 128 ? 0 : argb_to_565(argb);
      } else {
        chiaro = pal_565_[i - 64];
      }
      c = scurisci_565(chiaro, s_solco);
    }
    push_palette_entry(bank, i, c);
  }

  s_fb->UpdatePalette();




  if (s_fb2 != nullptr && GetLayer() == s_strato2) {
    uint16_t pal[256];
    for (unsigned i = 0; i < 256; i++) {
      if (transparency_) {
        uint32_t argb = pal_argb_[i];
        pal[i] = (argb >> 24) < 128 ? 0 : argb_to_565(argb);
      } else {
        pal[i] = pal_565_[i];
      }
    }
    secondo_tavolozza(pal, 256);
  }
}





void FrameBufferLayer::GetDstRect(int *x, int *y, int *w, int *h) {
  *x = dst_x_;
  *y = dst_y_;
  *w = dst_w_;
  *h = dst_h_;
}

void FrameBufferLayer::SetLayer(int layer) {
  if (layer_ != layer)
    s_need_clear = true;
  layer_ = layer;
}

int FrameBufferLayer::GetLayer() { return layer_; }

bool FrameBufferLayer::UsesShader() { return false; }

bool FrameBufferLayer::Showing() { return showing_; }

bool FrameBufferLayer::Composed() { return composed_; }

void FrameBufferLayer::SetComposed(bool composed) { composed_ = composed; }

void FrameBufferLayer::SetUsesShader(bool enabled) {


  (void)enabled;
  uses_shader_ = false;
}

void FrameBufferLayer::SetShaderParams(
    bool curvature, float curvature_x, float curvature_y, int mask,
    float mask_brightness, bool gamma, bool fake_gamma, bool scanlines,
    bool multisample, float scanline_weight, float scanline_gap_brightness,
    float bloom_factor, float input_gamma, float output_gamma, bool sharper,
    bool bilinear_interpolation) {
  (void)curvature; (void)curvature_x; (void)curvature_y; (void)mask;
  (void)mask_brightness; (void)gamma; (void)fake_gamma; (void)scanlines;
  (void)multisample; (void)scanline_weight; (void)scanline_gap_brightness;
  (void)bloom_factor; (void)input_gamma; (void)output_gamma; (void)sharper;
  (void)bilinear_interpolation;
}

void FrameBufferLayer::SetTransparency(bool transparency) {
  transparency_ = transparency;
}

void FrameBufferLayer::SetSrcRect(int x, int y, int w, int h) {
  if (x != src_x_ || y != src_y_ || w != src_w_ || h != src_h_) {
    s_need_clear = true;
  }
  bool cambia = (x != src_x_ || y != src_y_ || w != src_w_ || h != src_h_);
  src_x_ = x;
  src_y_ = y;
  src_w_ = w;
  src_h_ = h;

  if (cambia && showing_ && s_solco >= 0) {
    Hide();
    Show();
  }
}

void FrameBufferLayer::SetStretch(double hstretch, double vstretch,
                                  int hintstr, int vintstr, int use_hintstr,
                                  int use_vintstr) {
  hstretch_ = hstretch;
  vstretch_ = vstretch;
  hintstr_ = hintstr;
  vintstr_ = vintstr;
  use_hintstr_ = use_hintstr;
  use_vintstr_ = use_vintstr;
}

void FrameBufferLayer::SetVerticalAlignment(int alignment, int padding) {
  valign_ = alignment;
  vpadding_ = padding;
}

void FrameBufferLayer::SetHorizontalAlignment(int alignment, int padding) {
  halign_ = alignment;
  hpadding_ = padding;
}

void FrameBufferLayer::SetPadding(double leftPadding, double rightPadding,
                                  double topPadding, double bottomPadding) {
  leftPadding_ = leftPadding;
  rightPadding_ = rightPadding;
  topPadding_ = topPadding;
  bottomPadding_ = bottomPadding;
}

void FrameBufferLayer::SetCenterOffset(int cx, int cy) {
  h_center_offset_ = cx;
  v_center_offset_ = cy;
}

void FrameBufferLayer::GetDimensions(int *display_w, int *display_h, int *fb_w,
                                     int *fb_h, int *src_w, int *src_h,
                                     int *dst_w, int *dst_h) {
  *display_w = display_width_;
  *display_h = display_height_;
  *fb_w = fb_width_;
  *fb_h = fb_height_;
  *src_w = src_w_;
  *src_h = src_h_;
  *dst_w = dst_w_;
  *dst_h = dst_h_;
}




void FrameBufferLayer::SetInterpolation(int enable) { (void)enable; }


void FrameBufferLayer::Swap(DISPMANX_UPDATE_HANDLE_T &u) { (void)u; }
void FrameBufferLayer::SwapGL(bool sync) { (void)sync; }
void FrameBufferLayer::RenderGL() {}
void FrameBufferLayer::ShaderInit() {}
void FrameBufferLayer::ShaderDestroy() {}
void FrameBufferLayer::CreateTexture() {}
void FrameBufferLayer::ReCreateTexture() {}
void FrameBufferLayer::ShaderUpdate() {}
void FrameBufferLayer::EnableShader() {}
void FrameBufferLayer::DisableShader() {}
void FrameBufferLayer::ConcatShaderDefines(char *dst) { (void)dst; }

#endif

//
// fbl.h
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _fb2_h
#define _fb2_h

#include <cstdint>

struct bmx_crt_effect_params;

#ifndef BMX_PI4_LEGACY_DISPLAY
#define BMX_PI4_LEGACY_DISPLAY 0
#endif

#if RASPPI == 5 || (RASPPI == 4 && !BMX_PI4_LEGACY_DISPLAY)
#include <circle/bcmframebuffer.h>
#if RASPPI == 4
#include "pi4kms/pi4_native_kms.h"
namespace pi5kms = pi4nativekms;
#else
#include "pi5kms/pi5_kms.h"
#endif
#else
#include "bcm_host.h"
#include "GLES2/gl2.h"
#include "GLES2/gl2ext.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"
#if RASPPI == 4
#include "pi4kms/pi4_kms.h"
#include "pi4v3d/pi4_v3d.h"
#endif
#endif

// A wrapper that manages a single dispmanx layer and
// indexed frame buffer.
class FrameBufferLayer {
public:
  FrameBufferLayer();
  virtual ~FrameBufferLayer();

  // Sets the layer for this frame buffer. Must be called before Allocate.
  void SetLayer(int layer);
  int GetLayer(void);

  // Tells this fb to use a palette with transparency.
  // Must be called before Allocate.  If transparency is used, palette
  // calls must use the 32 bit ARGB format.  Otherwise, the RGB565 format
  // is used.
  void SetTransparency(bool transparency);

  // pixel mode: 0=8-bit-indexed, 1=RGB565
  // pitch represents bytes per line
  int Allocate(int pixelmode, uint8_t **pixels,
               int width, int height, int *pitch);

  // Allocate again with the same parameters. pixels and
  // pitch will remain the same.  Used for turning the shader
  // on/off.
  int ReAllocate(bool shader_enable);

  void Free();
  void Clear();

  // Get a pointer to raw pixel data for this frame buffer
  void* GetPixels();

  // Indicates raw pixel data has a complete frame. Upload to the
  // offscreen resource unless to_offscreen is 0, in which case
  // the currently visible resource is the destination.
  void FrameReady(int to_offscreen);

  // Show the framebuffer.
  void Show();

  // Hide the framebuffer.
  void Hide();

  // Set one color of the indexed palette. Can only be called when
  // transparency is false.
  void SetPalette(uint8_t index, uint16_t rgb565);

  // Set one color of the indexed palette. Can only be called when
  // transparency is true.
  void SetPalette(uint8_t index, uint32_t rgba);

  // Commit current palette to frame buffer
  void UpdatePalette();

  // Sets the region within the frame buffer to scale. Takes effect
  // on the next call to Show()
  void SetSrcRect(int x, int y, int w, int h);

  // The amount to stretch to vertical and horizontal dimensions
  // If hstretch is positive, the src region height is scaled up to the
  // height of the frame buffer * vstretch, the width is then determined by
  // frame buffer height * hstretch.
  // Otherwise, the src region width is scaled up to the width of the
  // frame buffer * vstretch, the height is determined by 
  // frame buffer width / hstretch.
  void SetStretch(double hstretch, double vstretch, int hintstr, int vintstr, int use_hintstr, int use_vintstr);

  void SetCenterOffset(int cx, int cy);

  // alignment can be -1 = ALIGN TOP, 0 = CENTER, 1 = ALIGN BOTTOM
  // padding applies to TOP or BOTTOM only.
  void SetVerticalAlignment(int alignment, int padding);

  // alignment can be -1 = ALIGN LEFT, 0 = CENTER, 1 = ALIGN RIGHT
  // padding applies to LEFT or RIGHT only.
  void SetHorizontalAlignment(int alignment, int padding);

  // Takes some space away from the full screen when determining destination rect
  // Used to force a fb into a smaller space (for things like PIP or side-by-side.
  void SetPadding(double leftPadding, double rightPadding, double topPadding, double bottomPadding);

  // Retrieve dimensions for this layer. 
  void GetDimensions(int *display_w, int *display_h,
                     int *fb_w, int *fb_h,
                     int *src_w, int *src_h,
                     int *dst_w, int *dst_h);

  // initializes the bcm_host interface
  static bool Initialize();
  static void Shutdown();
  static bool OGLInit();
  static bool ShaderBackendAvailable();
  static bool ShaderBackendAvailableForLayer(int logical_layer);

  bool UsesShader();

  bool Showing();

  void SetUsesShader(bool enable);

  // NOTE: This will implicitly Hide the layer since the shader must be
  // destroyed and recompiled.
  void SetShaderParams(const struct bmx_crt_effect_params &params);

  // Make offscreen resources for all ready layers visible.
  static void PresentLayers(bool sync, FrameBufferLayer *layers, uint32_t ready_mask);
  static void PresentLayer(bool sync, FrameBufferLayer *layer);

  static void SetInterpolation(int enable);

  // One-shot capture of the fully composed visible display. The caller owns
  // the output buffer; no capture storage remains allocated afterwards.
  static bool CaptureDimensions(int *width, int *height);
  static bool CaptureRgb888(uint8_t *output, int width, int height,
                            unsigned pitch);




  void BlitIntoSecondScreen(void);
  int GetLogicalLayer(void) const { return logical_layer_; }
  const uint16_t *GetPalette565(void) const { return pal_565_; }



  static bool BmcFwPrepara(FrameBufferLayer **layers, unsigned count);
  static FrameBufferLayer *BmcFwFoto(unsigned posto, FrameBufferLayer *layer);
  static void BmcFwMetti(FrameBufferLayer *layer, int start_x, int end_x,
                         int start_y, int end_y, int64_t x_step,
                         int64_t y_step, bool bilineare);
  static void BmcFwManda(int left, int top, int right, int bottom,
                         bool valido, bool attendi, uint64_t inizio_us);
  static void BmcLavoroFirmware(void);

private:
  static void PresentLayerList(bool sync, FrameBufferLayer **layers, unsigned count);
  void FreeInternal(bool keepPixels);
#if BMX_PI4_LEGACY_DISPLAY
  void Swap(DISPMANX_UPDATE_HANDLE_T& dispman_update);
  void SwapGL(bool sync);
  void RenderGL();
  bool EnsureDispmanResources();
#if RASPPI == 4
  bool CanUsePi4V3d() const;
  bool AllocatePi4V3dResources(unsigned width, unsigned height);
  bool EnsurePi4V3dDispmanResources();
  bool UploadPi4V3dDispmanResource(unsigned resource_index);
  void FreePi4V3dResources();
  bool RenderPi4V3dFrame(unsigned resource_index);
  bool ShouldDeferPi4DispmanResources() const;
  bool ReleasePi4KmsDispmanResources();
  bool CanUsePi4KmsOverlay() const;
  bool AllocatePi4KmsOverlayResources(unsigned width, unsigned height);
  void FreePi4KmsOverlayResources();
  bool BuildPi4KmsOverlayPlane(unsigned resource_index,
                               pi4kms::Plane *plane);
  static bool TryPi4KmsPresent(bool sync, FrameBufferLayer **layers,
                               unsigned count);
#endif

  bool ShaderInit();
  void ShaderDestroy();
  void CreateTexture();
  void ReCreateTexture();
  void ShaderUpdate();

  void EnableShader();
  void DisableShader();

  void ConcatShaderDefines(char *dst);
#else
  void MarkDirty();
  static void DrawLayerNearest(FrameBufferLayer *layer,
                               int start_x,
                               int end_x,
                               int start_y,
                               int end_y,
                               int64_t x_step,
                               int64_t y_step);
  static void DrawLayerBilinear(FrameBufferLayer *layer,
                                int start_x,
                                int end_x,
                                int start_y,
                                int end_y,
                                int64_t x_step,
                                int64_t y_step);
  static bool CopyLayerSourceToHwscaleFramebuffer(FrameBufferLayer *layer,
                                                  unsigned buffer_index);
  static bool CanUseKmsDirectScanout(FrameBufferLayer *layer,
                                     int screen_w,
                                     int screen_h);
  static bool BuildKmsLayerPlane(FrameBufferLayer *layer,
                                 unsigned buffer_index,
                                 int screen_w,
                                 int screen_h,
                                 pi5kms::Plane *plane);
  static bool TryKmsDirectScanout(FrameBufferLayer **layers,
                                  unsigned layer_count,
                                  int screen_w,
                                  int screen_h,
                                  bool wait_for_vblank);
  static bool TryV3dPostprocess(FrameBufferLayer **layers,
                                unsigned layer_count,
                                int screen_w,
                                int screen_h,
                                bool wait_for_vblank);
#endif

  // Raw pixel data. Not VC memory.
  uint8_t* pixels_;

#if !BMX_PI4_LEGACY_DISPLAY
  static CBcmFrameBuffer *screen_;
  static uint8_t *screen_pixels_;
  static unsigned screen_pitch_bytes_;
  static unsigned screen_bytes_per_pixel_;
#else
  static DISPMANX_DISPLAY_HANDLE_T dispman_display_;
  static bool egl_initialized_;
  DISPMANX_ELEMENT_HANDLE_T dispman_element_;
  DISPMANX_RESOURCE_HANDLE_T dispman_resource_[2];
#if RASPPI == 4
  DISPMANX_RESOURCE_HANDLE_T pi4_v3d_resource_[2];
  uint8_t *pi4_v3d_allocation_[2];
  uint8_t *pi4_v3d_pixels_[2];
  unsigned pi4_v3d_pitch_;
  unsigned pi4_v3d_width_;
  unsigned pi4_v3d_height_;
  bool pi4_v3d_ready_[2];
  pi4v3d::RenderedFrame pi4_v3d_scanout_[2];
  VC_RECT_T pi4_v3d_copy_dst_rect_;
  VC_RECT_T pi4_v3d_src_rect_;
  uint8_t *pi4_kms_overlay_allocation_[2];
  uint8_t *pi4_kms_overlay_pixels_[2];
  unsigned pi4_kms_overlay_pitch_;
  unsigned pi4_kms_overlay_width_;
  unsigned pi4_kms_overlay_height_;
  unsigned pi4_kms_overlay_front_;
  bool pi4_kms_overlay_front_valid_;
#endif

  static EGLDisplay egl_display_;
  static EGLContext egl_context_;
  EGLConfig egl_config_;
  EGLSurface egl_surface_;
  EGL_DISPMANX_WINDOW_T egl_native_window_;

  VC_RECT_T scale_dst_rect_;
  VC_RECT_T copy_dst_rect_;

  // Defines the region within the frame buffer we are scaling
  VC_RECT_T src_rect_;
  VC_DISPMANX_ALPHA_T alpha_;
#endif

  static bool initialized_;

  int fb_width_;
  int fb_height_;
  int fb_pitch_;
  // Stable canvas identity. layer_ is the mutable display/Z order.
  int logical_layer_;
  int layer_;
  int transparency_;
  double hstretch_;
  double vstretch_;
  int hintstr_;
  int vintstr_;
  int use_hintstr_;
  int use_vintstr_;

  // -1 = top, 0 = center, 1 = bottom
  int valign_;
  int vpadding_;
  // -1 = left, 0 = center, 1 = right
  int halign_;
  int hpadding_;

  int h_center_offset_;
  int v_center_offset_;

  // Represents the resource currently visible
  int rnum_;

  // Used to take away some available display area
  // before deciding what the dest rect coords should be. Expressed
  // as percentage of available area.
  double leftPadding_;
  double rightPadding_;
  double topPadding_;
  double bottomPadding_;

  int display_width_;
  int display_height_;

  int src_x_;
  int src_y_;
  int src_w_;
  int src_h_;

  int dst_x_;
  int dst_y_;
  int dst_w_;
  int dst_h_;

  bool showing_;
  bool allocated_;

#if !BMX_PI4_LEGACY_DISPLAY
  int pixelmode_;
#else
  VC_IMAGE_TYPE_T mode_;
#endif
  int bytes_per_pixel_;

  uint16_t pal_565_[256];
  uint32_t pal_argb_[256];

  bool uses_shader_;

#if !BMX_PI4_LEGACY_DISPLAY
  bool dirty_;
  uint32_t palette_generation_;
  uint32_t content_generation_;
#endif

#if BMX_PI4_LEGACY_DISPLAY
  bool shader_init_;
  GLuint vshader_;
  GLuint fshader_;
  GLuint shader_program_;
  GLuint vbo_;
  GLuint attr_vertex_;
  GLuint attr_texcoord_;
  GLuint texture_sampler_;
  GLuint palette_sampler_;
  GLuint tex_;
  GLuint pal_;

  // Orthographic projection matrix
  GLint mvp_;

  // Shader parameters
  GLuint input_size_;
  GLuint output_size_;
  GLuint texture_size_;
  GLuint texel_size_;

  // Curvature requires the texture to have only
  // the visible pixels in it. We can't get away
  // with texture coords to crop what we want out
  // of the buffer.  The shader could probably be
  // updated to avoid this.  This flag being true
  // causes hundreds of memcpy's to crop the data.
  bool need_cpu_crop_;
  uint8_t* cropped_pixels_;

  // Coordinates. One array is used for both vertex and
  // texture coordinates. 0-7 = vertex, 8-15 = texture
  GLfloat tex_coords_[16];

  // Shader params
  bool curvature_;
  float curvature_x_;
  float curvature_y_;
  float skew_x_;
  float skew_y_;
  float trapezoid_;
  float rotation_degrees_;
  float overscan_scale_;
  bool convergence_;
  float red_offset_x_;
  float red_offset_y_;
  float blue_offset_x_;
  float blue_offset_y_;
  float convergence_radial_strength_;
  bool horizontal_filtering_;
  float horizontal_sigma_x_;
  int mask_;
  float mask_brightness_;
  bool gamma_;
  bool fake_gamma_;
  unsigned output_level_mapping_;
  float output_saturation_;
  float black_level_;
  float white_clip_;
  bool scanlines_;
  bool multisample_;
  float scanline_weight_;
  float scanline_gap_brightness_;
  bool edge_blur_;
  float edge_blur_strength_;
  float edge_blur_radius_;
  bool vignette_;
  float vignette_strength_;
  float vignette_scale_;
  float vignette_softness_;
  bool uneven_illumination_;
  float uneven_illumination_strength_;
  float uneven_illumination_scale_;
  bool glass_reflection_;
  float glass_reflection_angle_;
  float glass_reflection_width_;
  float glass_reflection_position_;
  bool rounded_screen_mask_;
  float rounded_corner_radius_;
  float rounded_border_softness_;
  bool edge_glow_;
  float edge_glow_strength_;
  float edge_glow_width_;
  bool bloom_;
  float bloom_factor_;
  bool horizontal_jitter_;
  float horizontal_jitter_strength_;
  float horizontal_jitter_frequency_;
  float horizontal_jitter_speed_;
  bool composite_artifacts_;
  float composite_chroma_blur_;
  float composite_luma_sharpen_;
  float composite_color_bleed_;
  bool noise_;
  float luminance_noise_;
  float chroma_noise_;
  float noise_speed_;
  float input_gamma_;
  float output_gamma_;
  bool sharper_;
  bool bilinear_interpolation_;
#endif
};

#endif

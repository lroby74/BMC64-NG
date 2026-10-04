#extension GL_OES_standard_derivatives : require

precision mediump float;

#ifndef BMX_LATE_EFFECTS_EDGE_BLUR
#define BMX_LATE_EFFECTS_EDGE_BLUR 1
#endif

#ifndef BMX_LATE_EFFECTS_EARLY
#define BMX_LATE_EFFECTS_EARLY 1
#endif

#include "modules/uniforms_u_texture_through_u_source_texel_y.glsl"
#if BMX_LATE_EFFECTS_EARLY
uniform float u_geometry_enable;
uniform float u_curvature_x;
uniform float u_curvature_y;
uniform float u_skew_x;
uniform float u_skew_y;
uniform float u_trapezoid;
uniform float u_rotation_cosine;
uniform float u_rotation_sine;
uniform float u_overscan_scale;
#endif
#if BMX_LATE_EFFECTS_EDGE_BLUR
uniform float u_edge_blur_enable;
uniform float u_edge_blur_strength;
uniform float u_edge_blur_radius;
#endif
#include "modules/uniforms_u_scanline_weight_through_u_scanline_multisample.glsl"
#if BMX_LATE_EFFECTS_EARLY
#include "modules/uniforms_u_phosphor_mask_enable_through_u_phosphor_mask_brightness.glsl"
#endif

uniform float u_vignette_enable;
uniform float u_vignette_strength;
uniform float u_vignette_scale;
uniform float u_vignette_softness;

uniform float u_uneven_illumination_enable;
uniform float u_uneven_illumination_strength;
uniform float u_precomputed_uneven_sx;
uniform float u_precomputed_uneven_sy;
uniform float u_precomputed_uneven_sx_diagonal;
uniform float u_precomputed_uneven_sy_diagonal;

uniform float u_glass_reflection_enable;
uniform float u_glass_reflection_cosine;
uniform float u_glass_reflection_sine;
uniform float u_glass_reflection_width;
uniform float u_precomputed_glass_position;
uniform float u_precomputed_glass_outer_width;

uniform float u_rounded_screen_mask_enable;
uniform float u_precomputed_rounded_radius;
uniform float u_precomputed_rounded_box;
uniform float u_precomputed_rounded_softness;

uniform float u_edge_glow_enable;
uniform float u_precomputed_edge_glow_width;
uniform float u_precomputed_edge_glow_strength;
#include "modules/uniforms_u_edge_glow_top_r_through_u_edge_glow_right_b.glsl"

varying vec2 v_texcoord;

#include "modules/sample_source.glsl"

#if BMX_LATE_EFFECTS_EARLY
#include "modules/geometry_coordinate_variant2.glsl"

#include "modules/phosphor_mask_variant2.glsl"
#endif

float uneven_illumination_gain(vec2 point) {
  float field = sin(point.x * u_precomputed_uneven_sx + 0.73) * 0.42;
  field += cos(point.y * u_precomputed_uneven_sy + 2.11) * 0.32;
  field += sin((point.x * 0.75 + point.y * 0.55) *
               u_precomputed_uneven_sx_diagonal + 4.37) * 0.20;
  field += cos((point.x * -0.45 + point.y * 0.85) *
               u_precomputed_uneven_sy_diagonal + 5.19) * 0.16;
  return 1.0 + field * u_uneven_illumination_strength;
}

#include "modules/vignette_transmission.glsl"

#include "modules/glass_reflection_variant2.glsl"

float rounded_screen_mask(vec2 point) {
  vec2 q = abs(point) - vec2(u_precomputed_rounded_box);
  float outside = length(max(q, vec2(0.0)));
  float inside = min(max(q.x, q.y), 0.0);
  float distance = outside + inside - u_precomputed_rounded_radius;
  return 1.0 - smoothstep(
      -u_precomputed_rounded_softness,
      u_precomputed_rounded_softness,
      distance);
}

#include "modules/edge_glow_field_variant2.glsl"

#include "modules/bayer2.glsl"

#include "modules/rgb565_dither_threshold.glsl"

#include "modules/dither_for_rgb565.glsl"

#include "modules/source_coverage.glsl"

#include "modules/scanline_scale.glsl"

void main(void) {
  vec2 uv = v_texcoord;
#if BMX_LATE_EFFECTS_EARLY
  if (u_geometry_enable > 0.5) {
    uv = geometry_coordinate(uv);
  }
#endif
  vec2 screen_point = v_texcoord * 2.0 - 1.0;
  vec3 color = sample_source(uv);
#if BMX_LATE_EFFECTS_EDGE_BLUR
  if (u_edge_blur_enable > 0.5) {
    float strength = clamp(u_edge_blur_strength, 0.0, 1.0);
    vec2 source_point = uv * 2.0 - 1.0;
    float edge_distance = 1.0 -
        max(abs(source_point.x), abs(source_point.y));
    float radius = clamp(u_edge_blur_radius, 0.2, 1.0);
    float focus_loss = 1.0 -
        smoothstep(radius * 0.55, radius, edge_distance);
    vec2 texel = vec2(u_source_texel_x, u_source_texel_y);
    vec2 blur_x = vec2(texel.x * (1.0 + strength * 3.0), 0.0);
    vec2 blur_y = vec2(0.0, texel.y * (1.0 + strength * 3.0));
    vec3 blurred = sample_source(uv - blur_x);
    blurred += sample_source(uv + blur_x);
    blurred += sample_source(uv - blur_y);
    blurred += sample_source(uv + blur_y);
    color = mix(color, blurred * 0.25, focus_loss * strength);
  }
#endif
  if (u_scanline_weight > 0.0) {
    color *= source_coverage(uv) * scanline_scale(uv);
#if BMX_LATE_EFFECTS_EARLY
  } else if (u_geometry_enable > 0.5) {
    color *= source_coverage(uv);
#endif
  }
#if BMX_LATE_EFFECTS_EARLY
  if (u_phosphor_mask_enable > 0.5) {
    color *= phosphor_mask();
  }
#endif

  float dither_modulation = 0.0;
  if (u_vignette_enable > 0.5) {
    float transmission = vignette_transmission(screen_point);
    color *= transmission;
    dither_modulation = 1.0 - transmission;
  }
  if (u_uneven_illumination_enable > 0.5) {
    color *= uneven_illumination_gain(screen_point);
    dither_modulation = u_uneven_illumination_strength;
  }
  if (u_glass_reflection_enable > 0.5) {
    float reflection = glass_reflection(screen_point);
    vec3 tint = vec3(0.82, 0.93, 1.0);
    color += (1.0 - color) * reflection * tint * 0.15;
    dither_modulation = max(dither_modulation, 0.15);
  }
  if (u_edge_glow_enable > 0.5) {
    vec4 field = edge_glow_field(v_texcoord,
                                 u_precomputed_edge_glow_width);
    vec3 warm = field.rgb * vec3(1.0, 0.92, 0.82);
    float glow_strength = field.a * u_precomputed_edge_glow_strength;
    color += (1.0 - color) * warm * glow_strength;
    dither_modulation = max(
        dither_modulation, u_precomputed_edge_glow_strength);
  }

  float screen_coverage = 1.0;
  if (u_rounded_screen_mask_enable > 0.5) {
    screen_coverage = rounded_screen_mask(screen_point);
    color *= screen_coverage;
    dither_modulation = max(dither_modulation, 1.0 - screen_coverage);
  }
  if (dither_modulation > 0.0 && screen_coverage > 0.0) {
    color = dither_for_rgb565(color, dither_modulation);
  }
  gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}

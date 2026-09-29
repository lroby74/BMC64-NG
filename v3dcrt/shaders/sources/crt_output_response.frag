#extension GL_OES_standard_derivatives : require

precision mediump float;

uniform sampler2D u_texture;
#include "modules/uniforms_u_source_texel_x_through_u_trapezoid.glsl"
uniform float u_rotation_cosine;
uniform float u_rotation_sine;
uniform float u_overscan_scale;
#include "modules/uniforms_u_edge_blur_enable_through_u_scanline_multisample.glsl"
#include "modules/uniforms_u_phosphor_mask_enable_through_u_uneven_illumination_scale.glsl"
uniform float u_glass_reflection_enable;
uniform float u_glass_reflection_cosine;
uniform float u_glass_reflection_sine;
#include "modules/uniforms_u_glass_reflection_width_through_u_rounded_border_softness.glsl"
uniform float u_edge_glow_enable;
uniform float u_edge_glow_strength;
uniform float u_edge_glow_width;
uniform float u_output_response_enable;
#ifndef BMX_OUTPUT_RESPONSE_FAST_CUBIC
uniform float u_output_response_fast;
uniform float u_input_gamma;
uniform float u_inverse_output_gamma;
#endif
uniform float u_saturation;
uniform float u_black_level;
uniform float u_white_clip;
#ifndef BMX_OUTPUT_RESPONSE_FAST_CUBIC
uniform float u_level_mapping;
#endif
#include "modules/uniforms_u_edge_glow_top_r_through_u_edge_glow_right_b.glsl"

#ifdef BMX_PRECOMPUTED_OUTPUT_CONSTANTS
#ifndef BMX_PRECOMPUTE_LEVELS
#define BMX_PRECOMPUTE_LEVELS 1
#endif
#ifndef BMX_PRECOMPUTE_CLAMPS
#define BMX_PRECOMPUTE_CLAMPS 1
#endif
#ifndef BMX_PRECOMPUTE_EDGE_BLUR
#define BMX_PRECOMPUTE_EDGE_BLUR 1
#endif
#ifndef BMX_PRECOMPUTE_SCANLINES
#define BMX_PRECOMPUTE_SCANLINES 1
#endif
#ifndef BMX_PRECOMPUTE_VIGNETTE
#define BMX_PRECOMPUTE_VIGNETTE 1
#endif
#ifndef BMX_PRECOMPUTE_UNEVEN
#define BMX_PRECOMPUTE_UNEVEN 1
#endif
#ifndef BMX_PRECOMPUTE_GLASS
#define BMX_PRECOMPUTE_GLASS 1
#endif
#ifndef BMX_PRECOMPUTE_ROUNDED
#define BMX_PRECOMPUTE_ROUNDED 1
#endif
#ifndef BMX_PRECOMPUTE_EDGE_GLOW
#define BMX_PRECOMPUTE_EDGE_GLOW 1
#endif
uniform float u_precomputed_black_point;
uniform float u_precomputed_level_scale;
uniform float u_precomputed_edge_blur_offset_scale;
uniform float u_precomputed_edge_blur_inner_radius;
uniform float u_precomputed_scanline_gap;
uniform float u_precomputed_vignette_outer;
uniform float u_precomputed_uneven_sx;
uniform float u_precomputed_uneven_sy;
uniform float u_precomputed_uneven_sx_diagonal;
uniform float u_precomputed_uneven_sy_diagonal;
uniform float u_precomputed_glass_position;
uniform float u_precomputed_glass_outer_width;
uniform float u_precomputed_rounded_radius;
uniform float u_precomputed_rounded_box;
uniform float u_precomputed_rounded_softness;
uniform float u_precomputed_edge_glow_width;
uniform float u_precomputed_edge_glow_strength;
#endif

varying vec2 v_texcoord;

#include "modules/luminance.glsl"

vec3 apply_output_levels(vec3 color) {
  float source_luma = luminance(color);
#if defined(BMX_PRECOMPUTED_OUTPUT_CONSTANTS) && defined(BMX_OUTPUT_RESPONSE_FAST_CUBIC) && BMX_PRECOMPUTE_LEVELS
  float output_luma = clamp(
      (source_luma - u_precomputed_black_point) *
          u_precomputed_level_scale,
      0.0, 1.0);
#else
  float black_level = clamp(u_black_level, 0.0, 1.0);
  float white_clip = clamp(u_white_clip, 0.0, 1.0);
#ifdef BMX_OUTPUT_RESPONSE_FAST_CUBIC
  float mapping = 1.0;
#else
  float mapping = clamp(floor(u_level_mapping + 0.5), 0.0, 2.0);
#endif
  float output_luma;

  if (mapping < 1.5) {
    if (mapping > 0.5) {
      black_level = black_level * black_level * black_level;
      float white_headroom = 1.0 - white_clip;
      white_clip = 1.0 -
          white_headroom * white_headroom * white_headroom;
    }
    float white_point = max(white_clip, black_level + 0.00390625);
    output_luma = clamp(
        (source_luma - black_level) / (white_point - black_level),
        0.0, 1.0);
  } else {
    float luma_squared = source_luma * source_luma;
    float shadow_target = luma_squared * luma_squared;
    float toe_luma = mix(source_luma, shadow_target, black_level);
    float headroom = 1.0 - toe_luma;
    float headroom_squared = headroom * headroom;
    float highlight_target = 1.0 - headroom_squared * headroom_squared;
    output_luma = mix(toe_luma, highlight_target, 1.0 - white_clip);
  }
#endif

  vec3 chroma = (color - vec3(source_luma)) *
#if defined(BMX_PRECOMPUTED_OUTPUT_CONSTANTS) && BMX_PRECOMPUTE_CLAMPS
      u_saturation;
#else
      clamp(u_saturation, 0.0, 1.0);
#endif
  float positive_chroma = max(chroma.r, max(chroma.g, chroma.b));
  float negative_chroma = max(-chroma.r, max(-chroma.g, -chroma.b));
  float chroma_scale = min(
      1.0,
      min((1.0 - output_luma) / max(positive_chroma, 0.000001),
          output_luma / max(negative_chroma, 0.000001)));
  return vec3(output_luma) + chroma * max(chroma_scale, 0.0);
}

#include "modules/sample_source.glsl"

#include "modules/source_coverage.glsl"

#include "modules/geometry_coordinate_variant2.glsl"


#include "modules/edge_glow_field_variant2.glsl"

#include "modules/source_row_beam.glsl"

vec3 phosphor_mask(void) {
#if defined(BMX_PRECOMPUTED_OUTPUT_CONSTANTS) && BMX_PRECOMPUTE_CLAMPS
  float brightness = u_phosphor_mask_brightness;
#else
  float brightness = clamp(u_phosphor_mask_brightness, 0.0, 1.0);
#endif
  float column = floor(gl_FragCoord.x);
  if (u_phosphor_mask_pattern < 1.5) {
    if (mod(column, 2.0) < 1.0) {
      return vec3(brightness, 1.0, brightness);
    }
    return vec3(1.0, brightness, 1.0);
  }

  vec3 mask = vec3(brightness);
  float triad_column = mod(column, 3.0);
  if (triad_column < 1.0) {
    mask.r = 1.0;
  } else if (triad_column < 2.0) {
    mask.g = 1.0;
  } else {
    mask.b = 1.0;
  }
  return mask;
}

float vignette_transmission(vec2 point) {
  vec2 optical = vec2(point.x * 0.75, point.y);
#if defined(BMX_PRECOMPUTED_OUTPUT_CONSTANTS) && BMX_PRECOMPUTE_VIGNETTE
  float edge = smoothstep(
      u_vignette_scale, u_precomputed_vignette_outer, length(optical));
  return 1.0 - u_vignette_strength * edge;
#else
  float scale = clamp(u_vignette_scale, 0.2, 1.0);
  float softness = max(u_vignette_softness, 0.02);
  float edge = smoothstep(scale, scale + softness, length(optical));
  return 1.0 - clamp(u_vignette_strength, 0.0, 1.0) * edge;
#endif
}

float uneven_illumination_gain(vec2 point) {
#if defined(BMX_PRECOMPUTED_OUTPUT_CONSTANTS) && BMX_PRECOMPUTE_UNEVEN
  float field = sin(point.x * u_precomputed_uneven_sx + 0.73) * 0.42;
  field += cos(point.y * u_precomputed_uneven_sy + 2.11) * 0.32;
  field += sin((point.x * 0.75 + point.y * 0.55) *
               u_precomputed_uneven_sx_diagonal + 4.37) * 0.20;
  field += cos((point.x * -0.45 + point.y * 0.85) *
               u_precomputed_uneven_sy_diagonal + 5.19) * 0.16;
  return 1.0 + field * u_uneven_illumination_strength;
#else
  float scale = clamp(u_uneven_illumination_scale, 0.02, 0.25);
  float sx = 1.2 + scale * 8.0;
  float sy = 0.9 + scale * 7.0;
  float field = sin(point.x * sx + 0.73) * 0.42;
  field += cos(point.y * sy + 2.11) * 0.32;
  field += sin((point.x * 0.75 + point.y * 0.55) *
               (sx * 0.8) + 4.37) * 0.20;
  field += cos((point.x * -0.45 + point.y * 0.85) *
               (sy * 0.7) + 5.19) * 0.16;
  return 1.0 + field *
      clamp(u_uneven_illumination_strength, 0.0, 0.35);
#endif
}

float glass_reflection(vec2 point) {
  float normal = point.x * u_glass_reflection_cosine;
  normal += point.y * u_glass_reflection_sine;
  float face = normal + 0.12 * point.x * point.x;
  face -= 0.08 * point.y * point.y;
#if defined(BMX_PRECOMPUTED_OUTPUT_CONSTANTS) && BMX_PRECOMPUTE_GLASS
  face -= u_precomputed_glass_position;
  float stripe = 1.0 - smoothstep(
      u_glass_reflection_width,
      u_precomputed_glass_outer_width,
      abs(face));
#else
  face -= u_glass_reflection_position * 2.0 - 1.0;
  float width = max(u_glass_reflection_width, 0.01);
  float stripe = 1.0 - smoothstep(width, width * 2.5, abs(face));
#endif
  float fresnel = pow(clamp(dot(point, point) * 0.5, 0.0, 1.0), 1.8);
  return clamp(stripe * 0.85 + fresnel * 0.35, 0.0, 1.0);
}

float rounded_screen_mask(vec2 point) {
#if defined(BMX_PRECOMPUTED_OUTPUT_CONSTANTS) && BMX_PRECOMPUTE_ROUNDED
  float radius = u_precomputed_rounded_radius;
  vec2 box = vec2(u_precomputed_rounded_box);
#else
  float radius = clamp(u_rounded_corner_radius, 0.0, 0.45) * 2.0;
  vec2 box = vec2(1.0 - radius);
#endif
  vec2 q = abs(point) - box;
  float outside = length(max(q, vec2(0.0)));
  float inside = min(max(q.x, q.y), 0.0);
  float distance = outside + inside - radius;
#if defined(BMX_PRECOMPUTED_OUTPUT_CONSTANTS) && BMX_PRECOMPUTE_ROUNDED
  float softness = u_precomputed_rounded_softness;
#else
  float softness = max(u_rounded_border_softness * 2.0, 0.0001);
#endif
  return 1.0 - smoothstep(-softness, softness, distance);
}

#include "modules/bayer2.glsl"

#include "modules/rgb565_dither_threshold.glsl"

#include "modules/dither_for_rgb565.glsl"

void main(void) {
  vec2 screen_point = v_texcoord * 2.0 - 1.0;
  vec2 uv = v_texcoord;
  if (u_geometry_enable > 0.5) {
    uv = geometry_coordinate(uv);
  }

  vec3 color = sample_source(uv);
  if (u_edge_blur_enable > 0.5) {
#if defined(BMX_PRECOMPUTED_OUTPUT_CONSTANTS) && BMX_PRECOMPUTE_EDGE_BLUR
    float strength = u_edge_blur_strength;
#else
    float strength = clamp(u_edge_blur_strength, 0.0, 1.0);
#endif
    vec2 source_point = uv * 2.0 - 1.0;
    float edge_distance = 1.0 -
        max(abs(source_point.x), abs(source_point.y));
#if defined(BMX_PRECOMPUTED_OUTPUT_CONSTANTS) && BMX_PRECOMPUTE_EDGE_BLUR
    float focus_loss = 1.0 - smoothstep(
        u_precomputed_edge_blur_inner_radius,
        u_edge_blur_radius,
        edge_distance);
    vec2 texel = vec2(u_source_texel_x, u_source_texel_y);
    vec2 blur_x = vec2(
        texel.x * u_precomputed_edge_blur_offset_scale, 0.0);
    vec2 blur_y = vec2(
        0.0, texel.y * u_precomputed_edge_blur_offset_scale);
#else
    float radius = clamp(u_edge_blur_radius, 0.2, 1.0);
    float focus_loss = 1.0 -
        smoothstep(radius * 0.55, radius, edge_distance);
    vec2 texel = vec2(u_source_texel_x, u_source_texel_y);
    vec2 blur_x = vec2(texel.x * (1.0 + strength * 3.0), 0.0);
    vec2 blur_y = vec2(0.0, texel.y * (1.0 + strength * 3.0));
#endif
    vec3 blurred = sample_source(uv - blur_x);
    blurred += sample_source(uv + blur_x);
    blurred += sample_source(uv - blur_y);
    blurred += sample_source(uv + blur_y);
    color = mix(color, blurred * 0.25, focus_loss * strength);
  }

  float response_enabled = step(0.5, u_output_response_enable);
#ifndef BMX_OUTPUT_RESPONSE_FAST_CUBIC
  float response_fast = step(0.5, u_output_response_fast);
#endif
  float coverage = source_coverage(uv);
  if (response_enabled > 0.5) {
    color *= coverage;
    color = apply_output_levels(color);

#ifdef BMX_OUTPUT_RESPONSE_FAST_CUBIC
    color = color * color;
#else
    vec3 accurate_linear = pow(color, vec3(max(u_input_gamma, 0.1)));
    vec3 fast_linear = color * color;
    color = mix(accurate_linear, fast_linear, response_fast);
#endif
  }

  float beam = source_row_beam(uv);
#if defined(BMX_PRECOMPUTED_OUTPUT_CONSTANTS) && BMX_PRECOMPUTE_SCANLINES
  float gap = u_precomputed_scanline_gap;
#else
  float scanline_strength = clamp(u_scanline_weight, 0.0, 1.0);
  float gap = mix(1.0, clamp(u_gap_brightness, 0.0, 1.0),
                  scanline_strength);
#endif
  float scanline = mix(gap, 1.0, beam);
  if (response_enabled > 0.5) {
    color *= scanline;
#ifdef BMX_OUTPUT_RESPONSE_FAST_CUBIC
    color = sqrt(clamp(color, 0.0, 1.0));
#else
    vec3 accurate_output = pow(clamp(color, 0.0, 1.0),
                               vec3(max(u_inverse_output_gamma, 0.1)));
    vec3 fast_output = sqrt(clamp(color, 0.0, 1.0));
    color = mix(accurate_output, fast_output, response_fast);
#endif
  } else {
    color *= coverage * scanline;
  }
  if (u_phosphor_mask_enable > 0.5) {
    color *= phosphor_mask();
  }

  float continuous_modulation = 0.0;
  if (u_vignette_enable > 0.5) {
    float transmission = vignette_transmission(screen_point);
    color *= transmission;
    continuous_modulation = 1.0 - transmission;
  }
  if (u_uneven_illumination_enable > 0.5) {
    float illumination = uneven_illumination_gain(screen_point);
    color *= illumination;
    continuous_modulation = max(continuous_modulation,
                                abs(illumination - 1.0));
  }
  if (u_glass_reflection_enable > 0.5) {
    float reflection = glass_reflection(screen_point);
    vec3 tint = vec3(0.82, 0.93, 1.0);
    color += (1.0 - color) * reflection * tint * 0.15;
    continuous_modulation = max(continuous_modulation, 0.15);
  }
  if (u_edge_glow_enable > 0.5) {
#if defined(BMX_PRECOMPUTED_OUTPUT_CONSTANTS) && BMX_PRECOMPUTE_EDGE_GLOW
    vec4 field = edge_glow_field(
        v_texcoord, u_precomputed_edge_glow_width);
    vec3 warm = field.rgb * vec3(1.0, 0.92, 0.82);
    float glow_strength = field.a * u_precomputed_edge_glow_strength;
#else
    float width_control = clamp(
        (u_edge_glow_width - 0.01) / 0.34, 0.0, 1.0);
    float width = 0.01 + sqrt(width_control) * 0.34;
    vec4 field = edge_glow_field(v_texcoord, width);
    vec3 warm = field.rgb * vec3(1.0, 0.92, 0.82);
    float strength_control = clamp(
        u_edge_glow_strength / 0.35, 0.0, 1.0);
    float glow_strength = field.a *
        sqrt(strength_control) * 1.2;
#endif
    vec3 glow = warm * glow_strength;
    color += (1.0 - color) * glow;
    continuous_modulation = max(continuous_modulation,
                                sqrt(strength_control) * 1.2);
  }

  float screen_coverage = 1.0;
  if (u_rounded_screen_mask_enable > 0.5) {
    screen_coverage = rounded_screen_mask(screen_point);
    color *= screen_coverage;
    continuous_modulation = max(continuous_modulation,
                                1.0 - screen_coverage);
  }
  if (continuous_modulation > 0.0 && screen_coverage > 0.0) {
    color = dither_for_rgb565(color, continuous_modulation);
  }
  gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}

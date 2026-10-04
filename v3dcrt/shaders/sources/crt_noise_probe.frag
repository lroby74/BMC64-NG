#extension GL_OES_standard_derivatives : require

#ifdef GL_ES
precision highp float;
#endif

#include "modules/uniforms_u_texture_through_u_rounded_border_softness.glsl"
#include "modules/uniforms_u_phosphor_mask_enable_through_u_uneven_illumination_scale.glsl"
#include "modules/uniforms_u_horizontal_jitter_enable_through_u_horizontal_jitter_frequency.glsl"
#include "modules/uniforms_u_composite_artifacts_enable_through_u_composite_color_bleed.glsl"
#include "modules/uniforms_u_noise_enable_through_u_temporal_frame.glsl"
uniform float u_level_mapping;

varying vec2 v_texcoord;

#include "modules/luminance.glsl"

#include "modules/apply_output_levels.glsl"

#include "modules/noise_hash.glsl"

#include "modules/sample_source.glsl"

#include "modules/source_coverage.glsl"

#include "modules/geometry_coordinate.glsl"


#include "modules/edge_glow_sample.glsl"

#include "modules/edge_glow_field.glsl"

#include "modules/rounded_screen_mask.glsl"

void main(void) {
  vec2 screen_point = v_texcoord * 2.0 - 1.0;
  vec2 uv = v_texcoord;

  if (u_horizontal_jitter_enable > 0.5) {
    float line = gl_FragCoord.y;
    float frequency = max(u_horizontal_jitter_frequency, 0.001);
    float phase = u_temporal_frame *
        clamp(u_horizontal_jitter_speed, 0.0, 1.0) * 0.6135923;
    float wave = sin(line * frequency + phase) * 0.68;
    wave += sin(line * frequency * 2.7 + 1.7 + phase * 2.0) * 0.32;
    uv.x -= wave * u_horizontal_jitter_strength * u_source_texel_x;
  }

  if (u_geometry_enable > 0.5) {
    uv = geometry_coordinate(uv);
  }

  float coverage = source_coverage(uv);
  vec2 texel_x = vec2(u_source_texel_x, 0.0);
  vec2 texel_y = vec2(0.0, u_source_texel_y);
  vec3 center = sample_source(uv);
  vec3 left = sample_source(uv - texel_x);
  vec3 right = sample_source(uv + texel_x);
  vec3 color = center;

  if (u_horizontal_filter_enable > 0.5) {
    float side_weight = clamp(u_horizontal_sigma_x, 0.0, 1.0) * 0.25;
    color = center * (1.0 - side_weight * 2.0);
    color += (left + right) * side_weight;
  }

  if (u_convergence_enable > 0.5) {
    float radius2 = dot(screen_point, screen_point);
    float radial = max(u_convergence_radial_strength, 0.0);
    float field = 1.0 + radial * (0.65 * radius2 + 0.35 * radius2 * radius2);
    vec2 bow = vec2(screen_point.x * screen_point.y * 0.25,
                    (screen_point.x * screen_point.x -
                     screen_point.y * screen_point.y) * 0.12) * radial;
    vec2 red_offset = vec2(u_red_offset_x * u_source_texel_x,
                           u_red_offset_y * u_source_texel_y) * field;
    vec2 blue_offset = vec2(u_blue_offset_x * u_source_texel_x,
                            u_blue_offset_y * u_source_texel_y) * field;
    red_offset += bow * vec2(u_source_texel_x, u_source_texel_y);
    blue_offset -= bow * vec2(u_source_texel_x, u_source_texel_y);
    color.r = sample_source(uv - red_offset).r;
    color.b = sample_source(uv - blue_offset).b;
  }

  if (u_edge_blur_enable > 0.5) {
    float strength = clamp(u_edge_blur_strength, 0.0, 1.0);
    float edge_distance = 1.0 - max(abs(screen_point.x), abs(screen_point.y));
    float radius = clamp(u_edge_blur_radius, 0.2, 1.0);
    float focus_loss = 1.0 - smoothstep(radius * 0.55, radius, edge_distance);
    vec2 blur_x = texel_x * (1.0 + strength * 3.0);
    vec2 blur_y = texel_y * (1.0 + strength * 3.0);
    vec3 edge_blur = sample_source(uv - blur_x);
    edge_blur += sample_source(uv + blur_x);
    edge_blur += sample_source(uv - blur_y);
    edge_blur += sample_source(uv + blur_y);
    edge_blur *= 0.25;
    color = mix(color, edge_blur, focus_loss * strength);
  }

  if (u_composite_artifacts_enable > 0.5) {
    float center_luma = luminance(color);
    float left_luma = luminance(left);
    float right_luma = luminance(right);
    vec3 horizontal_blur = (left + center * 2.0 + right) * 0.25;
    float blur_luma = luminance(horizontal_blur);
    vec3 center_chroma = color - vec3(center_luma);
    vec3 blur_chroma = horizontal_blur - vec3(blur_luma);
    float chroma_mix = clamp(u_composite_chroma_blur * 0.5, 0.0, 1.0);
    float sharpened_luma = center_luma;
    sharpened_luma +=
        (center_luma - (left_luma + center_luma * 2.0 + right_luma) * 0.25) *
        clamp(u_composite_luma_sharpen, 0.0, 1.0) * 2.2;
    vec3 bleed_chroma = right - vec3(right_luma);
    vec3 chroma = mix(center_chroma, blur_chroma, chroma_mix);
    chroma = mix(chroma, bleed_chroma,
                 clamp(u_composite_color_bleed, 0.0, 1.0));
    color = clamp(vec3(sharpened_luma) + chroma, 0.0, 1.0);
  }

  color *= coverage;
  float response_enabled = step(0.5, u_output_response_enable);
  float response_fast = step(0.5, u_output_response_fast);
  if (response_enabled > 0.5) {
    color = apply_output_levels(color);
    vec3 accurate_linear = pow(color, vec3(max(u_input_gamma, 0.1)));
    vec3 fast_linear = color * color;
    color = mix(accurate_linear, fast_linear, response_fast);
  }

  float row = floor(gl_FragCoord.y);
  float dark_row = mod(row, 2.0);
  float multisample = step(0.5, u_scanline_multisample);
  dark_row *= mix(1.125, 1.0, multisample);
  float scanline_strength = clamp(u_scanline_weight, 0.0, 1.0);
  float gap = mix(1.0, clamp(u_gap_brightness, 0.0, 1.0),
                  scanline_strength);
  color *= mix(1.0, gap, dark_row);

  if (response_enabled > 0.5) {
    vec3 accurate_output = pow(clamp(color, 0.0, 1.0),
                               vec3(max(u_inverse_output_gamma, 0.1)));
    vec3 fast_output = sqrt(clamp(color, 0.0, 1.0));
    color = mix(accurate_output, fast_output, response_fast);
  }

  if (u_phosphor_mask_enable > 0.5) {
    float brightness = clamp(u_phosphor_mask_brightness, 0.0, 1.0);
    float column = floor(gl_FragCoord.x);
    vec3 mask;
    if (u_phosphor_mask_pattern < 1.5) {
      if (mod(column, 2.0) < 1.0) {
        mask = vec3(brightness, 1.0, brightness);
      } else {
        mask = vec3(1.0, brightness, 1.0);
      }
    } else {
      mask = vec3(brightness);
      float triad_column = mod(column, 3.0);
      if (triad_column < 1.0) {
        mask.r = 1.0;
      } else if (triad_column < 2.0) {
        mask.g = 1.0;
      } else {
        mask.b = 1.0;
      }
    }
    color *= mask;
  }

  if (u_vignette_enable > 0.5) {
    vec2 optical = vec2(screen_point.x * 0.75, screen_point.y);
    float edge = smoothstep(
        u_vignette_scale,
        u_vignette_scale + max(u_vignette_softness, 0.0001),
        length(optical));
    float transmission =
        1.0 - clamp(u_vignette_strength, 0.0, 1.0) * edge;
    color *= transmission;
  }

  if (u_uneven_illumination_enable > 0.5) {
    float scale = clamp(u_uneven_illumination_scale, 0.02, 0.25);
    float sx = 1.2 + scale * 8.0;
    float sy = 0.9 + scale * 7.0;
    float field = sin(screen_point.x * sx + 0.73) * 0.42;
    field += cos(screen_point.y * sy + 2.11) * 0.32;
    field += sin((screen_point.x * 0.75 + screen_point.y * 0.55) *
                 (sx * 0.8) + 4.37) * 0.20;
    field += cos((screen_point.x * -0.45 + screen_point.y * 0.85) *
                 (sy * 0.7) + 5.19) * 0.16;
    color *= 1.0 + field *
        clamp(u_uneven_illumination_strength, 0.0, 0.35);
  }

  if (u_noise_enable > 0.5) {
    float noise_frame = floor(
        u_temporal_frame * clamp(u_noise_speed, 0.0, 1.0));
    float noise_normalization = 3.4641016;
    float luma_noise =
        (noise_hash(gl_FragCoord.xy, noise_frame) - 0.5) *
        noise_normalization;
    float row_noise =
        (noise_hash(vec2(gl_FragCoord.y, 17.0), noise_frame + 109.0) - 0.5) *
        noise_normalization;
    float signal_gain =
        0.35 + 0.75 * sqrt(clamp(luminance(color), 0.0, 1.0));
    color += vec3((luma_noise + row_noise * 0.55) * signal_gain *
                  u_luminance_noise);
    float chroma_i =
        (noise_hash(gl_FragCoord.xy, noise_frame + 41.0) - 0.5) *
        noise_normalization;
    float chroma_q =
        (noise_hash(gl_FragCoord.xy, noise_frame + 71.0) - 0.5) *
        noise_normalization;
    color += vec3(chroma_i * 0.956 + chroma_q * 0.621,
                  chroma_i * -0.272 + chroma_q * -0.6474,
                  chroma_i * -1.106 + chroma_q * 1.7046) * u_chroma_noise;
  }

  if (u_glass_reflection_enable > 0.5) {
    float normal = screen_point.x * cos(u_glass_reflection_angle);
    normal += screen_point.y * sin(u_glass_reflection_angle);
    float face = normal + 0.12 * screen_point.x * screen_point.x;
    face -= 0.08 * screen_point.y * screen_point.y;
    face -= u_glass_reflection_position * 2.0 - 1.0;
    float width = max(u_glass_reflection_width, 0.01);
    float stripe = 1.0 - smoothstep(width, width * 2.5, abs(face));
    float fresnel = pow(clamp(dot(screen_point, screen_point) * 0.5,
                                0.0, 1.0), 1.8);
    float reflection = clamp(stripe * 0.85 + fresnel * 0.35, 0.0, 1.0);
    vec3 tint = vec3(0.82, 0.93, 1.0);
    color += (1.0 - color) * reflection * tint * 0.15;
  }

  if (u_edge_glow_enable > 0.5) {
    float width = max(u_edge_glow_width, 0.01);
    vec4 field = edge_glow_field(v_texcoord, width);
    vec3 warm = field.rgb * vec3(1.0, 0.86, 0.68);
    color += (1.0 - color) * warm * field.a *
             clamp(u_edge_glow_strength, 0.0, 0.35) * 1.8;
  }

  if (u_rounded_screen_mask_enable > 0.5) {
    color *= rounded_screen_mask(screen_point);
  }

  gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}

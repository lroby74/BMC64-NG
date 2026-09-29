precision mediump float;

#include "modules/uniforms_u_texture_through_u_source_texel_y.glsl"
uniform float u_horizontal_filter_enable;
uniform float u_horizontal_sigma_x;
#include "modules/uniforms_u_convergence_enable_through_u_convergence_radial_strength.glsl"
#include "modules/uniforms_u_composite_artifacts_enable_through_u_composite_color_bleed.glsl"
#include "modules/uniforms_u_horizontal_jitter_enable_through_u_horizontal_jitter_frequency.glsl"

varying vec2 v_texcoord;

#include "modules/sample_source.glsl"

#include "modules/luminance_variant2.glsl"

void main(void) {
  vec2 sample_uv = v_texcoord;
  if (u_horizontal_jitter_enable > 0.5) {
    float source_row = floor(
        v_texcoord.y / max(u_source_texel_y, 0.000001));
    float frequency = max(u_horizontal_jitter_frequency, 0.001);
    float wave = sin(source_row * frequency) * 0.68;
    wave += sin(source_row * frequency * 2.7 + 1.7) * 0.32;
    sample_uv.x -= wave * max(u_horizontal_jitter_strength, 0.0) *
        u_source_texel_x;
  }

  vec2 texel_x = vec2(u_source_texel_x, 0.0);
  vec3 center = sample_source(sample_uv);
  vec3 left = sample_source(sample_uv - texel_x);
  vec3 right = sample_source(sample_uv + texel_x);
  vec3 color = center;

  if (u_horizontal_filter_enable > 0.5) {
    float side_weight = clamp(u_horizontal_sigma_x, 0.0, 1.0) * 0.25;
    color = center * (1.0 - side_weight * 2.0);
    color += (left + right) * side_weight;
  }

  if (u_convergence_enable > 0.5) {
    vec2 screen_point = v_texcoord * 2.0 - 1.0;
    float radius2 = dot(screen_point, screen_point);
    float radial = max(u_convergence_radial_strength, 0.0);
    float field = 1.0 + radial * (0.65 * radius2 + 0.35 * radius2 * radius2);
    vec2 bow = vec2(screen_point.x * screen_point.y * 0.25,
                    (screen_point.x * screen_point.x -
                     screen_point.y * screen_point.y) * 0.12) * radial;
    vec2 source_texel = vec2(u_source_texel_x, u_source_texel_y);
    vec2 red_offset = vec2(u_red_offset_x, u_red_offset_y) *
        source_texel * field;
    vec2 blue_offset = vec2(u_blue_offset_x, u_blue_offset_y) *
        source_texel * field;
    red_offset += bow * source_texel;
    blue_offset -= bow * source_texel;
    color.r = sample_source(sample_uv - red_offset).r;
    color.b = sample_source(sample_uv - blue_offset).b;
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
    color = vec3(sharpened_luma) + chroma;
  }

  gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}

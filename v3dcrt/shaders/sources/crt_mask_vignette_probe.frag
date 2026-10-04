#extension GL_OES_standard_derivatives : require

precision mediump float;

#include "modules/uniforms_u_texture_through_u_rounded_border_softness.glsl"
#include "modules/uniforms_u_phosphor_mask_enable_through_u_phosphor_mask_brightness.glsl"
uniform float u_vignette_enable;
uniform float u_vignette_strength;
uniform float u_vignette_scale;
uniform float u_vignette_softness;

varying vec2 v_texcoord;

#include "modules/luminance.glsl"

#include "modules/sample_source.glsl"

#include "modules/source_coverage.glsl"

#include "modules/geometry_coordinate.glsl"


#include "modules/edge_glow_sample.glsl"

#include "modules/edge_glow_field.glsl"

#include "modules/rounded_screen_mask.glsl"

void main(void) {
  vec2 screen_point = v_texcoord * 2.0 - 1.0;
  vec2 uv = v_texcoord;
  if (u_geometry_enable > 0.5) {
    uv = geometry_coordinate(uv);
  }

  float coverage = source_coverage(uv);
  vec2 texel_x = vec2(u_source_texel_x, 0.0);
  vec2 texel_y = vec2(0.0, u_source_texel_y);
  vec3 center = sample_source(uv);
  vec3 color = center;

  if (u_horizontal_filter_enable > 0.5) {
    vec3 left = sample_source(uv - texel_x);
    vec3 right = sample_source(uv + texel_x);
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

  color *= coverage;
  float response_enabled = step(0.5, u_output_response_enable);
  float response_fast = step(0.5, u_output_response_fast);
  if (response_enabled > 0.5) {
    float black_level = clamp(u_black_level, 0.0, 1.0);
    float white_clip = clamp(u_white_clip, 0.0, 1.0);
    float level_range = max(white_clip - black_level, 0.00390625);
    color = clamp((color - vec3(black_level)) / level_range, 0.0, 1.0);
    float luma = luminance(color);
    color = clamp(mix(vec3(luma), color, clamp(u_saturation, 0.0, 2.0)),
                  0.0, 1.0);
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

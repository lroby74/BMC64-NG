#extension GL_OES_standard_derivatives : require

precision mediump float;

#include "modules/uniforms_u_texture_through_u_scanline_multisample.glsl"

varying vec2 v_texcoord;

#include "modules/sample_source.glsl"

#include "modules/source_coverage.glsl"

#include "modules/geometry_coordinate.glsl"

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
  float row = floor(gl_FragCoord.y);
  float dark_row = mod(row, 2.0);
  float multisample = step(0.5, u_scanline_multisample);
  dark_row *= mix(1.125, 1.0, multisample);
  float scanline_strength = clamp(u_scanline_weight, 0.0, 1.0);
  float gap = mix(1.0, clamp(u_gap_brightness, 0.0, 1.0),
                  scanline_strength);
  color *= mix(1.0, gap, dark_row);

  gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}

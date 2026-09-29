precision mediump float;

#include "modules/uniforms_u_texture_through_u_source_texel_y.glsl"
uniform float u_horizontal_filter_enable;
uniform float u_horizontal_sigma_x;
#include "modules/uniforms_u_convergence_enable_through_u_convergence_radial_strength.glsl"

varying vec2 v_texcoord;

#include "modules/sample_source.glsl"

void main(void) {
  vec2 texel_x = vec2(u_source_texel_x, 0.0);
  vec3 center = sample_source(v_texcoord);
  vec3 left = sample_source(v_texcoord - texel_x);
  vec3 right = sample_source(v_texcoord + texel_x);
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
    color.r = sample_source(v_texcoord - red_offset).r;
    color.b = sample_source(v_texcoord - blue_offset).b;
  }

  gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}

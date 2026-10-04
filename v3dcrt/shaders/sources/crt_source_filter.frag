precision mediump float;

#include "modules/uniforms_u_texture_through_u_source_texel_y.glsl"
uniform float u_horizontal_filter_enable;
uniform float u_horizontal_sigma_x;

varying vec2 v_texcoord;

#include "modules/sample_source.glsl"

void main(void) {
  vec3 center = sample_source(v_texcoord);
  vec3 color = center;
  if (u_horizontal_filter_enable > 0.5) {
    vec2 texel_x = vec2(u_source_texel_x, 0.0);
    vec3 left = sample_source(v_texcoord - texel_x);
    vec3 right = sample_source(v_texcoord + texel_x);
    float side_weight = clamp(u_horizontal_sigma_x, 0.0, 1.0) * 0.25;
    color = center * (1.0 - side_weight * 2.0);
    color += (left + right) * side_weight;
  }
  gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}

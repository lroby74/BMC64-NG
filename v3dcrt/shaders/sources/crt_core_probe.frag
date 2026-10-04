#extension GL_OES_standard_derivatives : require

precision mediump float;

uniform sampler2D u_texture;
uniform vec2 u_output_size;
#include "modules/uniforms_u_source_texel_x_through_u_overscan_scale.glsl"
uniform float u_horizontal_filter_enable;
uniform float u_horizontal_sigma_x;
#include "modules/uniforms_u_scanline_weight_through_u_scanline_multisample.glsl"

varying vec2 v_texcoord;

#include "modules/sample_source.glsl"

#include "modules/source_coverage.glsl"

#include "modules/geometry_coordinate.glsl"

void main(void) {
  vec2 uv = v_texcoord;
  if (u_geometry_enable > 0.5) {
    uv = geometry_coordinate(uv);
  }

  float coverage = source_coverage(uv);
  vec3 center = sample_source(uv);
  vec3 color = center;

  if (u_horizontal_filter_enable > 0.5) {
    vec2 texel_x = vec2(u_source_texel_x, 0.0);
    vec3 left = sample_source(uv - texel_x);
    vec3 right = sample_source(uv + texel_x);
    float side_weight = clamp(u_horizontal_sigma_x, 0.0, 1.0) * 0.25;
    color = center * (1.0 - side_weight * 2.0);
    color += (left + right) * side_weight;
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

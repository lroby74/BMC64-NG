#extension GL_OES_standard_derivatives : require

precision mediump float;

#include "modules/uniforms_u_texture_through_u_source_texel_y.glsl"
#include "modules/uniforms_u_scanline_weight_through_u_scanline_multisample.glsl"

varying vec2 v_texcoord;

#include "modules/sample_source.glsl"

#include "modules/source_coverage.glsl"

#include "modules/source_row_beam.glsl"

void main(void) {
  vec2 uv = v_texcoord;
  vec3 color = sample_source(uv) * source_coverage(uv);
  float strength = clamp(u_scanline_weight, 0.0, 1.0);
  float gap = mix(1.0, clamp(u_gap_brightness, 0.0, 1.0), strength);
  float scale = mix(gap, 1.0, source_row_beam(uv));
  gl_FragColor = vec4(color * scale, 1.0);
}

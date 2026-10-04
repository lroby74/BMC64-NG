#extension GL_OES_standard_derivatives : require

precision mediump float;

uniform sampler2D u_texture;
#include "modules/uniforms_u_source_texel_x_through_u_overscan_scale.glsl"
#include "modules/uniforms_u_scanline_weight_through_u_scanline_multisample.glsl"

varying vec2 v_texcoord;

#include "modules/sample_source.glsl"

#include "modules/source_coverage.glsl"

#include "modules/geometry_coordinate.glsl"

#include "modules/source_row_beam.glsl"

void main(void) {
  vec2 uv = v_texcoord;
  if (u_geometry_enable > 0.5) {
    uv = geometry_coordinate(uv);
  }

  float beam = source_row_beam(uv);
  float scanline_strength = clamp(u_scanline_weight, 0.0, 1.0);
  float gap = mix(1.0, clamp(u_gap_brightness, 0.0, 1.0),
                  scanline_strength);
  float scanline = mix(gap, 1.0, beam);
  vec3 color = sample_source(uv) * source_coverage(uv) * scanline;
  gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}

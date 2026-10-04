#extension GL_OES_standard_derivatives : require

precision mediump float;

uniform sampler2D u_texture;
#include "modules/uniforms_u_source_texel_x_through_u_overscan_scale.glsl"

varying vec2 v_texcoord;

#include "modules/sample_source.glsl"

#include "modules/source_coverage.glsl"

#include "modules/geometry_coordinate.glsl"

void main(void) {
  vec2 uv = v_texcoord;
  if (u_geometry_enable > 0.5) {
    uv = geometry_coordinate(uv);
  }
  vec3 color = sample_source(uv) * source_coverage(uv);
  gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}

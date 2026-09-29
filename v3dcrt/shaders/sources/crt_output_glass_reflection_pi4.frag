#extension GL_OES_standard_derivatives : require

precision mediump float;

uniform sampler2D u_texture;
#include "modules/uniforms_u_source_texel_x_through_u_trapezoid.glsl"
uniform float u_rotation_cosine;
uniform float u_rotation_sine;
uniform float u_overscan_scale;
#include "modules/uniforms_u_edge_blur_enable_through_u_scanline_multisample.glsl"
#include "modules/uniforms_u_phosphor_mask_enable_through_u_phosphor_mask_brightness.glsl"
uniform float u_glass_reflection_cosine;
uniform float u_glass_reflection_sine;
uniform float u_glass_reflection_width;
uniform float u_precomputed_glass_position;
uniform float u_precomputed_glass_outer_width;

varying vec2 v_texcoord;

#include "modules/sample_source.glsl"

#include "modules/geometry_coordinate_variant2.glsl"

#include "modules/phosphor_mask_variant2.glsl"

#include "modules/glass_reflection_variant2.glsl"

#include "modules/bayer2.glsl"

#include "modules/rgb565_dither_threshold.glsl"

#include "modules/dither_for_rgb565.glsl"

#include "modules/source_coverage.glsl"

#include "modules/scanline_scale.glsl"

void main(void) {
  vec2 uv = v_texcoord;
  if (u_geometry_enable > 0.5) {
    uv = geometry_coordinate(uv);
  }
  vec3 color = sample_source(uv);
  if (u_edge_blur_enable > 0.5) {
    float strength = clamp(u_edge_blur_strength, 0.0, 1.0);
    vec2 source_point = uv * 2.0 - 1.0;
    float edge_distance = 1.0 -
        max(abs(source_point.x), abs(source_point.y));
    float radius = clamp(u_edge_blur_radius, 0.2, 1.0);
    float focus_loss = 1.0 -
        smoothstep(radius * 0.55, radius, edge_distance);
    vec2 texel = vec2(u_source_texel_x, u_source_texel_y);
    vec2 blur_x = vec2(texel.x * (1.0 + strength * 3.0), 0.0);
    vec2 blur_y = vec2(0.0, texel.y * (1.0 + strength * 3.0));
    vec3 blurred = sample_source(uv - blur_x);
    blurred += sample_source(uv + blur_x);
    blurred += sample_source(uv - blur_y);
    blurred += sample_source(uv + blur_y);
    color = mix(color, blurred * 0.25, focus_loss * strength);
  }
  if (u_scanline_weight > 0.0) {
    color *= source_coverage(uv) * scanline_scale(uv);
  } else if (u_geometry_enable > 0.5) {
    color *= source_coverage(uv);
  }
  if (u_phosphor_mask_enable > 0.5) {
    color *= phosphor_mask();
  }
  float reflection = glass_reflection(v_texcoord * 2.0 - 1.0);
  vec3 tint = vec3(0.82, 0.93, 1.0);
  color += (1.0 - color) * reflection * tint * 0.15;
  color = dither_for_rgb565(color, 0.15);
  gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}

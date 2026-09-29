#extension GL_OES_standard_derivatives : require

precision mediump float;

uniform sampler2D u_texture;
#include "modules/uniforms_u_source_texel_x_through_u_overscan_scale.glsl"
#include "modules/uniforms_u_edge_blur_enable_through_u_scanline_multisample.glsl"
#include "modules/uniforms_u_phosphor_mask_enable_through_u_phosphor_mask_brightness.glsl"
uniform float u_vignette_enable;
uniform float u_vignette_strength;
uniform float u_vignette_scale;
uniform float u_vignette_softness;

varying vec2 v_texcoord;

#include "modules/sample_source.glsl"

#include "modules/source_coverage.glsl"

#include "modules/geometry_coordinate.glsl"

#include "modules/source_row_beam.glsl"

#include "modules/phosphor_mask.glsl"

#include "modules/vignette_transmission.glsl"

#include "modules/bayer2.glsl"

#include "modules/rgb565_dither_threshold.glsl"

vec3 dither_vignette_for_rgb565(vec3 color, float attenuation) {
  float amount = smoothstep(0.0, 1.0 / 64.0, attenuation);
  vec3 quantum = vec3(1.0 / 31.0, 1.0 / 63.0, 1.0 / 31.0);
  return color + rgb565_dither_threshold(gl_FragCoord.xy) * quantum * amount;
}

void main(void) {
  vec2 screen_point = v_texcoord * 2.0 - 1.0;
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

  float beam = source_row_beam(uv);
  float scanline_strength = clamp(u_scanline_weight, 0.0, 1.0);
  float gap = mix(1.0, clamp(u_gap_brightness, 0.0, 1.0),
                  scanline_strength);
  float scanline = mix(gap, 1.0, beam);
  color *= source_coverage(uv) * scanline;
  if (u_phosphor_mask_enable > 0.5) {
    color *= phosphor_mask();
  }
  if (u_vignette_enable > 0.5) {
    float transmission = vignette_transmission(screen_point);
    color *= transmission;
    color = dither_vignette_for_rgb565(color, 1.0 - transmission);
  }
  gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}

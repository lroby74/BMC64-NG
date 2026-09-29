#extension GL_OES_standard_derivatives : require

precision mediump float;

uniform sampler2D u_texture;
#include "modules/uniforms_u_source_texel_x_through_u_overscan_scale.glsl"
#include "modules/uniforms_u_edge_blur_enable_through_u_scanline_multisample.glsl"
#include "modules/uniforms_u_phosphor_mask_enable_through_u_uneven_illumination_scale.glsl"
uniform float u_glass_reflection_enable;
uniform float u_glass_reflection_angle;
#include "modules/uniforms_u_glass_reflection_width_through_u_rounded_border_softness.glsl"
uniform float u_edge_glow_enable;
uniform float u_edge_glow_strength;
uniform float u_edge_glow_width;
#include "modules/uniforms_u_edge_glow_top_r_through_u_edge_glow_right_b.glsl"

varying vec2 v_texcoord;

#include "modules/luminance.glsl"

#include "modules/sample_source.glsl"

#include "modules/source_coverage.glsl"

#include "modules/geometry_coordinate.glsl"


#include "modules/edge_glow_field_variant2.glsl"

#include "modules/source_row_beam.glsl"

#include "modules/phosphor_mask.glsl"

#include "modules/vignette_transmission.glsl"

#include "modules/uneven_illumination_gain.glsl"

#include "modules/glass_reflection.glsl"

#include "modules/rounded_screen_mask.glsl"

#include "modules/bayer2.glsl"

#include "modules/rgb565_dither_threshold.glsl"

#include "modules/dither_for_rgb565.glsl"

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

  float continuous_modulation = 0.0;
  if (u_vignette_enable > 0.5) {
    float transmission = vignette_transmission(screen_point);
    color *= transmission;
    continuous_modulation = 1.0 - transmission;
  }
  if (u_uneven_illumination_enable > 0.5) {
    float illumination = uneven_illumination_gain(screen_point);
    color *= illumination;
    continuous_modulation = max(continuous_modulation,
                                abs(illumination - 1.0));
  }
  if (u_glass_reflection_enable > 0.5) {
    float reflection = glass_reflection(screen_point);
    vec3 tint = vec3(0.82, 0.93, 1.0);
    color += (1.0 - color) * reflection * tint * 0.15;
    continuous_modulation = max(continuous_modulation, 0.15);
  }
  if (u_edge_glow_enable > 0.5) {
    float width_control = clamp(
        (u_edge_glow_width - 0.01) / 0.34, 0.0, 1.0);
    float width = 0.01 + sqrt(width_control) * 0.34;
    vec4 field = edge_glow_field(v_texcoord, width);
    vec3 warm = field.rgb * vec3(1.0, 0.92, 0.82);
    float strength_control = clamp(
        u_edge_glow_strength / 0.35, 0.0, 1.0);
    float glow_strength = field.a *
        sqrt(strength_control) * 1.2;
    vec3 glow = warm * glow_strength;
    color += (1.0 - color) * glow;
    // Match the Pi4 path: the dither amplitude follows the configured glow
    // strength, not the already faded local glow sample. Otherwise the outer
    // part of the soft edge quantizes into visible bands.
    continuous_modulation = max(continuous_modulation,
                                sqrt(strength_control) * 1.2);
  }

  float screen_coverage = 1.0;
  if (u_rounded_screen_mask_enable > 0.5) {
    screen_coverage = rounded_screen_mask(screen_point);
    color *= screen_coverage;
    continuous_modulation = max(continuous_modulation,
                                1.0 - screen_coverage);
  }
  if (continuous_modulation > 0.0 && screen_coverage > 0.0) {
    color = dither_for_rgb565(color, continuous_modulation);
  }
  gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}

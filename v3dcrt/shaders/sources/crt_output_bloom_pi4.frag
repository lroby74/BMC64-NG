#ifdef GL_ES
precision highp float;
#endif

#include "modules/uniforms_u_texture_through_u_source_texel_y.glsl"
uniform float u_bloom_enable;
uniform float u_bloom_factor;

varying vec2 v_texcoord;

const float kBloomThresholdLow = 0.20;
const float kBloomThresholdHigh = 0.65;

#include "modules/sample_source.glsl"

vec3 bright_sample(vec2 uv) {
  vec3 color = sample_source(uv);
  float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
  float contribution = smoothstep(
      kBloomThresholdLow, kBloomThresholdHigh, luminance);
  return color * contribution;
}

vec3 bright_color(vec3 color) {
  float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
  float contribution = smoothstep(
      kBloomThresholdLow, kBloomThresholdHigh, luminance);
  return color * contribution;
}

void main(void) {
  vec3 base = sample_source(v_texcoord);
  vec3 color = base;
  if (u_bloom_enable > 0.5) {
    vec2 horizontal = vec2(u_source_texel_x * 3.0, 0.0);
    vec2 vertical = vec2(0.0, u_source_texel_y * 3.0);
    // Keep the Pi4 path single-pass, but anchor the energy at the emitting
    // pixel. Equal full-strength offset copies look like a displaced shadow
    // around text instead of a centered CRT light halo.
    vec3 bloom = bright_color(base) * 0.5;
    bloom += bright_sample(v_texcoord - horizontal) * 0.125;
    bloom += bright_sample(v_texcoord + horizontal) * 0.125;
    bloom += bright_sample(v_texcoord - vertical) * 0.125;
    bloom += bright_sample(v_texcoord + vertical) * 0.125;
    color += bloom * clamp(u_bloom_factor, 0.0, 5.0) * 0.2;
  }
  gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}

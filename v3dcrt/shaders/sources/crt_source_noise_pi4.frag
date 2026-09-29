#ifdef GL_ES
precision highp float;
#endif

#include "modules/uniforms_u_texture_through_u_source_texel_y.glsl"
uniform float u_noise_enable;
uniform float u_luminance_noise;
uniform float u_chroma_noise;
uniform float u_noise_speed;
uniform float u_temporal_frame;

varying vec2 v_texcoord;

#include "modules/noise_hash.glsl"

void main(void) {
  vec3 color = texture2D(u_texture, v_texcoord).rgb;
  if (u_noise_enable > 0.5) {
    vec2 source_texel = max(
        vec2(u_source_texel_x, u_source_texel_y), vec2(0.000001));
    vec2 source_pixel = floor(v_texcoord / source_texel);
    float noise_frame = floor(
        u_temporal_frame * clamp(u_noise_speed, 0.0, 1.0));
    float normalization = 3.4641016;
    float luma =
        (noise_hash(source_pixel, noise_frame) - 0.5) * normalization;
    float signal = 0.35 + 0.75 *
        dot(color, vec3(0.299, 0.587, 0.114));
    color += vec3(luma * signal * max(u_luminance_noise, 0.0));

    float chroma_i =
        (noise_hash(source_pixel, noise_frame + 41.0) - 0.5) *
        normalization;
    float chroma_q =
        (noise_hash(source_pixel, noise_frame + 71.0) - 0.5) *
        normalization;
    color += vec3(chroma_i * 0.956 + chroma_q * 0.621,
                  chroma_i * -0.272 + chroma_q * -0.6474,
                  chroma_i * -1.106 + chroma_q * 1.7046) *
        max(u_chroma_noise, 0.0);
  }
  gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}

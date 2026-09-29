#ifdef GL_ES
precision highp float;
#endif

uniform sampler2D u_texture;
uniform float u_source_texel_x;

varying vec2 v_texcoord;

const float kBloomThresholdLow = 0.20;
const float kBloomThresholdHigh = 0.65;

vec3 bright_sample(vec2 uv) {
  vec3 color = texture2D(u_texture, uv).rgb;
  float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
  float contribution = smoothstep(
      kBloomThresholdLow, kBloomThresholdHigh, luminance);
  return color * contribution;
}

void main(void) {
  vec2 side = vec2(u_source_texel_x * 3.16227766016838, 0.0);
  vec3 color = bright_sample(v_texcoord) * 0.6;
  color += bright_sample(v_texcoord - side) * 0.2;
  color += bright_sample(v_texcoord + side) * 0.2;
  gl_FragColor = vec4(color, 1.0);
}

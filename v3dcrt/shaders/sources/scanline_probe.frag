precision mediump float;

uniform sampler2D u_texture;
uniform vec2 u_output_size;
uniform float u_scanline_weight;
uniform float u_gap_brightness;

varying vec2 v_texcoord;

void main(void) {
  vec3 color = texture2D(u_texture, v_texcoord).rgb;
  float row = floor(gl_FragCoord.y);
  float dark_row = mod(row, 2.0);
  float strength = clamp(u_scanline_weight, 0.0, 1.0);
  float gap = mix(1.0, clamp(u_gap_brightness, 0.0, 1.0), strength);
  float scale = mix(1.0, gap, dark_row);
  gl_FragColor = vec4(color * scale, 1.0);
}

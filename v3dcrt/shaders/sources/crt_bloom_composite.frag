#ifdef GL_ES
precision highp float;
#endif

uniform sampler2D u_texture;
uniform sampler2D u_bloom_texture;
uniform float u_bloom_factor;
uniform float u_rounded_screen_mask_enable;
uniform float u_rounded_corner_radius;
uniform float u_rounded_border_softness;

varying vec2 v_texcoord;

float rounded_screen_mask(vec2 screen_point) {
  float radius = clamp(u_rounded_corner_radius, 0.0, 0.45) * 2.0;
  vec2 box = vec2(1.0 - radius);
  vec2 q = abs(screen_point) - box;
  float outside = length(max(q, vec2(0.0)));
  float inside = min(max(q.x, q.y), 0.0);
  float distance = outside + inside - radius;
  float softness = max(u_rounded_border_softness * 2.0, 0.0001);
  return 1.0 - smoothstep(-softness, softness, distance);
}

void main(void) {
  vec3 base = texture2D(u_texture, v_texcoord).rgb;
  vec3 bloom = texture2D(u_bloom_texture, v_texcoord).rgb;
  vec3 color = base + bloom * clamp(u_bloom_factor, 0.0, 5.0) * 0.2;
  if (u_rounded_screen_mask_enable > 0.5) {
    color *= rounded_screen_mask(v_texcoord * 2.0 - 1.0);
  }
  gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}

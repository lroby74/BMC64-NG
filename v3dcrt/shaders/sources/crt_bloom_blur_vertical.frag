#ifdef GL_ES
precision highp float;
#endif

uniform sampler2D u_texture;
uniform float u_source_texel_y;

varying vec2 v_texcoord;

void main(void) {
  float source_edge = v_texcoord.y / u_source_texel_y;
  float anchor_edge = floor(source_edge + 0.5);
  vec2 anchor = vec2(v_texcoord.x, anchor_edge * u_source_texel_y);
  vec3 color = texture2D(u_texture, anchor).rgb * 0.193353196569;
  color += texture2D(u_texture, anchor - vec2(0.0, u_source_texel_y * 1.97068539673)).rgb * 0.172232818624;
  color += texture2D(u_texture, anchor + vec2(0.0, u_source_texel_y * 1.97068539673)).rgb * 0.172232818624;
  color += texture2D(u_texture, anchor - vec2(0.0, u_source_texel_y * 3.94157163419)).rgb * 0.12173190974;
  color += texture2D(u_texture, anchor + vec2(0.0, u_source_texel_y * 3.94157163419)).rgb * 0.12173190974;
  color += texture2D(u_texture, anchor - vec2(0.0, u_source_texel_y * 5.91285408652)).rgb * 0.0682655637399;
  color += texture2D(u_texture, anchor + vec2(0.0, u_source_texel_y * 5.91285408652)).rgb * 0.0682655637399;
  color += texture2D(u_texture, anchor - vec2(0.0, u_source_texel_y * 7.88471750795)).rgb * 0.030372649658;
  color += texture2D(u_texture, anchor + vec2(0.0, u_source_texel_y * 7.88471750795)).rgb * 0.030372649658;
  color += texture2D(u_texture, anchor - vec2(0.0, u_source_texel_y * 9.85733147527)).rgb * 0.0107204599527;
  color += texture2D(u_texture, anchor + vec2(0.0, u_source_texel_y * 9.85733147527)).rgb * 0.0107204599527;
  gl_FragColor = vec4(color, 1.0);
}

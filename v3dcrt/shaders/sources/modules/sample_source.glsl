vec3 sample_source(vec2 uv) {
  vec2 half_texel = vec2(u_source_texel_x, u_source_texel_y) * 0.5;
  return texture2D(u_texture,
                   clamp(uv, half_texel, vec2(1.0) - half_texel)).rgb;
}

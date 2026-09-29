vec3 edge_glow_sample(vec2 screen_uv) {
  vec3 color = sample_source(screen_uv);
  float bright_weight =
      0.25 + smoothstep(0.35, 0.95, luminance(color)) * 0.75;
  return color * bright_weight;
}

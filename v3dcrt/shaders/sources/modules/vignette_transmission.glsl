float vignette_transmission(vec2 point) {
  vec2 optical = vec2(point.x * 0.75, point.y);
  float scale = clamp(u_vignette_scale, 0.2, 1.0);
  float softness = max(u_vignette_softness, 0.02);
  float edge = smoothstep(scale, scale + softness, length(optical));
  return 1.0 - clamp(u_vignette_strength, 0.0, 1.0) * edge;
}

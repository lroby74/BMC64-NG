float source_coverage(vec2 uv) {
  vec2 pixel_width = max(fwidth(uv), vec2(0.000001));
  vec2 edge_distance = min(uv, vec2(1.0) - uv);
  vec2 coverage = smoothstep(-0.5 * pixel_width,
                             0.5 * pixel_width,
                             edge_distance);
  return coverage.x * coverage.y;
}

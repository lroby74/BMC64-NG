float rounded_screen_mask(vec2 point) {
  float radius = clamp(u_rounded_corner_radius, 0.0, 0.45) * 2.0;
  vec2 box = vec2(1.0 - radius);
  vec2 q = abs(point) - box;
  float outside = length(max(q, vec2(0.0)));
  float inside = min(max(q.x, q.y), 0.0);
  float distance = outside + inside - radius;
  float softness = max(u_rounded_border_softness * 2.0, 0.0001);
  return 1.0 - smoothstep(-softness, softness, distance);
}

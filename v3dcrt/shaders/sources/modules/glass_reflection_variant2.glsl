float glass_reflection(vec2 point) {
  float normal = point.x * u_glass_reflection_cosine;
  normal += point.y * u_glass_reflection_sine;
  float face = normal + 0.12 * point.x * point.x;
  face -= 0.08 * point.y * point.y;
  face -= u_precomputed_glass_position;
  float stripe = 1.0 - smoothstep(
      u_glass_reflection_width,
      u_precomputed_glass_outer_width,
      abs(face));
  float fresnel = pow(clamp(dot(point, point) * 0.5, 0.0, 1.0), 1.8);
  return clamp(stripe * 0.85 + fresnel * 0.35, 0.0, 1.0);
}

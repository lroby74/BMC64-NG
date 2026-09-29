float glass_reflection(vec2 point) {
  float normal = point.x * cos(u_glass_reflection_angle);
  normal += point.y * sin(u_glass_reflection_angle);
  float face = normal + 0.12 * point.x * point.x;
  face -= 0.08 * point.y * point.y;
  face -= u_glass_reflection_position * 2.0 - 1.0;
  float width = max(u_glass_reflection_width, 0.01);
  float stripe = 1.0 - smoothstep(width, width * 2.5, abs(face));
  float fresnel = pow(clamp(dot(point, point) * 0.5, 0.0, 1.0), 1.8);
  return clamp(stripe * 0.85 + fresnel * 0.35, 0.0, 1.0);
}

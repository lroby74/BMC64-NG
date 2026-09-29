float uneven_illumination_gain(vec2 point) {
  float scale = clamp(u_uneven_illumination_scale, 0.02, 0.25);
  float sx = 1.2 + scale * 8.0;
  float sy = 0.9 + scale * 7.0;
  float field = sin(point.x * sx + 0.73) * 0.42;
  field += cos(point.y * sy + 2.11) * 0.32;
  field += sin((point.x * 0.75 + point.y * 0.55) *
               (sx * 0.8) + 4.37) * 0.20;
  field += cos((point.x * -0.45 + point.y * 0.85) *
               (sy * 0.7) + 5.19) * 0.16;
  return 1.0 + field *
      clamp(u_uneven_illumination_strength, 0.0, 0.35);
}

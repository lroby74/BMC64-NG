vec2 geometry_coordinate(vec2 uv) {
  vec2 point = uv * 2.0 - 1.0;
  vec2 rotated = vec2(
      point.x * u_rotation_cosine + point.y * u_rotation_sine,
      -point.x * u_rotation_sine + point.y * u_rotation_cosine);
  float denominator = 1.0 + u_trapezoid * rotated.y;
  denominator = sign(denominator) * max(abs(denominator), 0.15);
  vec2 source;
  source.x = (rotated.x - u_skew_x * rotated.y) / denominator;
  source.y = rotated.y - u_skew_y * source.x;
  float source_x2 = source.x * source.x;
  float source_y2 = source.y * source.y;
  source.x *= 1.0 + u_curvature_x * source_y2;
  source.y *= 1.0 + u_curvature_y * source_x2;
  source /= max(u_overscan_scale, 1.0);
  return source * 0.5 + 0.5;
}

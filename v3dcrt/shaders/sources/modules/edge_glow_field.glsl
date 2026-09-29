vec4 edge_glow_field(vec2 screen_uv, float width) {
  vec3 top = edge_glow_sample(vec2(0.50, 0.05));
  vec3 bottom = edge_glow_sample(vec2(0.50, 0.95));
  vec3 left = edge_glow_sample(vec2(0.05, 0.50));
  vec3 right = edge_glow_sample(vec2(0.95, 0.50));
  vec4 edge_distance = vec4(
      screen_uv.y,
      1.0 - screen_uv.y,
      screen_uv.x,
      1.0 - screen_uv.x);
  vec4 edge_weight =
      vec4(1.0) - smoothstep(vec4(0.0), vec4(width), edge_distance);
  float weight_sum = dot(edge_weight, vec4(1.0));
  vec3 irradiance =
      (top * edge_weight.x +
       bottom * edge_weight.y +
       left * edge_weight.z +
       right * edge_weight.w) / max(weight_sum, 0.0001);
  float coverage = max(
      max(edge_weight.x, edge_weight.y),
      max(edge_weight.z, edge_weight.w));
  return vec4(irradiance, coverage);
}

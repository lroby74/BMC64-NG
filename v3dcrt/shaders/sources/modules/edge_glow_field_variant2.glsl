vec4 edge_glow_field(vec2 screen_uv, float width) {
  vec3 top = vec3(
      u_edge_glow_top_r,
      u_edge_glow_top_g,
      u_edge_glow_top_b);
  vec3 bottom = vec3(
      u_edge_glow_bottom_r,
      u_edge_glow_bottom_g,
      u_edge_glow_bottom_b);
  vec3 left = vec3(
      u_edge_glow_left_r,
      u_edge_glow_left_g,
      u_edge_glow_left_b);
  vec3 right = vec3(
      u_edge_glow_right_r,
      u_edge_glow_right_g,
      u_edge_glow_right_b);
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

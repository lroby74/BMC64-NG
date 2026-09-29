float scanline_scale(vec2 uv) {
  const float pi = 3.14159265;
  const float two_pi = 6.28318531;
  float source_row = uv.y / max(u_source_texel_y, 0.000001);
  float footprint = max(fwidth(source_row), 0.0001);
  float integration_width = min(footprint, 1.0);
  float integration_phase = pi * integration_width;
  float integrated_amplitude = max(
      sin(integration_phase) / max(integration_phase, 0.0001), 0.0);
  float multisample = step(0.5, u_scanline_multisample);
  float amplitude = mix(1.0, integrated_amplitude, multisample);
  float beam = clamp(
      0.5 - 0.5 * cos(two_pi * source_row) * amplitude, 0.0, 1.0);
  float strength = clamp(u_scanline_weight, 0.0, 1.0);
  float gap = mix(1.0, clamp(u_gap_brightness, 0.0, 1.0), strength);
  return mix(gap, 1.0, beam);
}

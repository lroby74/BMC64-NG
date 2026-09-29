vec3 dither_for_rgb565(vec3 color, float modulation) {
  float amount = smoothstep(0.0, 1.0 / 64.0, modulation);
  vec3 quantum = vec3(1.0 / 31.0, 1.0 / 63.0, 1.0 / 31.0);
  return color + rgb565_dither_threshold(gl_FragCoord.xy) * quantum * amount;
}

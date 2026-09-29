vec3 phosphor_mask(void) {
  float brightness = clamp(u_phosphor_mask_brightness, 0.0, 1.0);
  float column = floor(gl_FragCoord.x);
  if (u_phosphor_mask_pattern < 1.5) {
    if (mod(column, 2.0) < 1.0) {
      return vec3(brightness, 1.0, brightness);
    }
    return vec3(1.0, brightness, 1.0);
  }
  vec3 mask = vec3(brightness);
  float triad_column = mod(column, 3.0);
  if (triad_column < 1.0) {
    mask.r = 1.0;
  } else if (triad_column < 2.0) {
    mask.g = 1.0;
  } else {
    mask.b = 1.0;
  }
  return mask;
}

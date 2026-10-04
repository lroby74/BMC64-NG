float rgb565_dither_threshold(vec2 pixel) {
  vec2 position = mod(floor(pixel), 4.0);
  vec2 low_bit = mod(position, 2.0);
  vec2 high_bit = floor(position * 0.5);
  float value = 4.0 * bayer2(low_bit.x, low_bit.y) +
                bayer2(high_bit.x, high_bit.y);
  return (value + 0.5) / 16.0 - 0.5;
}

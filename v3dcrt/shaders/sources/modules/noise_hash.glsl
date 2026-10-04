float noise_hash(vec2 position, float seed) {
  vec3 state = fract(
      vec3(position.x, position.y, position.x + position.y + seed) *
      vec3(0.1031, 0.1030, 0.0973));
  state += dot(state, state.yzx + vec3(33.33));
  return fract((state.x + state.y) * state.z);
}

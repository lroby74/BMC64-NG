vec3 apply_output_levels(vec3 color) {
  float source_luma = luminance(color);
  float black_level = clamp(u_black_level, 0.0, 1.0);
  float white_clip = clamp(u_white_clip, 0.0, 1.0);
  float mapping = clamp(floor(u_level_mapping + 0.5), 0.0, 2.0);
  float output_luma;

  if (mapping < 1.5) {
    if (mapping > 0.5) {
      black_level = black_level * black_level * black_level;
      float white_headroom = 1.0 - white_clip;
      white_clip = 1.0 -
          white_headroom * white_headroom * white_headroom;
    }
    float white_point = max(white_clip, black_level + 0.00390625);
    output_luma = clamp(
        (source_luma - black_level) / (white_point - black_level),
        0.0, 1.0);
  } else {
    float luma_squared = source_luma * source_luma;
    float shadow_target = luma_squared * luma_squared;
    float toe_luma = mix(source_luma, shadow_target, black_level);
    float headroom = 1.0 - toe_luma;
    float headroom_squared = headroom * headroom;
    float highlight_target = 1.0 - headroom_squared * headroom_squared;
    output_luma = mix(toe_luma, highlight_target, 1.0 - white_clip);
  }

  vec3 chroma = (color - vec3(source_luma)) *
      clamp(u_saturation, 0.0, 1.0);
  float positive_chroma = max(chroma.r, max(chroma.g, chroma.b));
  float negative_chroma = max(-chroma.r, max(-chroma.g, -chroma.b));
  float chroma_scale = min(
      1.0,
      min((1.0 - output_luma) / max(positive_chroma, 0.000001),
          output_luma / max(negative_chroma, 0.000001)));
  return vec3(output_luma) + chroma * max(chroma_scale, 0.0);
}

#include "pi4kms/pi4_kms_dlist.h"

#include <string.h>

namespace pi4kms {

namespace {

const uint32_t kHvsCtl0End = 1U << 31U;
const uint32_t kHvsCtl0Valid = 1U << 30U;
const uint32_t kHvsCtl0SizeShift = 24U;
const uint32_t kHvsCtl0SizeMask = 0x3fU;
const uint32_t kHvsCtl0OrderXrgb = 2U << 13U;
const uint32_t kHvsCtl0OrderArgb = 2U << 13U;
const uint32_t kHvs5Ctl0Unity = 1U << 15U;
const uint32_t kHvs5Ctl0AlphaExpand = 1U << 12U;
const uint32_t kHvs5Ctl0RgbExpand = 1U << 11U;
const uint32_t kHvsPixelFormatRgb565 = 4U;
const uint32_t kHvsPixelFormatRgba8888 = 7U;
const uint32_t kHvsCtl0ScaleShift0 = 5U;
const uint32_t kHvsCtl0ScaleShift1 = 8U;
const uint32_t kHvsScaleHorizontalPpfVerticalPpf = 0U;
const uint32_t kHvsScaleHorizontalTpzVerticalPpf = 1U;
const uint32_t kHvsScaleHorizontalPpfVerticalTpz = 2U;
const uint32_t kHvsScaleHorizontalTpzVerticalTpz = 3U;
const uint32_t kHvsScaleHorizontalPpfVerticalNone = 4U;
const uint32_t kHvsScaleHorizontalNoneVerticalPpf = 5U;
const uint32_t kHvsScaleHorizontalNoneVerticalTpz = 6U;
const uint32_t kHvsScaleHorizontalTpzVerticalNone = 7U;
const uint32_t kHvs5Ctl2AlphaModeFixed = 1U << 30U;
const uint32_t kHvs5Ctl2OpaqueAlpha = 0xfffU << 4U;
const uint32_t kHvsContextPlaceholder = 0xc0c0c0c0U;
const uint32_t kHvs5MaximumX = 0x3fffU;
const uint32_t kHvs5MaximumY = 0x0fffU;
const uint32_t kHvs5MaximumDimension = 0x1fffU;
const uint32_t kHvsMaximumRasterPitch = 0xffffU;
const uint32_t kHvsPpfNoInterpolation = 1U << 31U;
const uint32_t kHvsPpfAgc = 1U << 30U;
const uint32_t kHvsPpfPhaseBits = 6U;
const uint32_t kHvsLbmPlaneStrideWords = 4096U;

enum HvsScaling {
  kHvsScalingNone = 0,
  kHvsScalingPpf,
  kHvsScalingTpz
};

constexpr uint32_t HvsPpfFilterWord(int c0, int c1, int c2) {
  return (static_cast<uint32_t>(c0 & 0x1ff) << 0U) |
         (static_cast<uint32_t>(c1 & 0x1ff) << 9U) |
         (static_cast<uint32_t>(c2 & 0x1ff) << 18U);
}

const uint32_t kHvsNearestKernelUnique[6] = {
  HvsPpfFilterWord(0, 0, 0),
  HvsPpfFilterWord(0, 0, 0),
  HvsPpfFilterWord(1, 1, 1),
  HvsPpfFilterWord(1, 255, 255),
  HvsPpfFilterWord(255, 255, 255),
  HvsPpfFilterWord(255, 255, 0)
};

const uint32_t kHvsMitchellKernelUnique[6] = {
  HvsPpfFilterWord(0, -2, -6),
  HvsPpfFilterWord(-8, -10, -8),
  HvsPpfFilterWord(-3, 2, 18),
  HvsPpfFilterWord(50, 82, 119),
  HvsPpfFilterWord(155, 187, 213),
  HvsPpfFilterWord(227, 227, 0)
};

uint32_t Fnv1a32(const uint32_t *words, uint32_t count) {
  uint32_t hash = 2166136261U;
  for (uint32_t i = 0U; i < count; ++i) {
    uint32_t value = words[i];
    for (uint32_t byte = 0U; byte < sizeof value; ++byte) {
      hash ^= value & 0xffU;
      hash *= 16777619U;
      value >>= 8U;
    }
  }
  return hash;
}

bool FitsDisplay(uint32_t start, uint32_t size, uint32_t display_size) {
  return start <= display_size && size <= display_size - start;
}

HvsScaling GetScaling(uint32_t source, uint32_t destination) {
  if (source == destination) {
    return kHvsScalingNone;
  }
  return 3U * destination >= 2U * source ? kHvsScalingPpf
                                         : kHvsScalingTpz;
}

uint32_t MakeScaleField(HvsScaling horizontal, HvsScaling vertical) {
  if (horizontal == kHvsScalingPpf && vertical == kHvsScalingPpf) {
    return kHvsScaleHorizontalPpfVerticalPpf;
  }
  if (horizontal == kHvsScalingTpz && vertical == kHvsScalingPpf) {
    return kHvsScaleHorizontalTpzVerticalPpf;
  }
  if (horizontal == kHvsScalingPpf && vertical == kHvsScalingTpz) {
    return kHvsScaleHorizontalPpfVerticalTpz;
  }
  if (horizontal == kHvsScalingTpz && vertical == kHvsScalingTpz) {
    return kHvsScaleHorizontalTpzVerticalTpz;
  }
  if (horizontal == kHvsScalingPpf && vertical == kHvsScalingNone) {
    return kHvsScaleHorizontalPpfVerticalNone;
  }
  if (horizontal == kHvsScalingNone && vertical == kHvsScalingPpf) {
    return kHvsScaleHorizontalNoneVerticalPpf;
  }
  if (horizontal == kHvsScalingNone && vertical == kHvsScalingTpz) {
    return kHvsScaleHorizontalNoneVerticalTpz;
  }
  if (horizontal == kHvsScalingTpz && vertical == kHvsScalingNone) {
    return kHvsScaleHorizontalTpzVerticalNone;
  }
  return 0U;
}

uint32_t MakePpfWord(uint32_t source, uint32_t destination, bool nearest) {
  const uint32_t source_fixed = source << 16U;
  uint32_t scale = source_fixed / destination;
  int32_t phase = -(1 << (kHvsPpfPhaseBits - 1U));
  scale &= ~1U;
  const int32_t precision_error = static_cast<int32_t>(
      source_fixed - destination * scale) >> (16U - kHvsPpfPhaseBits);
  phase += precision_error >> 1U;
  if (phase >= (1 << kHvsPpfPhaseBits)) {
    phase = (1 << kHvsPpfPhaseBits) - 1;
  }
  phase &= (1U << (kHvsPpfPhaseBits + 1U)) - 1U;
  return (nearest ? kHvsPpfNoInterpolation : 0U) | kHvsPpfAgc |
         ((scale & 0x1ffffU) << 8U) | static_cast<uint32_t>(phase);
}

uint32_t MakeTpzWord0(uint32_t source, uint32_t destination) {
  const uint32_t source_fixed = source << 16U;
  const uint32_t scale = destination < source
      ? source_fixed / destination : (1U << 16U) + 1U;
  return (scale & 0x1fffffU) << 8U;
}

uint32_t MakeTpzWord1(uint32_t source, uint32_t destination) {
  const uint32_t source_fixed = source << 16U;
  const uint32_t scale = destination < source
      ? source_fixed / destination : (1U << 16U) + 1U;
  return (destination < source ? ~0U / scale : (1U << 16U) - 1U) &
         0xffffU;
}

bool AppendWord(uint32_t value, uint32_t *words, uint32_t capacity,
                uint32_t *count) {
  if (*count >= capacity) {
    return false;
  }
  words[(*count)++] = value;
  return true;
}

bool AppendPlane(const Hvs5Plane &plane, uint32_t plane_index,
                 uint32_t display_width, uint32_t display_height,
                 uint32_t *words, uint32_t capacity, uint32_t *count,
                 Hvs5FilterKernelUsage *required_kernels) {
  const uint32_t bytes_per_pixel =
      plane.format == kHvs5PixelFormatArgb8888 ? 4U : 2U;
  if (plane.framebuffer_bus_address == 0U ||
      (plane.framebuffer_bus_address & 1U) != 0U ||
      plane.width == 0U || plane.width > kHvs5MaximumDimension ||
      plane.height == 0U || plane.height > kHvs5MaximumDimension ||
      plane.destination_x > kHvs5MaximumX ||
      plane.destination_y > kHvs5MaximumY ||
      plane.destination_width == 0U ||
      plane.destination_width > kHvs5MaximumDimension ||
      plane.destination_height == 0U ||
      plane.destination_height > kHvs5MaximumDimension ||
      display_width == 0U || display_width > kHvs5MaximumDimension ||
      display_height == 0U || display_height > kHvs5MaximumDimension ||
      !FitsDisplay(plane.destination_x, plane.destination_width,
                   display_width) ||
      !FitsDisplay(plane.destination_y, plane.destination_height,
                   display_height) ||
      plane.width > kHvsMaximumRasterPitch / bytes_per_pixel ||
      plane.pitch < plane.width * bytes_per_pixel ||
      plane.pitch > kHvsMaximumRasterPitch ||
      (plane.pitch & (bytes_per_pixel - 1U)) != 0U ||
      (plane.format != kHvs5PixelFormatRgb565 &&
       plane.format != kHvs5PixelFormatArgb8888) ||
      (plane.filter != kHvs5ScaleFilterNearest &&
       plane.filter != kHvs5ScaleFilterMitchell)) {
    return false;
  }

  const HvsScaling horizontal =
      GetScaling(plane.width, plane.destination_width);
  const HvsScaling vertical =
      GetScaling(plane.height, plane.destination_height);
  const bool unity = horizontal == kHvsScalingNone &&
                     vertical == kHvsScalingNone;
  const bool nearest = plane.filter == kHvs5ScaleFilterNearest;
  const uint32_t start = *count;
  if (!AppendWord(0U, words, capacity, count) ||
      !AppendWord((plane.destination_y << 16U) | plane.destination_x,
                  words, capacity, count)) {
    return false;
  }
  const uint32_t control2 = plane.format == kHvs5PixelFormatRgb565
      ? kHvs5Ctl2AlphaModeFixed | kHvs5Ctl2OpaqueAlpha
      : kHvs5Ctl2OpaqueAlpha;
  if (!AppendWord(control2, words, capacity, count)) {
    return false;
  }
  if (!unity &&
      !AppendWord((plane.destination_height << 16U) |
                      plane.destination_width,
                  words, capacity, count)) {
    return false;
  }
  if (!AppendWord((plane.height << 16U) | plane.width,
                  words, capacity, count) ||
      !AppendWord(kHvsContextPlaceholder, words, capacity, count) ||
      !AppendWord(plane.framebuffer_bus_address, words, capacity, count) ||
      !AppendWord(kHvsContextPlaceholder, words, capacity, count) ||
      !AppendWord(plane.pitch, words, capacity, count)) {
    return false;
  }

  if (vertical != kHvsScalingNone &&
      !AppendWord(plane_index * kHvsLbmPlaneStrideWords,
                  words, capacity, count)) {
    return false;
  }
  if (horizontal == kHvsScalingPpf &&
      !AppendWord(MakePpfWord(plane.width, plane.destination_width, nearest),
                  words, capacity, count)) {
    return false;
  }
  if (vertical == kHvsScalingPpf &&
      (!AppendWord(MakePpfWord(plane.height, plane.destination_height, nearest),
                   words, capacity, count) ||
       !AppendWord(kHvsContextPlaceholder, words, capacity, count))) {
    return false;
  }
  if (horizontal == kHvsScalingTpz &&
      (!AppendWord(MakeTpzWord0(plane.width, plane.destination_width),
                   words, capacity, count) ||
       !AppendWord(MakeTpzWord1(plane.width, plane.destination_width),
                   words, capacity, count))) {
    return false;
  }
  if (vertical == kHvsScalingTpz &&
      (!AppendWord(MakeTpzWord0(plane.height, plane.destination_height),
                   words, capacity, count) ||
       !AppendWord(MakeTpzWord1(plane.height, plane.destination_height),
                   words, capacity, count) ||
       !AppendWord(kHvsContextPlaceholder, words, capacity, count))) {
    return false;
  }
  if (horizontal == kHvsScalingPpf || vertical == kHvsScalingPpf) {
    const uint32_t kernel_slot = nearest ? kHvs5NearestKernelSlot
                                         : kHvs5MitchellKernelSlot;
    for (uint32_t i = 0U; i < 4U; ++i) {
      if (!AppendWord(kernel_slot, words, capacity, count)) {
        return false;
      }
    }
    if (nearest) {
      required_kernels->nearest = true;
    } else {
      required_kernels->mitchell = true;
    }
  }

  const uint32_t plane_words = *count - start;
  if (plane_words == 0U || plane_words > 0x3fU) {
    return false;
  }
  const uint32_t pixel_format =
      plane.format == kHvs5PixelFormatArgb8888
          ? kHvsPixelFormatRgba8888 : kHvsPixelFormatRgb565;
  const uint32_t pixel_order =
      plane.format == kHvs5PixelFormatArgb8888
          ? kHvsCtl0OrderArgb : kHvsCtl0OrderXrgb;
  const uint32_t scale_field = unity ? 0U
      : MakeScaleField(horizontal, vertical);
  words[start] = kHvsCtl0Valid |
                 (plane_words << kHvsCtl0SizeShift) |
                 pixel_order |
                 (unity ? kHvs5Ctl0Unity : 0U) |
                 kHvs5Ctl0AlphaExpand | kHvs5Ctl0RgbExpand |
                 (scale_field << kHvsCtl0ScaleShift0) |
                 (scale_field << kHvsCtl0ScaleShift1) |
                 pixel_format;
  return true;
}

}  // namespace

bool InspectHvs5Dlist(const uint32_t *words, uint32_t word_count,
                      Hvs5DlistInfo *info) {
  if (info == nullptr) {
    return false;
  }
  memset(info, 0, sizeof *info);
  if (words == nullptr || word_count == 0U) {
    return false;
  }

  uint32_t offset = 0U;
  while (offset < word_count) {
    const uint32_t control = words[offset];
    if ((control & kHvsCtl0End) != 0U) {
      info->end_found = true;
      info->used_words = offset + 1U;
      info->hash = Fnv1a32(words, info->used_words);
      return true;
    }
    if ((control & kHvsCtl0Valid) == 0U) {
      return false;
    }

    const uint32_t plane_words =
        (control >> kHvsCtl0SizeShift) & kHvsCtl0SizeMask;
    if (plane_words == 0U || plane_words >= word_count - offset) {
      return false;
    }
    offset += plane_words;
    ++info->plane_count;
  }
  return false;
}

bool BuildHvs5Rgb565UnityDlist(const Hvs5Rgb565UnityPlane &plane,
                              uint32_t *words, uint32_t capacity,
                              uint32_t *word_count) {
  if (word_count != nullptr) {
    *word_count = 0U;
  }
  if (words == nullptr || word_count == nullptr ||
      capacity < kHvs5Rgb565UnityDlistWords ||
      plane.framebuffer_bus_address == 0U ||
      (plane.framebuffer_bus_address & 1U) != 0U ||
      plane.width == 0U || plane.width > kHvs5MaximumDimension ||
      plane.height == 0U || plane.height > kHvs5MaximumDimension ||
      plane.destination_x > kHvs5MaximumX ||
      plane.destination_y > kHvs5MaximumY ||
      plane.display_width == 0U ||
      plane.display_width > kHvs5MaximumDimension ||
      plane.display_height == 0U ||
      plane.display_height > kHvs5MaximumDimension ||
      !FitsDisplay(plane.destination_x, plane.width, plane.display_width) ||
      !FitsDisplay(plane.destination_y, plane.height, plane.display_height) ||
      plane.width > kHvsMaximumRasterPitch / 2U ||
      plane.pitch < plane.width * 2U ||
      plane.pitch > kHvsMaximumRasterPitch ||
      (plane.pitch & 1U) != 0U) {
    return false;
  }

  words[0] = kHvsCtl0Valid |
             (kHvs5Rgb565UnityPlaneWords << kHvsCtl0SizeShift) |
             kHvsCtl0OrderXrgb | kHvs5Ctl0Unity |
             kHvs5Ctl0AlphaExpand | kHvs5Ctl0RgbExpand |
             kHvsPixelFormatRgb565;
  words[1] = (plane.destination_y << 16U) | plane.destination_x;
  words[2] = kHvs5Ctl2AlphaModeFixed | kHvs5Ctl2OpaqueAlpha;
  words[3] = (plane.height << 16U) | plane.width;
  words[4] = kHvsContextPlaceholder;
  words[5] = plane.framebuffer_bus_address;
  words[6] = kHvsContextPlaceholder;
  words[7] = plane.pitch;
  words[8] = kHvsCtl0End;
  *word_count = kHvs5Rgb565UnityDlistWords;
  return true;
}

bool BuildHvs5Dlist(const Hvs5Plane *planes, uint32_t plane_count,
                    uint32_t display_width, uint32_t display_height,
                    uint32_t *words, uint32_t capacity,
                    uint32_t *word_count,
                    Hvs5FilterKernelUsage *required_kernels) {
  if (word_count != nullptr) {
    *word_count = 0U;
  }
  if (required_kernels != nullptr) {
    memset(required_kernels, 0, sizeof *required_kernels);
  }
  if (planes == nullptr || plane_count == 0U ||
      plane_count > kHvs5MaximumPlanes || words == nullptr ||
      word_count == nullptr || required_kernels == nullptr ||
      capacity == 0U || capacity > kHvs5MaximumArmDlistWords) {
    return false;
  }

  uint32_t count = 0U;
  for (uint32_t i = 0U; i < plane_count; ++i) {
    if (!AppendPlane(planes[i], i, display_width, display_height,
                     words, capacity, &count, required_kernels)) {
      return false;
    }
  }
  if (!AppendWord(kHvsCtl0End, words, capacity, &count)) {
    return false;
  }
  *word_count = count;
  return true;
}

void BuildHvs5NearestKernel(uint32_t words[kHvs5FilterKernelWords]) {
  if (words == nullptr) {
    return;
  }
  for (uint32_t i = 0U; i < kHvs5FilterKernelWords; ++i) {
    const uint32_t source = i < 6U ? i : kHvs5FilterKernelWords - i - 1U;
    words[i] = kHvsNearestKernelUnique[source];
  }
}

void BuildHvs5MitchellKernel(uint32_t words[kHvs5FilterKernelWords]) {
  if (words == nullptr) {
    return;
  }
  for (uint32_t i = 0U; i < kHvs5FilterKernelWords; ++i) {
    const uint32_t source = i < 6U ? i : kHvs5FilterKernelWords - i - 1U;
    words[i] = kHvsMitchellKernelUnique[source];
  }
}

}  // namespace pi4kms

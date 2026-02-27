// SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef KLEIDICV_OPTICAL_FLOW_LK_PYRAMID_H
#define KLEIDICV_OPTICAL_FLOW_LK_PYRAMID_H

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "kleidicv/ctypes.h"
#include "kleidicv/filters/blur_and_downsample.h"
#include "kleidicv/filters/scharr.h"

namespace KLEIDICV_TARGET_NAMESPACE {

// Per-level metadata for both pyramids.
// - image_data points to the level in the uint8 image pyramid.
// - scharr_data points to the level in the int16 interleaved dx/dy pyramid.
struct OpticalFlowLevel {
  size_t width;
  size_t height;
  size_t image_stride;
  size_t scharr_stride;
  uint8_t* image_data;
  int16_t* scharr_data;
};

class OpticalFlowLKPyramid;

class OpticalFlowLKPyramidDeleter {
 public:
  void operator()(OpticalFlowLKPyramid* ptr) const { std::free(ptr); }
};

class OpticalFlowLKPyramid final {
 public:
  using Pointer =
      std::unique_ptr<OpticalFlowLKPyramid, OpticalFlowLKPyramidDeleter>;

  // OpticalFlowLKPyramid is only creatable with create().
  OpticalFlowLKPyramid() = delete;
  OpticalFlowLKPyramid(const OpticalFlowLKPyramid&) = delete;
  OpticalFlowLKPyramid& operator=(const OpticalFlowLKPyramid&) = delete;
  OpticalFlowLKPyramid(OpticalFlowLKPyramid&&) = delete;
  OpticalFlowLKPyramid& operator=(OpticalFlowLKPyramid&&) = delete;

  // This function has many guarded allocation/overflow checks by design.
  // Keep explicit checks for safety and suppress cognitive-complexity warning.
  // NOLINTBEGIN(readability-function-cognitive-complexity)
  // Allocate one contiguous block that stores metadata and all level buffers.
  static kleidicv_error_t create(Pointer& pyramid, size_t level_count,
                                 size_t width, size_t height, size_t channels) {
    // First sizing pass: compute total bytes required for all image levels and
    // all Scharr levels.
    size_t total_image_bytes = 0;
    size_t total_scharr_bytes = 0;
    size_t level_width = width;
    size_t level_height = height;
    for (size_t level = 0; level < level_count; ++level) {
      LevelStorageLayout layout{};
      if (kleidicv_error_t err = compute_level_storage_layout(
              level_width, level_height, channels, &layout)) {
        return err;
      }

      // Accumulate total storage required across all levels.
      if (add_overflow_size(total_image_bytes, layout.image_bytes,
                            &total_image_bytes) ||
          add_overflow_size(total_scharr_bytes, layout.scharr_bytes,
                            &total_scharr_bytes)) {
        // Unreachable for practical API paths: per-level sizes are already
        // checked, and level count is naturally small due pyramid halving.
        // Keep the guard as a hard safety net for defensive programming.
        return KLEIDICV_ERROR_RANGE;  // GCOVR_EXCL_LINE
      }

      // LK pyramid geometry uses ceil division by 2 between levels.
      level_width = (level_width + 1) / 2;
      level_height = (level_height + 1) / 2;
    }

    size_t levels_bytes = 0;
    // Metadata table footprint: one OpticalFlowLevel entry per pyramid level.
    if (__builtin_mul_overflow(level_count, sizeof(OpticalFlowLevel),
                               &levels_bytes)) {
      // Unreachable for practical API paths: level count is bounded by input
      // geometry and stop criteria. Keep as defensive overflow protection.
      return KLEIDICV_ERROR_RANGE;  // GCOVR_EXCL_LINE
    }

    // Single-allocation packed layout:
    // [object header][levels table][image storage][scharr storage]
    //
    // Each section start is aligned for its element type, and each size/offset
    // update is overflow-checked to keep allocation math safe.
    size_t levels_offset = 0;
    size_t image_offset = 0;
    size_t scharr_offset = 0;
    size_t allocation_size = 0;
    if (!align_up_size(sizeof(OpticalFlowLKPyramid), alignof(OpticalFlowLevel),
                       &levels_offset) ||
        add_overflow_size(levels_offset, levels_bytes, &image_offset) ||
        !align_up_size(image_offset, alignof(int16_t), &image_offset) ||
        add_overflow_size(image_offset, total_image_bytes, &scharr_offset) ||
        !align_up_size(scharr_offset, alignof(int16_t), &scharr_offset) ||
        add_overflow_size(scharr_offset, total_scharr_bytes,
                          &allocation_size)) {
      // Unreachable for practical API paths after previous bounded-size checks.
      // Keep as final allocation-layout overflow guard.
      return KLEIDICV_ERROR_RANGE;  // GCOVR_EXCL_LINE
    }

    void* allocation = std::malloc(allocation_size);
    pyramid = Pointer{reinterpret_cast<OpticalFlowLKPyramid*>(allocation)};
    if (!pyramid) {
      return KLEIDICV_ERROR_ALLOCATION;
    }

    auto* instance = pyramid.get();
    instance->level_count_ = level_count;
    instance->channels_ = channels;

    auto* allocation_base = reinterpret_cast<uint8_t*>(instance);
    instance->levels_ =
        reinterpret_cast<OpticalFlowLevel*>(allocation_base + levels_offset);

    auto* images_cursor = allocation_base + image_offset;
    auto* scharr_cursor = allocation_base + scharr_offset;

    // Second pass: fill metadata and point level entries into the contiguous
    // block.
    level_width = width;
    level_height = height;
    for (size_t level = 0; level < level_count; ++level) {
      // Bind the metadata entry for this pyramid level and store the logical
      // (interior) image size that callers see through the getter APIs.
      auto& entry = instance->levels_[level];
      entry.width = level_width;
      entry.height = level_height;

      LevelStorageLayout layout{};
      if (kleidicv_error_t err = compute_level_storage_layout(
              level_width, level_height, channels, &layout)) {
        // Unreachable: second pass uses the same per-level dimensions/channels
        // already validated in the first pass. Keep as defensive check.
        return err;  // GCOVR_EXCL_LINE
      }
      entry.image_stride = layout.image_stride;
      entry.scharr_stride = layout.scharr_stride;

      // images_cursor points at the beginning of this level's bordered image
      // allocation.
      auto* image_base = images_cursor;

      // Expose a pointer to the interior top-left pixel by skipping:
      //   - one full bordered row   -> + image_stride
      //   - one pixel at row start  -> + channels
      //
      // This allows border writes/reads via pointer arithmetic around
      // entry.image_data without extra allocations/copies.
      entry.image_data = image_base + entry.image_stride + channels;

      // scharr_cursor points at the beginning of this level's bordered Scharr
      // allocation (int16_t elements viewed through byte cursor).
      auto* scharr_base = reinterpret_cast<int16_t*>(scharr_cursor);

      // Convert Scharr stride bytes to element count for pointer arithmetic.
      const size_t scharr_stride_elems = entry.scharr_stride / sizeof(int16_t);

      // Interior starts after:
      //   - one bordered row in Scharr space -> scharr_stride_elems
      //   - one bordered pixel in x direction -> channels * 2 elements
      const size_t scharr_interior_offset =
          scharr_stride_elems + (channels * 2);
      entry.scharr_data = scharr_base + scharr_interior_offset;

      // Move cursors to the next level's storage region within the same
      // allocation block.
      images_cursor += layout.image_bytes;
      scharr_cursor += layout.scharr_bytes;

      // Next level dimensions follow the LK pyramid halving rule
      // (ceil division by 2).
      level_width = (level_width + 1) / 2;
      level_height = (level_height + 1) / 2;
    }

    return KLEIDICV_OK;
  }
  // NOLINTEND(readability-function-cognitive-complexity)

  // Default processing entry point.
  //
  // This overload wires the standard single-threaded kernels
  // (blur_and_downsample + scharr_interleaved) and forwards to the generic
  // overload below.
  //
  // Note on naming:
  // There are intentionally two functions named process():
  // 1) this one: convenience wrapper with default kernel selection
  // 2) the templated overload: shared core implementation with injectable
  //    kernels (used by both single-thread and threaded builders).
  kleidicv_error_t process(const uint8_t* src, size_t src_stride) {
    auto blur_and_downsample_fn =
        [](const uint8_t* blur_src, size_t blur_src_stride, size_t blur_width,
           size_t blur_height, uint8_t* blur_dst, size_t blur_dst_stride,
           size_t blur_channels, kleidicv_border_type_t border_type) {
          return kleidicv_blur_and_downsample_u8(
              blur_src, blur_src_stride, blur_width, blur_height, blur_dst,
              blur_dst_stride, blur_channels, border_type);
        };

    auto scharr_fn = [](const uint8_t* scharr_src, size_t scharr_src_stride,
                        size_t scharr_width, size_t scharr_height,
                        size_t scharr_channels, int16_t* scharr_dst,
                        size_t scharr_dst_stride) {
      return kleidicv_scharr_interleaved_s16_u8(
          scharr_src, scharr_src_stride, scharr_width, scharr_height,
          scharr_channels, scharr_dst, scharr_dst_stride);
    };

    return process(src, src_stride, blur_and_downsample_fn, scharr_fn);
  }

  // Generic processing implementation.
  //
  // Callers provide the concrete blur/downsample and Scharr functions, which
  // allows reusing the same pyramid-construction logic for different execution
  // paths (for example single-thread vs threaded variants) without duplicating
  // the algorithm.
  template <typename BlurAndDownsampleFn, typename ScharrFn>
  kleidicv_error_t process(const uint8_t* src, size_t src_stride,
                           BlurAndDownsampleFn blur_and_downsample_fn,
                           ScharrFn scharr_fn) {
    auto build_scharr_for_level = [&](size_t level_index) -> kleidicv_error_t {
      const auto& entry = levels_[level_index];
      const size_t bordered_width = entry.width + 2;
      const size_t bordered_height = entry.height + 2;

      // Zero bordered Scharr storage so derivative border is constant.
      const size_t scharr_height_with_border = entry.height + 2;
      size_t scharr_bytes = 0;
      if (__builtin_mul_overflow(entry.scharr_stride, scharr_height_with_border,
                                 &scharr_bytes)) {
        // Unreachable: entry.scharr_stride and level geometry originate from
        // validated create() layout checks. Keep as defensive runtime guard.
        return KLEIDICV_ERROR_RANGE;  // GCOVR_EXCL_LINE
      }

      auto* scharr_base = reinterpret_cast<uint8_t*>(entry.scharr_data) -
                          entry.scharr_stride -
                          (channels_ * 2 * sizeof(int16_t));
      std::memset(scharr_base, 0, scharr_bytes);

      const auto* bordered_src =
          entry.image_data - entry.image_stride - channels_;
      if (kleidicv_error_t err = scharr_fn(
              bordered_src, entry.image_stride, bordered_width, bordered_height,
              channels_, entry.scharr_data, entry.scharr_stride)) {
        return err;
      }
      return KLEIDICV_OK;
    };

    // Build level 0 image and Scharr.
    {
      auto& level0 = levels_[0];
      const size_t interior_bytes_per_row = level0.width * channels_;
      for (size_t y = 0; y < level0.height; ++y) {
        std::memcpy(level0.image_data + y * level0.image_stride,
                    src + y * src_stride, interior_bytes_per_row);
      }
      fill_reflect_101_border_1px_in_place(level0.image_data,
                                           level0.image_stride, level0.width,
                                           level0.height, channels_);
      if (kleidicv_error_t err = build_scharr_for_level(0)) {
        return err;
      }
    }

    // Build remaining levels. Each level is completed immediately:
    // image -> border fill -> Scharr.
    for (size_t level = 1; level < level_count_; ++level) {
      const auto& previous = levels_[level - 1];
      auto& current = levels_[level];

      if (kleidicv_error_t err = blur_and_downsample_fn(
              previous.image_data, previous.image_stride, previous.width,
              previous.height, current.image_data, current.image_stride,
              channels_, KLEIDICV_BORDER_TYPE_REVERSE)) {
        return err;
      }

      fill_reflect_101_border_1px_in_place(current.image_data,
                                           current.image_stride, current.width,
                                           current.height, channels_);

      if (kleidicv_error_t err = build_scharr_for_level(level)) {
        return err;
      }
    }

    return KLEIDICV_OK;
  }

  // Returns how many pyramid levels were allocated and populated.
  // Valid level indices are in the range [0, level_count()).
  size_t level_count() const { return level_count_; }

  // Returns metadata and data pointers for one level.
  // The returned reference is non-owning and remains valid while this
  // OpticalFlowLKPyramid instance is alive.
  const OpticalFlowLevel& level(size_t index) const { return levels_[index]; }

 private:
  struct LevelStorageLayout {
    size_t image_stride;
    size_t scharr_stride;
    size_t image_bytes;
    size_t scharr_bytes;
  };

  static kleidicv_error_t compute_level_storage_layout(
      size_t level_width, size_t level_height, size_t channels,
      LevelStorageLayout* out) {
    const size_t image_width_with_border = level_width + 2;
    const size_t image_height_with_border = level_height + 2;
    size_t image_stride = 0;
    if (__builtin_mul_overflow(image_width_with_border, channels,
                               &image_stride)) {
      return KLEIDICV_ERROR_RANGE;
    }

    const size_t scharr_width_with_border = level_width + 2;
    const size_t scharr_height_with_border = level_height + 2;
    size_t scharr_elements_stride = 0;
    size_t scharr_stride = 0;
    if (__builtin_mul_overflow(scharr_width_with_border, channels,
                               &scharr_elements_stride) ||
        __builtin_mul_overflow(scharr_elements_stride, static_cast<size_t>(2),
                               &scharr_elements_stride) ||
        __builtin_mul_overflow(scharr_elements_stride, sizeof(int16_t),
                               &scharr_stride)) {
      return KLEIDICV_ERROR_RANGE;
    }

    size_t image_bytes = 0;
    if (__builtin_mul_overflow(image_stride, image_height_with_border,
                               &image_bytes)) {
      return KLEIDICV_ERROR_RANGE;
    }

    size_t scharr_bytes = 0;
    if (__builtin_mul_overflow(scharr_stride, scharr_height_with_border,
                               &scharr_bytes)) {
      return KLEIDICV_ERROR_RANGE;
    }

    out->image_stride = image_stride;
    out->scharr_stride = scharr_stride;
    out->image_bytes = image_bytes;
    out->scharr_bytes = scharr_bytes;
    return KLEIDICV_OK;
  }

  // Rounds value up to the next multiple of alignment.
  //
  // Returns false only if the intermediate value + (alignment - 1) overflows,
  // otherwise stores the aligned result in `*aligned_value` and returns true.
  static bool align_up_size(size_t value, size_t alignment,
                            size_t* aligned_value) {
    const size_t addend = alignment - 1;
    size_t with_addend = 0;
    if (__builtin_add_overflow(value, addend, &with_addend)) {
      // Unreachable for this file's call sites after bounded size checks.
      // Kept to preserve generic overflow-safe utility behavior.
      return false;  // GCOVR_EXCL_LINE
    }
    *aligned_value = with_addend & ~addend;
    return true;
  }

  // Adds two size_t values and reports overflow.
  //
  // Mirrors __builtin_add_overflow semantics:
  // - returns true  -> overflow happened
  // - returns false -> result is valid and written to `*out`
  static bool add_overflow_size(size_t a, size_t b, size_t* out) {
    return __builtin_add_overflow(a, b, out);
  }

  // Maps an index (including out-of-range indices) into [0, length)
  // using reflect-101 border behavior.
  static size_t border_index_reflect_101(ptrdiff_t index, size_t length) {
    if (length <= 1) {
      return 0;
    }

    ptrdiff_t reflected = index;
    while (reflected < 0 || reflected >= static_cast<ptrdiff_t>(length)) {
      if (reflected < 0) {
        reflected = -reflected;
      } else {
        reflected = 2 * static_cast<ptrdiff_t>(length) - reflected - 2;
      }
    }
    return static_cast<size_t>(reflected);
  }

  static void fill_reflect_101_border_1px_in_place(uint8_t* interior,
                                                   size_t stride, size_t width,
                                                   size_t height,
                                                   size_t channels) {
    // Fill the top and bottom border rows (including corner pixels).
    // `x` iterates from -1 to width, where:
    //   -1        -> left border column
    //   [0,width) -> interior columns
    //   width     -> right border column
    for (ptrdiff_t x = -1; x <= static_cast<ptrdiff_t>(width); ++x) {
      // Fold the possibly out-of-range x-coordinate to a valid source column.
      const size_t src_x = border_index_reflect_101(x, width);

      // Source row index used for the top border row (y = -1).
      const size_t src_y_top = border_index_reflect_101(-1, height);
      // Address of source pixel copied into top border at this column.
      const uint8_t* src_top = interior + src_y_top * stride + src_x * channels;
      // Address of destination top-border pixel (one row above interior).
      uint8_t* dst_top =
          interior - stride + x * static_cast<ptrdiff_t>(channels);
      // Copy one pixel (all channels) into the top border.
      std::memcpy(dst_top, src_top, channels);

      // Source row index used for the bottom border row (y = height).
      const size_t src_y_bottom =
          border_index_reflect_101(static_cast<ptrdiff_t>(height), height);
      // Address of source pixel copied into bottom border at this column.
      const uint8_t* src_bottom =
          interior + src_y_bottom * stride + src_x * channels;
      // Address of destination bottom-border pixel (one row below interior).
      uint8_t* dst_bottom =
          interior + height * stride + x * static_cast<ptrdiff_t>(channels);
      // Copy one pixel (all channels) into the bottom border.
      std::memcpy(dst_bottom, src_bottom, channels);
    }

    // Precompute mirrored source columns for left and right borders.
    const size_t src_x_left = border_index_reflect_101(-1, width);
    const size_t src_x_right =
        border_index_reflect_101(static_cast<ptrdiff_t>(width), width);
    // Fill left and right borders for every interior row.
    for (size_t y = 0; y < height; ++y) {
      // Start address of interior row y.
      uint8_t* row = interior + y * stride;

      // Source pixel used to fill left border on this row.
      const uint8_t* src_left = row + src_x_left * channels;
      // Destination address of left-border pixel (one pixel before row start).
      uint8_t* dst_left = row - channels;
      // Copy one pixel (all channels) to left border.
      std::memcpy(dst_left, src_left, channels);

      // Source pixel used to fill right border on this row.
      const uint8_t* src_right = row + src_x_right * channels;
      // Destination address of right-border pixel (one pixel after row end).
      uint8_t* dst_right = row + width * channels;
      // Copy one pixel (all channels) to right border.
      std::memcpy(dst_right, src_right, channels);
    }
  }

  size_t level_count_;
  size_t channels_;
  OpticalFlowLevel* levels_;
};

}  // namespace KLEIDICV_TARGET_NAMESPACE

#endif  // KLEIDICV_OPTICAL_FLOW_LK_PYRAMID_H

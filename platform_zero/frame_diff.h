#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Zero {

// The LCD framebuffer is live. Leave unchanged pixels alone so a focus move
// only writes the affected cells instead of refreshing the whole display.
inline void copyChangedFrame(const uint16_t* pixels, uint16_t* previous,
                             uint8_t* destination, std::size_t stride,
                             unsigned width, unsigned height, bool hasPrevious) {
  for (unsigned y = 0; y < height; ++y) {
    auto* row = destination + std::size_t{y} * stride;
    const auto* source = pixels + std::size_t{y} * width;
    auto* old = previous + std::size_t{y} * width;
    if (!hasPrevious) {
      std::memcpy(row, source, width * sizeof(*pixels));
    } else {
      unsigned x = 0;
      while (x < width) {
        while (x < width && source[x] == old[x]) ++x;
        const unsigned start = x;
        while (x < width && source[x] != old[x]) ++x;
        if (x > start) {
          std::memcpy(row + std::size_t{start} * sizeof(*pixels), source + start,
                      (x - start) * sizeof(*pixels));
        }
      }
    }
    std::memcpy(old, source, width * sizeof(*pixels));
  }
}

}  // namespace Zero

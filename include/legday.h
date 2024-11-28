#ifndef LIB_LEGDAY_H
#define LIB_LEGDAY_H

#include <cstdint>
#include <span>
#include <vector>

namespace legday {

enum Layout {
  INT8,
  BF16,
  FP32,
};

std::vector<uint8_t> compress(std::span<uint8_t> input, Layout layout);
std::vector<uint8_t> decompress(std::span<uint8_t> input);

} // namespace legday

#endif // LIB_LEGDAY_H

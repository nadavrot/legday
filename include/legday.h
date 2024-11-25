#ifndef LIB_LEGDAY_H
#define LIB_LEGDAY_H

#include <cstdint>
#include <span>
#include <vector>

namespace legday {
/// Convert a buffer to a transposed buffer. Each bit is extracted and placed
/// consecutively. The size of the returned buffer is equal to the size of the
// input buffer. The input buffer must be a multiple of 8.
std::vector<uint8_t> transpose(std::span<uint8_t> buffer);

/// Reverses the transpose operation.
std::vector<uint8_t> untranspose(std::span<uint8_t> in);
} // namespace legday

#endif // LIB_LEGDAY_H

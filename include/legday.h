#ifndef LIB_LEGDAY_H
#define LIB_LEGDAY_H

#include <cstdint>
#include <optional>
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

/// An arithmetic encoder that encodes one bit at a time, with a given
/// probability expressed as a 16-bit integer.
class BitonicEncoder {
  uint32_t low_;
  uint32_t high_;
  std::vector<uint8_t> &output_;

public:
  BitonicEncoder(std::vector<uint8_t> &output);

  /// Encode the bit 'bit' with probability 'prob' in the range 0..65536.
  /// Return the number of bytes written.
  size_t encode(bool bit, uint16_t prob);

  /// Seal the stream by flushing the state.
  size_t finalize();
};

/// An arithmetic decoder that decodes one bit at a time, with a given
/// probability expressed as a 16-bit integer. See 'BitonicEncoder' for details.
class BitonicDecoder {
  /// The input bit stream (read from the beginning).
  std::span<uint8_t> input_;
  /// Marks the location in the bitstream.
  size_t cursor_;
  /// The low point of the range.
  uint32_t low_;
  /// The high point of the range.
  uint32_t high_;
  /// The current state.
  uint32_t state_;

public:
  BitonicDecoder(std::span<uint8_t> input);

  /// Decode one bit with a probability 'prob' in the range 0..65536.
  std::optional<bool> decode(uint16_t prob);
};

#endif // LIB_LEGDAY_H

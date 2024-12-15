#ifndef LIB_LEGDAY_IMPL_H
#define LIB_LEGDAY_IMPL_H

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace legday {

/// Wraps a buffer of bytes as a stream of bits.
template <unsigned NumChannels> class Stream {
  std::span<uint8_t> buffer_;
  static_assert(NumChannels % 8 == 0, "NumChannels must be a multiple of 8");

public:
  Stream(std::span<uint8_t> buffer) : buffer_(buffer) {}

  /// Returns the number of words in the stream.
  size_t size() { return (buffer_.size() * 8) / NumChannels; }

  /// Sets the 'channel'-th bit in word at 'offset' to the value 'value'.
  void set(size_t offset, uint8_t channel, bool value) {
    size_t num_channels_bytes = NumChannels / 8;
    // Find the location of the word.
    size_t base = (offset * num_channels_bytes);
    // Figure out which byte in the word.
    size_t byte_idx = channel / 8;
    uint8_t bit = (channel % 8);
    buffer_[base + byte_idx] &= ~(0x1 << bit);
    buffer_[base + byte_idx] |= (value & 0x1) << bit;
  }

  /// Returns the 'channel'-th bit in word at 'offset'.
  bool get(size_t offset, uint8_t channel) {
    size_t num_channels_bytes = NumChannels / 8;
    // Find the location of the word.
    size_t base = (offset * num_channels_bytes);
    // Figure out which byte in the word.
    size_t byte_idx = channel / 8;
    uint8_t bit = (channel % 8);
    return (buffer_[base + byte_idx] >> bit) & 0x1;
  }

  /// return the NUM_BITS bits before the 'bit'-th bit in word at 'offset'.
  template <unsigned NumBits>
  uint8_t get_bits_before(size_t offset, uint8_t bit) {
    size_t num_channels_bytes = NumChannels / 8;
    size_t base = (offset * num_channels_bytes);
    uint64_t value = 0;
    for (int i = 0; i < NumChannels / 8; i++) {
      value |= uint64_t(buffer_[base + i]) << (i * 8);
    }
    // The bit sequence might start before the first byte. We clear the top by
    // shifting, avoiding overflow, and shifting back, which may underflow.
    uint64_t shifted = (value << (32 - bit)) & 0xffffffff;
    return shifted >> (32 - NumBits);
  }
};

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

  /// Return the number of bytes consumed.
  size_t consumed() { return cursor_; }
};

template <typename T> void push(std::vector<uint8_t> &output, T value) {
  for (int i = 0; i < sizeof(T); i++) {
    output.push_back(uint8_t(value >> (i * 8)));
  }
}

template <typename T> T read(std::span<uint8_t> input, size_t offset) {
  T value = 0;
  for (int i = sizeof(T) - 1; i >= 0; i--) {
    value = (value << 8) | input[offset + i];
  }
  return value;
}

/// Rotate right each group of uint16_t by n bits.
void rotate_b16(std::span<uint8_t> input, uint8_t n);

} // namespace legday

#endif // LIB_LEGDAY_IMPL_H

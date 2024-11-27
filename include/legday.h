#ifndef LIB_LEGDAY_H
#define LIB_LEGDAY_H

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace legday {

enum Layout {
  FP32,
  FP16,
  BF16,
  FP8,
  INT8,
};

constexpr uint8_t channels_for_layout[6] = {32, 32, 16, 16, 8, 8};

/// Count the number of bits in a buffer.
uint64_t popcnt(std::span<uint8_t> buffer);

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

  /// Storage for the popcnt result.
  using ArrayPopcntTy = uint64_t[NumChannels];

  /// Fill the array 'ones' with the number of bits in each channel.
  void popcnt(ArrayPopcntTy &ones) {
    size_t num_channels_bytes = NumChannels / 8;
    for (int i = 0; i < NumChannels; i++) {
      ones[i] = 0;
    }
    for (size_t offset = 0; offset < buffer_.size() / num_channels_bytes;
         offset++) {
      for (int channel = 0; channel < NumChannels; channel++) {
        ones[channel] += get(offset, channel);
      }
    }
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

void transform_bf16_buffer(std::span<uint8_t> input, bool forward);
std::vector<uint8_t> compress(std::span<uint8_t> input, Layout layout);
std::vector<uint8_t> decompress(std::span<uint8_t> input);

} // namespace legday

#endif // LIB_LEGDAY_H

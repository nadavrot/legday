#include "legday.h"

#include <cassert>

uint8_t legday::popcnt(uint8_t byte) {
  constexpr uint8_t bits[256] = {
      0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4,
      2, 3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
      2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4,
      2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
      2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
      4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
      2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5,
      3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
      2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
      4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
      4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8};

  return bits[byte];
}

static constexpr uint8_t nthbit(uint8_t byte, size_t n) {
  return (byte >> n) & 1;
}

std::vector<uint8_t> legday::transpose(std::span<uint8_t> buffer) {
  assert(buffer.size() % 8 == 0);
  std::vector<uint8_t> result(buffer.size(), 0);
  // The transposed buffer is split into 8 parts.
  size_t num_parts = buffer.size() / 8;

  // For each destination byte in a part of the transposed buffer.
  for (size_t dest = 0; dest < num_parts; dest++) {
    // The source offset is multiplied by 8 because we pack 8 bytes at once.
    size_t src = dest * 8;

    // For each bit in the source bytes.
    for (int bit = 0; bit < 8; bit++) {
      result[dest + bit * num_parts] |= nthbit(buffer[src + 0], bit) << 0 |
                                        nthbit(buffer[src + 1], bit) << 1 |
                                        nthbit(buffer[src + 2], bit) << 2 |
                                        nthbit(buffer[src + 3], bit) << 3 |
                                        nthbit(buffer[src + 4], bit) << 4 |
                                        nthbit(buffer[src + 5], bit) << 5 |
                                        nthbit(buffer[src + 6], bit) << 6 |
                                        nthbit(buffer[src + 7], bit) << 7;
    }
  }
  return result;
}

std::vector<uint8_t> legday::untranspose(std::span<uint8_t> in) {
  assert(in.size() % 8 == 0);
  std::vector<uint8_t> result(in.size(), 0);

  // The transposed buffer is split into 8 parts.
  size_t num_parts = in.size() / 8;

  // For each destination byte in a part of the transposed buffer.
  for (size_t dest = 0; dest < num_parts; dest++) {
    // For each bit:
    for (int bit = 0; bit < 8; bit++) {
      result[dest * 8 + bit] |= nthbit(in[dest + 0 * num_parts], bit) << 0 |
                                nthbit(in[dest + 1 * num_parts], bit) << 1 |
                                nthbit(in[dest + 2 * num_parts], bit) << 2 |
                                nthbit(in[dest + 3 * num_parts], bit) << 3 |
                                nthbit(in[dest + 4 * num_parts], bit) << 4 |
                                nthbit(in[dest + 5 * num_parts], bit) << 5 |
                                nthbit(in[dest + 6 * num_parts], bit) << 6 |
                                nthbit(in[dest + 7 * num_parts], bit) << 7;
    }
  }

  return result;
}

/// Constructor:
BitonicEncoder::BitonicEncoder(std::vector<uint8_t> &output)
    : low_(0), high_(0xffffffff), output_(output) {}

size_t BitonicEncoder::encode(bool bit, uint16_t prob) {
  assert(high_ > low_);

  // Figure out the mid point of the range, depending on the probability.
  uint64_t gap = (high_ - low_);
  uint64_t scale = (gap * uint64_t(prob)) >> 16;
  uint32_t mid = low_ + uint32_t(scale);
  assert(high_ > mid && mid >= low_);

  // Select the sub-range based on the bit.
  if (bit) {
    high_ = mid;
  } else {
    low_ = mid + 1;
  }

  size_t wrote = 0;
  // Write the identical leading bytes.
  while ((high_ ^ low_) < (1 << 24)) {
    output_.push_back(uint8_t(high_ >> 24));
    high_ = (high_ << 8) + 0xff;
    low_ <<= 8;
    wrote += 1;
  }
  return wrote;
}

size_t BitonicEncoder::finalize() {
  // Encode a zero-probability token which flushes the state.
  return encode(true, 0);
}

BitonicDecoder::BitonicDecoder(std::span<uint8_t> input) {
  assert(input.size() >= 4);
  cursor_ = 0;
  state_ = 0;
  for (int i = 0; i < 4; i++) {
    state_ = (state_ << 8) | uint32_t(input[cursor_]);
    cursor_++;
  }
  low_ = 0;
  high_ = 0xffffffff;
  input_ = input;
}

/// Decode one bit with a probability 'prob' in the range 0..65536.
std::optional<bool> BitonicDecoder::decode(uint16_t prob) {
  assert(high_ > low_);
  assert(high_ >= state_ && low_ <= state_);
  assert(cursor_ <= input_.size());

  // Figure out the mid point of the range, depending on the probability.
  uint64_t gap = (high_ - low_);
  uint64_t scale = (gap * uint64_t(prob)) >> 16;
  uint32_t mid = low_ + uint32_t(scale);
  assert(high_ > mid && mid >= low_);

  // Figure out which bit we extract based on where the state falls in the
  // range.
  bool bit = state_ <= mid;

  // Select the sub-range based on the bit.
  if (bit) {
    high_ = mid;
  } else {
    low_ = mid + 1;
  }

  // Clear the identical leading bytes.
  while ((high_ ^ low_) < (1 << 24)) {
    // Not enough bits in the input.
    if (cursor_ == input_.size()) {
      return std::nullopt;
    }
    high_ = (high_ << 8) + 0xff;
    low_ <<= 8;
    state_ = (state_ << 8) + uint32_t(input_[cursor_]);
    cursor_ += 1;
  }

  return bit;
}

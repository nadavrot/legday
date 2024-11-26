#include "legday.h"

#include <cassert>
#include <stdio.h>

static constexpr uint8_t nthbit(uint8_t byte, size_t n) {
  return (byte >> n) & 1;
}

legday::BitonicEncoder::BitonicEncoder(std::vector<uint8_t> &output)
    : low_(0), high_(0xffffffff), output_(output) {}

size_t legday::BitonicEncoder::encode(bool bit, uint16_t prob) {
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

size_t legday::BitonicEncoder::finalize() {
  // Encode a zero-probability token which flushes the state.
  return encode(true, 0);
}

legday::BitonicDecoder::BitonicDecoder(std::span<uint8_t> input) {
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

std::optional<bool> legday::BitonicDecoder::decode(uint16_t prob) {
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

float get_correlation_between_bits(std::span<uint8_t> input, int i, int j) {
  legday::Stream<16> stream(input);
  if (i < j) {
    return 0;
  }
  uint32_t correlation = 0;
  for (size_t k = 0; k < stream.size(); k++) {
    correlation += stream.get(k, i) == stream.get(k, j);
  }

  return float(correlation) / float(stream.size());
}

void print_correlation_matrix(std::span<uint8_t> input) {
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 10; j++) {
      printf("(%d, %d) %0.2f ", i, j,
             get_correlation_between_bits(input, i, j));
    }
    printf("\n");
  }
}

float get_correlation_between_consecutive_bits(std::span<uint8_t> input,
                                               int i) {
  bool prev = false;
  legday::Stream<16> stream(input);

  uint32_t correlation = 0;
  for (size_t k = 0; k < stream.size(); k++) {
    bool bit = stream.get(k, i);
    correlation += prev == bit;
    prev = bit;
  }

  return float(correlation) / float(stream.size());
}

void print_correlation_previous_bit(std::span<uint8_t> input) {
  for (int i = 0; i < 16; i++) {
    printf("%0.2f ", get_correlation_between_consecutive_bits(input, i));
  }
  printf("\n");
}

void add_bias_to_second_byte(std::span<uint8_t> input, uint8_t bias) {
  // Xor the low exponent bit with the highest mantissa bit.
  for (size_t i = 0; i < input.size() / 2; i++) {
    unsigned low_exp = ((input[i * 2 + 1] >> 1) & 0x1);
    input[i * 2] ^= low_exp << 7;
  }

  for (size_t i = 0; i < input.size() / 2; i++) {
    input[i * 2 + 1] += bias;
  }
}

void legday::try_compress(std::span<uint8_t> input) {
  constexpr int CHANNELS = 16;

  add_bias_to_second_byte(input, 127);
  print_correlation_previous_bit(input);
  print_correlation_matrix(input);

  assert(input.size() % CHANNELS == 0);
  std::vector<uint8_t> output;
  Stream<CHANNELS>::ArrayPopcntTy ones;
  Stream<CHANNELS> stream(input);
  stream.popcnt(ones);
  unsigned prev = 0;
  for (int i = 0; i < CHANNELS; i++) {
    BitonicEncoder encoder(output);
    uint16_t prob = uint16_t((ones[i] * 65535) / stream.size());

    for (size_t j = 0; j < stream.size(); j++) {
      bool bit = stream.get(j, i);
      encoder.encode(bit, prob);
    }
    encoder.finalize();
    printf("Channel %d: %lu/%zu (%d - %f) - %lu\n", i, ones[i], stream.size(),
           prob, prob / 65535.0, output.size() - prev);
    prev = output.size();
  }

  printf("Compressed %zu bytes to %zu bytes\n", input.size(), output.size());
}

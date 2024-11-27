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

static float get_correlation_between_bits(std::span<uint8_t> input, int i,
                                          int j) {
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

static void print_correlation_matrix(std::span<uint8_t> input) {
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 10; j++) {
      printf("(%d, %d) %0.2f ", i, j,
             get_correlation_between_bits(input, i, j));
    }
    printf("\n");
  }
}

static float get_correlation_between_consecutive_bits(std::span<uint8_t> input,
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

static void print_correlation_previous_bit(std::span<uint8_t> input) {
  for (int i = 0; i < 16; i++) {
    printf("%0.2f ", get_correlation_between_consecutive_bits(input, i));
  }
  printf("\n");
}

void legday::transform_bf16_buffer(std::span<uint8_t> input, bool forward) {
  // Xor the low exponent bit with the highest mantissa bit.
  for (size_t i = 0; i < input.size() / 2; i++) {
    if (forward) {
      unsigned low_exp = ((input[i * 2 + 1] >> 1) & 0x1);
      input[i * 2] ^= low_exp << 7;
      input[i * 2 + 1] += 127; // Bias the exponent.
    } else {
      input[i * 2 + 1] -= 127; // Un-bias the exponent.
      unsigned low_exp = ((input[i * 2 + 1] >> 1) & 0x1);
      input[i * 2] ^= low_exp << 7;
    }
  }
}

template <unsigned CHANNELS>
void compress_impl(std::span<uint8_t> input, std::vector<uint8_t> &output) {
  assert(input.size() % CHANNELS == 0);

  legday::push<uint8_t>(output, CHANNELS);
  legday::push<uint32_t>(output, (input.size() * 8) / CHANNELS);

  typename legday::Stream<CHANNELS>::ArrayPopcntTy ones;
  legday::Stream<CHANNELS> stream(input);
  stream.popcnt(ones);

  uint16_t prob[CHANNELS] = {0};
  for (int i = 0; i < CHANNELS; i++) {
    prob[i] = uint16_t((ones[i] * 65535) / stream.size());
  }

  // FORMAT: One uint16_t per channel, the probability of a bit being 1.
  for (int i = 0; i < CHANNELS; i++) {
    legday::push<uint16_t>(output, prob[i]);
  }

  for (int i = 0; i < CHANNELS; i++) {
    legday::BitonicEncoder encoder(output);

    for (size_t j = 0; j < stream.size(); j++) {
      bool bit = stream.get(j, i);
      encoder.encode(bit, prob[i]);
    }
    encoder.finalize();
  }
}

std::vector<uint8_t> legday::compress(std::span<uint8_t> input,
                                      legday::Layout layout) {
  std::vector<uint8_t> output;
  legday::push<uint32_t>(output, 'LGDY');
  legday::push<uint8_t>(output, layout);

  switch (layout) {
  case legday::Layout::BF16: {
    transform_bf16_buffer(input, true);
    compress_impl<16>(input, output);
    transform_bf16_buffer(input, false); // Undo the transformation.
    break;
  }
  case legday::Layout::FP16: {
    compress_impl<16>(input, output);
    break;
  }
  case legday::Layout::FP32: {
    compress_impl<32>(input, output);
    break;
  }
  case legday::Layout::FP8: {
    compress_impl<8>(input, output);
    break;
  }
  case legday::Layout::INT8: {
    compress_impl<8>(input, output);
    break;
  }
  }

  return output;
}

template <unsigned CHANNELS>
std::vector<uint8_t> decompress_impl(std::span<uint8_t> input, size_t words) {
  // Read the probability of each channel.
  uint16_t prob[CHANNELS] = {0};
  for (int i = 0; i < CHANNELS; i++) {
    prob[i] = legday::read<uint16_t>(input, 0);
    input = input.subspan(2);
  }

  std::vector<uint8_t> output(words * (CHANNELS / 8), 0);
  legday::Stream<CHANNELS> stream(output);

  for (int i = 0; i < CHANNELS; i++) {
    legday::BitonicDecoder decoder(input);

    for (size_t w = 0; w < words; w++) {
      auto bit = decoder.decode(prob[i]);
      assert(bit.has_value());
      stream.set(w, i, bit.value());
    }
    input = input.subspan(decoder.consumed());
  }

  return output;
}

std::vector<uint8_t> legday::decompress(std::span<uint8_t> input) {
  // FORMAT: Magic, Kind, num_channels, num_words.
  uint32_t magic = legday::read<uint32_t>(input, 0);
  uint8_t kind = legday::read<uint8_t>(input, 4);
  uint8_t channels = legday::read<uint8_t>(input, 5);
  uint32_t words = legday::read<uint32_t>(input, 6);
  assert(magic == 'LGDY');
  input = input.subspan(10);

  switch (kind) {
  case legday::Layout::BF16: {
    std::vector<uint8_t> output = decompress_impl<16>(input, words);
    transform_bf16_buffer(output, false);
    return output;
  }
  case legday::Layout::FP16:
    return decompress_impl<16>(input, words);
  case legday::Layout::FP32:
    return decompress_impl<32>(input, words);
  case legday::Layout::FP8:
    return decompress_impl<8>(input, words);
  case legday::Layout::INT8:
    return decompress_impl<8>(input, words);
  default:
    assert(false);
  }
}

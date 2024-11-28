#include "legday.h"
#include "legday_impl.h"

#include <cassert>
#include <stdio.h>

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

void legday::transform_buffer_offset(std::span<uint8_t> input, uint8_t stride,
                                     uint8_t offset, uint8_t value) {
  assert(input.size() % stride == 0);
  assert(offset < stride);
  for (size_t i = 0; i < input.size(); i += stride) {
    input[i + offset] += value;
  }
}

template <unsigned CHANNELS>
void compress_impl(std::span<uint8_t> input, std::vector<uint8_t> &output) {
  assert(input.size() % CHANNELS == 0);
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

template <unsigned CHANNELS>
static uint8_t pick_best_transform_parameter(std::span<uint8_t> input,
                                             uint8_t stride, uint8_t offset) {
  constexpr int test_buffer_size = 1<<16;
  std::span<uint8_t> small = input;
  if (small.size() > test_buffer_size) {
    small = small.first(test_buffer_size);
  }

  std::vector<uint8_t> output;
  uint32_t best = 0xffffffff;
  uint8_t best_param = 0;
  for (int i = 0; i < 255; i++) {
    legday::transform_buffer_offset(small, stride, offset, i);
    compress_impl<16>(small, output);
    legday::transform_buffer_offset(small, stride, offset, -i);

    if (output.size() < best) {
      best = output.size();
      best_param = i;
    }
    output.clear();
  }
  return best_param;
}

std::vector<uint8_t> legday::compress(std::span<uint8_t> input,
                                      legday::Layout layout) {
  std::vector<uint8_t> output;
  legday::push<uint32_t>(output, 'LGDY');
  legday::push<uint8_t>(output, layout);

  switch (layout) {
  case legday::Layout::BF16: {
    uint8_t tr_param = pick_best_transform_parameter<16>(input, 2, 1);
    legday::push<uint8_t>(output, tr_param);
    transform_buffer_offset(input, 2, 1, tr_param);
    compress_impl<16>(input, output);
    transform_buffer_offset(input, 2, 1, -tr_param);
    break;
  }
  case legday::Layout::FP32: {
    uint8_t tr_param = pick_best_transform_parameter<16>(input, 2, 1);
    legday::push<uint8_t>(output, tr_param);
    transform_buffer_offset(input, 4, 3, tr_param);
    compress_impl<32>(input, output);
    transform_buffer_offset(input, 4, 3, -tr_param);
    break;
  }
  case legday::Layout::INT8: {
    legday::push<uint8_t>(output, 0);
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
  uint8_t tr_param = legday::read<uint8_t>(input, 5);
  uint32_t words = legday::read<uint32_t>(input, 6);
  assert(magic == 'LGDY');
  input = input.subspan(10);

  switch (kind) {
  case legday::Layout::BF16: {
    std::vector<uint8_t> output = decompress_impl<16>(input, words);
    transform_buffer_offset(output, 2, 1, -tr_param);
    return output;
  }
  case legday::Layout::FP32: {
    std::vector<uint8_t> output = decompress_impl<32>(input, words);
    transform_buffer_offset(output, 4, 3, -tr_param);
    return output;
  }
  case legday::Layout::INT8:
    return decompress_impl<8>(input, words);
  default:
    assert(false);
  }
}

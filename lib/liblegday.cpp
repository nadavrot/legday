#include "legday.h"

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

void legday::transform_bf16_buffer(std::span<uint8_t> input, bool forward) {
  for (size_t i = 0; i < input.size() / 2; i++) {
    if (forward) {
      // Xor the low exponent bit with the highest mantissa bit.
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

void legday::transform_f32_buffer(std::span<uint8_t> input, bool forward) {

  legday::Stream<32> stream(input);
  for (size_t i = 0; i < stream.size(); i++) {
    bool b21 = stream.get(i, 21);
    bool b22 = stream.get(i, 22);
    bool b23 = stream.get(i, 23);
    bool b24 = stream.get(i, 24);
    bool b25 = stream.get(i, 25);
    bool b26 = stream.get(i, 26);
    bool b27 = stream.get(i, 27);
    stream.set(i, 21, b27 ^ b21);
    stream.set(i, 22, b27 ^ b22);
    stream.set(i, 23, b27 ^ b23);
    stream.set(i, 24, b27 ^ b24);
    stream.set(i, 25, b27 ^ b25);
    stream.set(i, 26, b27 ^ b26);
  }
}

/*void detect_correlation(std::span<uint8_t> input) {
  legday::Stream<32> stream(input);

  for (int i = 1; i < 32; i++) {
    for (int j = i+1; j < 32; j++) {
      uint64_t ones = 0;
      uint64_t cnt = 0;
      for (int k = 0; k < stream.size(); k++) {
        if (stream.get(k, i) == stream.get(k, j)) {
          ones += 1;
        }
        cnt += 1;
      }
      double p = double(ones) / double(cnt);
      if (ones == 0 || ones == cnt) {
        continue;
      }
      if (p > 0.52 || p < 0.48) {
        printf("Correlation: (%d %d) %02f\n", i, j, p);
      }
    }
  }
}*/

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
    transform_f32_buffer(input, true);
    compress_impl<32>(input, output);
    transform_f32_buffer(input, false); // Undo the transformation.
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
  uint32_t words = legday::read<uint32_t>(input, 5);
  assert(magic == 'LGDY');
  input = input.subspan(9);

  switch (kind) {
  case legday::Layout::BF16: {
    std::vector<uint8_t> output = decompress_impl<16>(input, words);
    transform_bf16_buffer(output, false);
    return output;
  }
  case legday::Layout::FP16:
    return decompress_impl<16>(input, words);
  case legday::Layout::FP32: {
    std::vector<uint8_t> output = decompress_impl<32>(input, words);
    transform_f32_buffer(output, false);
    return output;
  }
  case legday::Layout::FP8:
    return decompress_impl<8>(input, words);
  case legday::Layout::INT8:
    return decompress_impl<8>(input, words);
  default:
    assert(false);
  }
}

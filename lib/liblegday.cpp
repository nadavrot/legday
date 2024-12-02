#include "legday.h"
#include "legday_impl.h"

#include <algorithm>
#include <cassert>
#include <stdio.h>

using namespace legday;

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
    state_ = (state_ << 8) | uint32_t(input[cursor_++]);
  }
  low_ = 0;
  high_ = 0xffffffff;
  input_ = input;
}

std::optional<bool> legday::BitonicDecoder::decode(uint16_t prob) {
  assert(high_ > low_ && high_ >= state_ && low_ <= state_);
  assert(cursor_ <= input_.size());

  // Figure out the mid point of the range, depending on the probability.
  uint64_t gap = (high_ - low_);
  uint64_t scale = (gap * uint64_t(prob)) >> 16;
  uint32_t mid = low_ + uint32_t(scale);
  assert(high_ > mid && mid >= low_);

  // Compute the next bit based on where the state falls in the range.
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

void legday::rotate_b16(std::span<uint8_t> input, uint8_t n) {
  assert(input.size() % 2 == 0);
  for (size_t i = 0; i < input.size(); i += 2) {
    uint16_t value = uint16_t(input[i + 1]) | (uint16_t(input[i]) << 8);
    value = (value >> n) | (value << (16 - n));
    input[i] = uint8_t(value >> 8);
    input[i + 1] = uint8_t(value);
  }
}

template <unsigned CHANNELS>
void compress_impl(std::span<uint8_t> input, std::vector<uint8_t> &output) {
  assert((input.size() * 8) % CHANNELS == 0);

  typename Stream<CHANNELS>::ArrayPopcntTy ones;
  Stream<CHANNELS> stream(input);
  stream.popcnt(ones);

  // Save uint16_t per channel (the probability of a bit being 1).
  uint16_t prob[CHANNELS] = {0};
  for (int i = 0; i < CHANNELS; i++) {
    prob[i] = uint16_t((ones[i] * 65535) / stream.size());
    push<uint16_t>(output, prob[i]);
  }

  for (int i = 0; i < CHANNELS; i++) {
    BitonicEncoder encoder(output);

    for (size_t j = 0; j < stream.size(); j++) {
      bool bit = stream.get(j, i);
      encoder.encode(bit, prob[i]);
    }
    encoder.finalize();
  }
}

static void decode_symbols(std::span<uint8_t> input, std::span<uint8_t> decoder,
                           uint8_t stride, uint8_t offset) {
  for (int i = 0; i < input.size(); i += stride) {
    input[i + offset] = decoder[input[i + offset]];
  }
}

static void sort_symbols(std::span<uint8_t> input, std::span<uint8_t> decoder,
                         uint8_t stride, uint8_t offset) {
  assert(decoder.size() == 256);
  uint32_t hist[256] = {0};
  uint8_t lut[256] = {0};

  for (int i = 0; i < input.size(); i += stride) {
    hist[input[i + offset]] += 1;
  }

  for (int i = 0; i < 256; i++) {
    decoder[i] = i;
  }
  std::sort(decoder.begin(), decoder.end(),
            [&hist](uint8_t a, uint8_t b) { return hist[a] > hist[b]; });

  for (int i = 0; i < 256; i++) {
    lut[decoder[i]] = i;
  }

  for (int i = 0; i < input.size(); i += stride) {
    input[i + offset] = lut[input[i + offset]];
  }
}

std::vector<uint8_t> legday::compress(std::span<uint8_t> input, Layout layout) {
   uint8_t decoder[256];
  std::vector<uint8_t> output;
  push<uint32_t>(output, 0x474c5944); // Magic
  push<uint8_t>(output, layout);      // Kind
  switch (layout) {
  case Layout::BF16: {
    push<uint32_t>(output, input.size()/2); // Words
    rotate_b16(input, 15);
    sort_symbols(input, decoder, 2, 1);
    output.insert(output.end(), decoder, decoder + 256);
    compress_impl<16>(input, output);
    break;
  }
  case Layout::FP32: {
    push<uint32_t>(output, input.size()/4); // Words
    rotate_b16(input, 15);   
    sort_symbols(input, decoder, 4, 3);
    output.insert(output.end(), decoder, decoder + 256);
    compress_impl<32>(input, output);
    break;
  }
  case Layout::INT8: {
    push<uint32_t>(output, input.size()); // Words
    compress_impl<8>(input, output);
    break;
  }
  }

  return output;
}

template <unsigned CHANNELS>
std::vector<uint8_t> decompress_impl(std::span<uint8_t> input, size_t words) {
  uint16_t prob[CHANNELS] = {0};
  for (int i = 0; i < CHANNELS; i++) {
    prob[i] = read<uint16_t>(input, 0);
    input = input.subspan(2);
  }

  std::vector<uint8_t> output(words * (CHANNELS / 8), 0);
  Stream<CHANNELS> stream(output);

  for (int i = 0; i < CHANNELS; i++) {
    BitonicDecoder decoder(input);

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
  uint32_t magic = read<uint32_t>(input, 0);  // Magic
  uint8_t kind = read<uint8_t>(input, 4);     // Kind
  uint32_t words = read<uint32_t>(input, 5);  // Number of words
  assert(magic == 0x474c5944);
  input = input.subspan(9);

  switch (kind) {
  case Layout::BF16: {
    std::span<uint8_t> decoder(input.data(), 256);
    input = input.subspan(256);
    std::vector<uint8_t> output = decompress_impl<16>(input, words);
    decode_symbols(output, decoder, 2, 1);
    rotate_b16(output, 1);
    return output;
  }
  case Layout::FP32: {
    std::span<uint8_t> decoder(input.data(), 256);
    input = input.subspan(256);
    std::vector<uint8_t> output = decompress_impl<32>(input, words);
    decode_symbols(output, decoder, 4, 3);
    rotate_b16(output, 1);
    return output;
  }
  case Layout::INT8:
    return decompress_impl<8>(input, words);
  default:
    assert(false);
  }
}

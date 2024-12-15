#include "legday.h"
#include "legday_impl.h"
#include <gtest/gtest.h>

using namespace legday;

TEST(LegdayTest, PushPop0) {
  std::vector<uint8_t> buffer;
  for (int i = 0; i < 1000; i++) {
    push<uint16_t>(buffer, i);
    EXPECT_EQ(read<uint16_t>(buffer, 0), i);
    buffer.clear();
  }
}

TEST(LegdayTest, TestStream0) {
  uint8_t buffer[8] = {1, 2, 4, 8, 16, 32, 64, 128};
  Stream<8> stream(buffer);

  EXPECT_EQ(stream.get(0, 0), 1);
  EXPECT_EQ(stream.get(0, 1), 0);
  EXPECT_EQ(stream.get(0, 2), 0);
  EXPECT_EQ(stream.get(0, 3), 0);
  EXPECT_EQ(stream.get(1, 0), 0);
  EXPECT_EQ(stream.get(1, 1), 1);
  EXPECT_EQ(stream.get(1, 2), 0);
  EXPECT_EQ(stream.get(1, 3), 0);

  Stream<16> stream2(buffer);
  EXPECT_EQ(stream2.get(0, 0), 1);
  EXPECT_EQ(stream2.get(0, 1), 0);
  EXPECT_EQ(stream2.get(0, 8), 0);
  EXPECT_EQ(stream2.get(0, 9), 1);
  EXPECT_EQ(stream2.get(1, 0), 0);
  EXPECT_EQ(stream2.get(1, 2), 1);
}

TEST(LegdayTest, TestStreamSlice0) {
  uint8_t buffer[8] = {0x34, 0xff, 0xaa, 0x77};
  Stream<16> stream(buffer);

  // Basic stuff:
  EXPECT_EQ(stream.get_bits_before<4>(0, 4), 0x4);
  EXPECT_EQ(stream.get_bits_before<4>(0, 0), 0x0);
  EXPECT_EQ(stream.get_bits_before<4>(0, 8), 0x3);
  EXPECT_EQ(stream.get_bits_before<8>(0, 4), 0x40);
  EXPECT_EQ(stream.get_bits_before<2>(0, 3), 0x2);
  // Other:
  EXPECT_EQ(stream.get_bits_before<6>(0, 8) >> 2, 0x3);
  EXPECT_EQ(stream.get_bits_before<4>(0, 12), 0xf);
  EXPECT_EQ(stream.get_bits_before<2>(0, 14), 0x3);
  EXPECT_EQ(stream.get_bits_before<4>(1, 8), 0xa);
  // Cross nibble.
  EXPECT_EQ(stream.get_bits_before<3>(1, 11), 0x7);
  EXPECT_EQ(stream.get_bits_before<3>(1, 12), 0x3);
  EXPECT_EQ(stream.get_bits_before<3>(1, 13), 0x5);
  EXPECT_EQ(stream.get_bits_before<3>(1, 14), 0x6);
  //  Cross byte.
  EXPECT_EQ(stream.get_bits_before<8>(0, 12), 0xf3);
  EXPECT_EQ(stream.get_bits_before<8>(1, 12), 0x7a);
}

TEST(LegdayTest, BF16Transform0) {
  {
    uint8_t buffer[2] = {0x80, 0x01};
    rotate_b16(buffer, 1);
    EXPECT_EQ(buffer[0], 0xc0);
    EXPECT_EQ(buffer[1], 0x00);
    rotate_b16(buffer, 15);
    EXPECT_EQ(buffer[0], 0x80);
    EXPECT_EQ(buffer[1], 0x01);
  }
  {
    uint16_t buffer[1] = {0x80};
    std::span<uint8_t> span((uint8_t *)buffer, 2);
    rotate_b16(span, 1);
    EXPECT_EQ(buffer[0], 0x40);
  }
}

TEST(LegdayTest, BasicEncoders0) {
  std::vector<uint8_t> buffer;
  BitonicEncoder encoder(buffer);
  for (int i = 0; i < 1000; i++) {
    encoder.encode(i % 2, 30000);
  }
  encoder.finalize();
  BitonicDecoder decoder(buffer);
  for (int i = 0; i < 1000; i++) {
    auto bit = decoder.decode(30000);
    EXPECT_EQ(bit.has_value(), true);
    EXPECT_EQ(bit.value(), i % 2);
  }
}

TEST(LegdayTest, EncoderDecoder0) {
  bool buffer[24] = {
      1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 1, 0, 1,
  };
  std::vector<uint8_t> coded;
  BitonicEncoder encoder(coded);

  for (bool bit : buffer) {
    encoder.encode(bit, 30000);
  }
  encoder.finalize();

  BitonicDecoder decoder(coded);
  for (bool bit : buffer) {
    auto bit2 = decoder.decode(30000);
    EXPECT_EQ(bit2.has_value(), true);
    EXPECT_EQ(bit2.value(), bit);
  }
}

TEST(LegdayTest, RoundTrip0) {
  uint8_t buffer[2] = {
      0x60,
      0x59,
  };
  std::vector<uint8_t> compressed = compress(buffer, Layout::INT8);
  std::vector<uint8_t> decompressed = decompress(compressed);
  EXPECT_EQ(decompressed, std::vector<uint8_t>(buffer, buffer + 2));
}

TEST(LegdayTest, RoundTrip1) {
  uint8_t buffer[64] = {
      0x60, 0x59, 0x24, 0xd1, 0xc1, 0x94, 0x16, 0xf8, 0xcc, 0x92, 0x7f,
      0x90, 0x57, 0xca, 0xe3, 0x91, 0x60, 0x59, 0x24, 0xd1, 0xc1, 0x94,
      0x16, 0xf8, 0xcc, 0x92, 0x7f, 0x90, 0x57, 0xca, 0xe3, 0x91, 0x60,
      0x59, 0x24, 0xd1, 0xc1, 0x94, 0x16, 0xf8, 0xcc, 0x92, 0x7f, 0x90,
      0xff, 0xff, 0xff, 0xff, 0x60, 0x59, 0x24, 0xd1, 0xc1, 0x94, 0x16,
      0xf8, 0xcc, 0x92, 0x7f, 0x90, 0xaa, 0xaa, 0xaa, 0xaa,
  };
  std::vector<uint8_t> compressed = compress(buffer, Layout::INT8);
  std::vector<uint8_t> decompressed = decompress(compressed);
  EXPECT_EQ(decompressed, std::vector<uint8_t>(buffer, buffer + 64));
}
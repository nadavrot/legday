
#include "legday.h"
#include <gtest/gtest.h>

using namespace legday;

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

TEST(LegdayTest, TestStream1) {
  uint8_t buffer[4] = {
      1,
      0,
      3,
      1,
  };
  Stream<8> stream(buffer);

  Stream<8>::ArrayPopcntTy ones;
  stream.popcnt(ones);
  EXPECT_EQ(ones[0], 3);
  EXPECT_EQ(ones[1], 1);
  EXPECT_EQ(ones[2], 0);
  EXPECT_EQ(ones[3], 0);

  Stream<16> stream2(buffer);

  Stream<16>::ArrayPopcntTy ones2;
  stream2.popcnt(ones2);
  EXPECT_EQ(ones2[0], 2);
  EXPECT_EQ(ones2[1], 1);
  EXPECT_EQ(ones2[8], 1);
  EXPECT_EQ(ones2[9], 0);
}

TEST(LegdayTest, PopCnt0) {
  EXPECT_EQ(popcnt(0), 0);
  EXPECT_EQ(popcnt(1), 1);
  EXPECT_EQ(popcnt(2), 1);
  EXPECT_EQ(popcnt(3), 2);
  EXPECT_EQ(popcnt(0xff), 8);
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
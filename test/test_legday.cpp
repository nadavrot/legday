
#include <gtest/gtest.h>
#include "legday.h"

int add(int a, int b)
{
  return a + b;
}

TEST(LegdayTest, BasicTransposeTest0)
{
  uint8_t buffer[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  std::vector<uint8_t> transposed = legday::transpose(buffer);
  std::vector<uint8_t> orginal = legday::untranspose(transposed);

  EXPECT_EQ(transposed.size(), orginal.size());
  EXPECT_EQ(orginal.size(), 8);

  EXPECT_EQ(transposed[0], 0xff);
  EXPECT_EQ(transposed[1], 0x00);

  for (int i = 0; i < 8; i++)
  {
    EXPECT_EQ(buffer[i], orginal[i]);
  }
}

TEST(LegdayTest, BasicTransposeTest1)
{
  uint8_t buffer[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  std::vector<uint8_t> transposed = legday::transpose(buffer);
  std::vector<uint8_t> orginal = legday::untranspose(transposed);

  EXPECT_EQ(transposed.size(), orginal.size());
  EXPECT_EQ(orginal.size(), 8);

  for (int i = 0; i < 8; i++)
  {
    EXPECT_EQ(buffer[i], orginal[i]);
  }
}

TEST(LegdayTest, BasicTransposeTest2)
{
  uint8_t buffer[16] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7};
  std::vector<uint8_t> transposed = legday::transpose(buffer);
  std::vector<uint8_t> orginal = legday::untranspose(transposed);

  EXPECT_EQ(transposed.size(), orginal.size());
  EXPECT_EQ(orginal.size(), 16);

  for (int i = 0; i < orginal.size(); i++)
  {
    EXPECT_EQ(buffer[i], orginal[i]);
  }

  EXPECT_EQ(transposed[0], transposed[1]);
}

TEST(LegdayTest, BasicTransposeTest3)
{
  uint8_t buffer[16] = {
      0,
  };

  for (size_t iter = 0; iter < 5000; iter++)
  {

    // Randomize buffer:
    for (int i = 0; i < 16; i++)
    {
      buffer[i] = ((iter * 7141) + i * 13) & 0xff;
    }

    std::vector<uint8_t> transposed = legday::transpose(buffer);
    std::vector<uint8_t> orginal = legday::untranspose(transposed);

    for (int i = 0; i < orginal.size(); i++)
    {
      EXPECT_EQ(buffer[i], orginal[i]);
    }
  }
}

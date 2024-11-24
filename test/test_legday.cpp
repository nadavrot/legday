
#include <gtest/gtest.h>
#include "legday.h"

int add(int a, int b) {
  return a+b;
}

TEST(LegdayTest, BasicTest) {
  // Your test cases here
  EXPECT_EQ(2, add(1, 1));
}

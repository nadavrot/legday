
#include "legday.h"

#include <cassert>

static constexpr uint8_t nthbit(uint8_t byte, size_t n)
{
  return (byte >> n) & 1;
}

std::vector<uint8_t> legday::transpose(std::span<uint8_t> buffer)
{
  assert(buffer.size() % 8 == 0);
  std::vector<uint8_t> result(buffer.size(), 0);
  // The transposed buffer is split into 8 parts.
  size_t num_parts = buffer.size() / 8;

  // For each destination byte in a part of the transposed buffer.
  for (size_t dest = 0; dest < num_parts; dest++)
  {
    // The source offset is multiplied by 8 because we pack 8 bytes at once.
    size_t src = dest * 8;

    // For each bit in the source bytes.
    for (int bit = 0; bit < 8; bit++)
    {
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

std::vector<uint8_t> legday::untranspose(std::span<uint8_t> in)
{
  assert(in.size() % 8 == 0);
  std::vector<uint8_t> result(in.size(), 0);

  // The transposed buffer is split into 8 parts.
  size_t num_parts = in.size() / 8;

  // For each destination byte in a part of the transposed buffer.
  for (size_t dest = 0; dest < num_parts; dest++)
  {
    // For each bit:
    for (int bit = 0; bit < 8; bit++)
    {
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

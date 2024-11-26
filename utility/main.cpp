#include "legday.h"

#include <fstream>
#include <iostream>
#include <vector>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <filename>\n";
    return 1;
  }

  std::ifstream input_file(argv[1], std::ios::binary);

  if (!input_file) {
    std::cerr << "Error: could not open file '" << argv[1] << "'\n";
    return 1;
  }

  // Read the contents of the file into a vector of bytes
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input_file)),
                             std::istreambuf_iterator<char>());

  std::vector<uint8_t> compressed = legday::compress(bytes);
  std::vector<uint8_t> decompressed = legday::decompress(compressed);

  // Check that the decompressed data matches the original.
  if (bytes != decompressed) {
    std::cerr << "Error: decompressed data does not match original\n";
    return 1;
  }

  std::cout << "Success\n";

  return 0;
}

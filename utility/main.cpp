#include "legday.h"

#include <fstream>
#include <iostream>
#include <vector>

int main(int argc, char *argv[]) {
  if (argc < 4) {
    std::cerr << "Usage: " << argv[0]
              << " [compress|decompress|verify] <input> <output>\n";
    return 1;
  }

  bool compress = false;
  bool verify = false;
  if (std::string(argv[1]) == "compress") {
    compress = true;
  } else if (std::string(argv[1]) == "decompress") {
    compress = false;
  } else if (std::string(argv[1]) == "verify") {
    compress = true;
    verify = true;
  } else {
    std::cerr << "Error: unknown command '" << argv[1] << "'\n";
    return 1;
  }

  std::ifstream input_file(argv[2], std::ios::binary);

  if (!input_file) {
    std::cerr << "Error: could not open file '" << argv[2] << "'\n";
    return 1;
  }

  // Read the contents of the file into a vector of bytes
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input_file)),
                             std::istreambuf_iterator<char>());

  std::vector<uint8_t> output;

  if (compress) {
    output = legday::compress(bytes, legday::Layout::BF16);
    std::cout << "Compressed " << bytes.size() << " bytes to " << output.size()
              << " bytes ("
              << (100.0 * float(output.size()) / float(bytes.size())) << "%)\n";

    if (verify) {
      auto decompressed = legday::decompress(output);
      if (decompressed != bytes) {
        std::cerr << "Error: verification failed\n";
        return 1;
      }

      std::cout << "Verification succeeded\n";
    }
  } else {
    output = legday::decompress(bytes);
  }
  std::ofstream output_file(argv[3], std::ios::binary);
  if (!output_file) {
    std::cerr << "Error: could not open file '" << argv[3] << "'\n";
    return 1;
  }
  output_file.write(reinterpret_cast<char *>(output.data()), output.size());
  return 0;
}

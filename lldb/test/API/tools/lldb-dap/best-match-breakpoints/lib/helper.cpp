#include "helper.h"
#include <iostream>

namespace helper {

int computeValue(int x) {
  // Multiply by 10
  return x * 10;
}

void printHelperMessage() {
  std::cout << "Helper library loaded successfully" << std::endl;
}

} // namespace helper

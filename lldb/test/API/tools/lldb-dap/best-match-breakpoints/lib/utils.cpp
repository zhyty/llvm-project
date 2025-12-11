#include <iostream>

namespace helper {

void utilsFunction() {
  std::cout << "This is utils.cpp from the LIBRARY" << std::endl;
  int breakpoint_here = 42; // Set breakpoint here to verify lib/utils.cpp
  std::cout << "Library utils value: " << breakpoint_here << std::endl;
}

} // namespace helper

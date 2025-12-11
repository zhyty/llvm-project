#include <iostream>

namespace main_ns {

void utilsFunction() {
  std::cout << "This is utils.cpp from the MAIN executable" << std::endl;
  int breakpoint_here = 99; // Set breakpoint here to verify main/utils.cpp
  std::cout << "Main utils value: " << breakpoint_here << std::endl;
}

} // namespace main_ns

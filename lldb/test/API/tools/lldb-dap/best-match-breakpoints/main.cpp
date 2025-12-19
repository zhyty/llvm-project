#include <cassert>
#include <dlfcn.h>
#include <iostream>

namespace main_ns {
// In utils.cpp
void utilsFunction();
}

// Function pointer types for the library functions
typedef void (*HelperUtilsFuncType)();

int main() {
  // BREAK BEFORE .SO LOAD
  std::cout << "Before loading the dynamic library." << std::endl;

  void *handle = dlopen("lib/libhelper.so", RTLD_NOW);
  assert(handle && "Failed to load library");

  // BREAK AFTER .SO LOAD
  std::cout <<"After loading the dynamic library." << std::endl;

  auto helperUtilsFunction = reinterpret_cast<HelperUtilsFuncType>(
      dlsym(handle, "_ZN6helper13utilsFunctionEv"));
  assert(helperUtilsFunction && "Failed to load utilsFunction from helper so");

  std::cout << "\nCalling main exec's utils.cpp:" << std::endl;
  main_ns::utilsFunction();

  std::cout << "\nCalling lib/utils.cpp:" << std::endl;
  helperUtilsFunction();

  dlclose(handle);

  return 0;
}

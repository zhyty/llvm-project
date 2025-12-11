#include <dlfcn.h>
#include <iostream>

namespace main_ns {
void utilsFunction();
}

// Function pointer types for the library functions
typedef int (*ComputeValueFunc)(int);
typedef void (*PrintHelperMessageFunc)();
typedef void (*UtilsFunctionFunc)();

int main() {
  // Open the shared library
  void *handle = dlopen("lib/libhelper.so", RTLD_NOW);
  if (handle == nullptr) {
    std::cerr << "Error loading library: " << dlerror() << std::endl;
    return 1;
  }

  // Clear any existing errors
  dlerror();

  // Load function symbols from the library
  auto computeValue = reinterpret_cast<ComputeValueFunc>(
      dlsym(handle, "_ZN6helper12computeValueEi"));
  if (!computeValue) {
    std::cerr << "Error loading computeValue: " << dlerror() << std::endl;
    dlclose(handle);
    return 1;
  }

  auto printHelperMessage = reinterpret_cast<PrintHelperMessageFunc>(
      dlsym(handle, "_ZN6helper18printHelperMessageEv"));
  if (!printHelperMessage) {
    std::cerr << "Error loading printHelperMessage: " << dlerror() << std::endl;
    dlclose(handle);
    return 1;
  }

  auto helperUtilsFunction = reinterpret_cast<UtilsFunctionFunc>(
      dlsym(handle, "_ZN6helper13utilsFunctionEv"));
  if (!helperUtilsFunction) {
    std::cerr << "Error loading utilsFunction: " << dlerror() << std::endl;
    dlclose(handle);
    return 1;
  }

  // Call helper library functions via function pointers
  printHelperMessage();
  int result = computeValue(5);
  std::cout << "Computed value: " << result << std::endl;

  // Call utils functions from both locations
  std::cout << "\nCalling main/utils.cpp:" << std::endl;
  main_ns::utilsFunction();

  std::cout << "\nCalling lib/utils.cpp:" << std::endl;
  helperUtilsFunction();

  // Close the library
  dlclose(handle);

  return 0;
}

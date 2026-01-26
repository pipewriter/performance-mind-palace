# Vulkan C++ Mind Palace

A simple Vulkan C++ application using GLFW for window management.

## Building

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build
cmake --build .

# Run
./vulkan_app
```

## Requirements

- CMake 3.20+
- Vulkan SDK
- C++17 compiler
- GLFW (automatically downloaded by CMake)

## Project Structure

- `src/` - Source files
- `CMakeLists.txt` - CMake configuration
- `build/` - Build output (not tracked)

# use latest FUSE API
add_definitions(-DFUSE_USE_VERSION=26)
# required by FUSE
add_definitions(-D_FILE_OFFSET_BITS=64)

find_package(FUSE REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem)

add_executable(fs main.cpp fs.cpp fs.h error.h)
target_link_libraries(fs PUBLIC libfuse Boost::filesystem)

# ISO C++ spawns annoying warnings about string literals
target_compile_options(fs PRIVATE -Wno-write-strings)

cmake_minimum_required(VERSION 3.20)


project(vdx_c
VERSION 0.1.0
LANGUAGES C
)


# Create executable
add_executable(vdx_c
src/main.c
src/vdx.c
)


# Include headers
target_include_directories(vdx_c PRIVATE include)


# Require C11
target_compile_features(vdx_c PRIVATE c_std_11)

# Warnings
target_compile_options(vdx_c PRIVATE
-Wall
-Wextra
-Wpedantic
)

find_package(Threads REQUIRED)
target_link_libraries(vdx_c PRIVATE m Threads::Threads)

cmake_minimum_required(VERSION 3.5)
project(clamma-gen-multi VERSION 1.0 LANGUAGES C)

include(CTest)
if (NOT LIBCLAMMA_MAX_SESSIONS_PER_MODEL) # ie, out of clamma tree build
find_package(clamma)
endif()

add_compile_options(-Wall -Wextra -Werror -pedantic -g -Ofast)

add_executable(clamma-gen-multi clamma-gen-multi.c)
include_directories(${CMAKE_SOURCE_DIR}/inc)
target_link_libraries(clamma-gen-multi PRIVATE clamma m)

install(TARGETS clamma-gen-multi DESTINATION bin)


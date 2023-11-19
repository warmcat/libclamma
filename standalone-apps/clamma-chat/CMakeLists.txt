cmake_minimum_required(VERSION 3.5)
project(clamma-chat VERSION 1.0 LANGUAGES C)

include(CTest)
if (NOT LIBCLAMMA_MAX_SESSIONS_PER_MODEL) # ie, out of clamma tree build
find_package(clamma)
endif()

add_compile_options(-Wall -Wextra -Werror -pedantic -g -Ofast)

add_executable(clamma-chat clamma-chat.c)
include_directories(${CMAKE_SOURCE_DIR}/inc)
target_link_libraries(clamma-chat PRIVATE clamma m)

install(TARGETS clamma-chat DESTINATION bin)

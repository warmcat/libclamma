cmake_minimum_required(VERSION 3.5)
project(clamma-gen VERSION 1.0 LANGUAGES C)
include(CTest)

# ie, out of clamma tree build
if (NOT LIBCLAMMA_MAX_SESSIONS_PER_MODEL)
find_package(clamma)
endif()

add_compile_options(-Wall -Wextra -Werror -pedantic -g -Ofast)

add_executable(${PROJECT_NAME} clamma-gen.c)
target_link_libraries(${PROJECT_NAME} PRIVATE clamma m)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME} ../../stories110M.bin -z ../../tokenizer.bin -s 1234 )

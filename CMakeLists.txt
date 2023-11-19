cmake_minimum_required(VERSION 3.5)
project(clamma VERSION 1.0 LANGUAGES C)

include(GNUInstallDirs)
include(CTest)

# OFF, PTHREADS
set(LIBCLAMMA_THREADING              OFF CACHE STRING "Threading Model"                   )
set(LIBCLAMMA_MAX_THREADS            16  CACHE STRING "Maximum threads to spawn"          )
set(LIBCLAMMA_MAX_THREAD_JOB_QUEUE   256 CACHE STRING "Max thread job ring queue"         )
set(LIBCLAMMA_MAX_SESSIONS_PER_MODEL 16  CACHE STRING "Max concurrent sessions per model" )
set(LIBCLAMMA_WITH_LWS               OFF CACHE STRING "Build demo that needs latest lws"  )

add_compile_options(-Wall -Wextra -Werror -pedantic -g -Ofast -fvisibility=hidden)

# build libclamma itself

if (NOT LIBCLAMMA_THREADING STREQUAL "OFF" AND NOT LIBCLAMMA_THREADING STREQUAL "0")
        set(COMPILE_SMP "lib/smp.c")
endif()

if (LIBCLAMMA_THREADING STREQUAL "PTHREADS")
        set(COMPILE_THREADS "lib/smp-pthreads.c")
endif()

add_library(${PROJECT_NAME} lib/txf.c
                   lib/vocab.c
                   lib/sampler.c
                   lib/session.c
                   lib/weight_cache.c
                   ${COMPILE_SMP}
                   ${COMPILE_THREADS}
                   inc/clamma.h)

add_compile_definitions(
        LIBCLAMMA_MAX_SESSIONS_PER_MODEL=${LIBCLAMMA_MAX_SESSIONS_PER_MODEL})

if (LIBCLAMMA_THREADING STREQUAL "PTHREADS")
        add_compile_definitions(LIBCLAMMA_SMP=1
                                LIBCLAMMA_WITH_PTHREADS=1
                                LIBCLAMMA_THREAD_MODEL=\"pthreads\"
                                LIBCLAMMA_MAX_THREADS=${LIBCLAMMA_MAX_THREADS}
                                LIBCLAMMA_MAX_THREAD_JOB_QUEUE=${LIBCLAMMA_MAX_THREAD_JOB_QUEUE}
                                )
        set(LIBCLAMMA_WITH_PTHREADS 1)
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	find_package(Threads REQUIRED)
	target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
else()
        add_compile_definitions(LIBCLAMMA_THREAD_MODEL=\"single-threaded\")
endif()

add_compile_definitions(CLAMMA_MODEL_SEARCH_PATH=\"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/clamma-models\")

target_link_libraries(${PROJECT_NAME} PRIVATE m)

install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}_targets DESTINATION lib)
install(FILES inc/clamma.h DESTINATION include)
export(TARGETS ${PROJECT_NAME} FILE ${PROJECT_NAME}.cmake)
install(EXPORT ${PROJECT_NAME}_targets DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

# build the libclamma selftests

add_executable(clamma-selftest-tokenize test/selftest-tokenize.c)
include_directories(${CMAKE_SOURCE_DIR}/inc)
target_link_libraries(clamma-selftest-tokenize PRIVATE clamma)
install(TARGETS clamma-selftest-tokenize DESTINATION bin)

add_test(NAME selftest-tokenize COMMAND clamma-selftest-tokenize )

add_executable(clamma-selftest-absolute test/selftest-absolute.c)
include_directories(${CMAKE_SOURCE_DIR}/inc)
target_link_libraries(clamma-selftest-absolute PRIVATE clamma)
install(TARGETS clamma-selftest-absolute DESTINATION bin)

add_test(NAME selftest-absolute COMMAND clamma-selftest-absolute )

# build the standalone apps... these are buildable on their own after libclamma
# has been installed, as a convenience they are also built here

add_subdirectory(standalone-apps/clamma-gen)
add_subdirectory(standalone-apps/clamma-gen-multi)
add_subdirectory(standalone-apps/clamma-chat)

if (LIBCLAMMA_WITH_PTHREADS AND LIBCLAMMA_WITH_LWS)
# this demo requires pthreads
add_subdirectory(standalone-apps/clamma-ws-demo)
endif()

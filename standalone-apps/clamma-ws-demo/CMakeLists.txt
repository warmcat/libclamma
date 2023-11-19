cmake_minimum_required(VERSION 3.5)
project(clamma-ws-demo VERSION 1.0 LANGUAGES C)
include(CTest)
include(GNUInstallDirs)

# ie, out of clamma tree build
if (NOT LIBCLAMMA_MAX_SESSIONS_PER_MODEL)
find_package(clamma)
endif()
find_package(libwebsockets)

add_compile_options(-Wall -Wextra -Werror -pedantic -g -Ofast
                    -DCLAMMA_INSTALL_SHARE=\"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}\")

add_executable(${PROJECT_NAME} clamma-ws-demo.c
                               ss-genpipe.c
                               ss-clamma-srv.c)
                               
target_link_libraries(${PROJECT_NAME} PRIVATE clamma m websockets)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES assets/favicon.ico
              assets/index.html
              assets/clamma-ws-demo.js
              assets/clamma-ws-demo.css
        DESTINATION share/${PROJECT_NAME})

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME} ../../stories110M.bin -z ../../tokenizer.bin -s 1234 )

project(libgamestream)
cmake_minimum_required(VERSION 3.15)
set(SO_VERSION 4)

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL 1.0.2 REQUIRED)
find_package(EXPAT REQUIRED)

add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

aux_source_directory(./ GAMESTREAM_SRC_LIST)
aux_source_directory(../third_party/h264bitstream GAMESTREAM_SRC_LIST)
add_subdirectory(../third_party/moonlight-common-c ../../third_party/moonlight-common-c/build)
add_library(gamestream STATIC ${GAMESTREAM_SRC_LIST})

target_link_libraries(gamestream moonlight-common-c)

target_include_directories(gamestream PRIVATE ../third_party/moonlight-common-c/src ../third_party/h264bitstream)
target_link_libraries(gamestream ${CURL_LIBRARIES} ${OPENSSL_LIBRARIES} ${EXPAT_LIBRARIES})
target_link_libraries(gamestream ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})

install(TARGETS gamestream DESTINATION ${CMAKE_INSTALL_LIBDIR})

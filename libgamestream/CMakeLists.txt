project(libgamestream)
cmake_minimum_required(VERSION 3.15)
set(SO_VERSION 4)

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL 1.0.2 REQUIRED)
find_package(EXPAT REQUIRED)


aux_source_directory(./ GAMESTREAM_SRC_LIST)
aux_source_directory(../third_party/h264bitstream GAMESTREAM_SRC_LIST)

aux_source_directory(../third_party/moonlight-common-c/src MOONLIGHT_COMMON_SRC_LIST)
aux_source_directory(../third_party/moonlight-common-c/reedsolomon MOONLIGHT_COMMON_SRC_LIST)

add_library(moonlight-common SHARED ${MOONLIGHT_COMMON_SRC_LIST})

add_library(gamestream SHARED ${GAMESTREAM_SRC_LIST})
target_link_libraries(gamestream moonlight-common)

target_include_directories(gamestream PRIVATE ../third_party/moonlight-common-c/src ../third_party/h264bitstream)
target_include_directories(moonlight-common PRIVATE ../third_party/moonlight-common-c/reedsolomon ${ENET_INCLUDE_DIRS})
target_link_libraries(gamestream ${CURL_LIBRARIES} ${OPENSSL_LIBRARIES} ${EXPAT_LIBRARIES})
target_link_libraries(moonlight-common ${ENET_LIBRARIES})

target_link_libraries(gamestream ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})

install(TARGETS gamestream moonlight-common DESTINATION ${CMAKE_INSTALL_LIBDIR})

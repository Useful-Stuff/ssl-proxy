cmake_minimum_required(VERSION 3.8.2)

project(ssl-proxy VERSION 1.0.0)

find_package(OpenSSL)
find_package(Threads)

include_directories(${OPENSSL_INCLUDE_DIR})

add_executable(ssl-proxy main.cpp server.cpp selector.cpp)
add_executable(echo-client echo_client.cpp)
add_executable(echo-server echo_server.cpp)
add_executable(ssl-echo-server ssl_echo_server.cpp)

set_target_properties(ssl-proxy 
                      ssl-echo-server echo-client echo-server PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    COMPILE_OPTIONS "-Wpedantic;-Wall;-Wextra"
)

target_link_libraries(echo-client ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(echo-server ${CMAKE_THREAD_LIBS_INIT})  

target_link_libraries(ssl-proxy 
                                ${OPENSSL_LIBRARIES}
                                ${CMAKE_THREAD_LIBS_INIT}) 

target_link_libraries(ssl-echo-server
                                      ${OPENSSL_LIBRARIES} 
                                      ${CMAKE_THREAD_LIBS_INIT}) 

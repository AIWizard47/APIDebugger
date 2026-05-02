cmake_minimum_required(VERSION 3.20)

project(APIDebugger)

set(CMAKE_CXX_STANDARD 17)

include_directories(
    include
    third_party/openssl/include
)

link_directories(
    third_party/openssl/lib
)

add_executable(api_tester
    main.cpp
    src/http.cpp
)

target_link_libraries(api_tester
    ws2_32
    ssl
    crypto
)

include_directories(include)
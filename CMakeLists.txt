cmake_minimum_required(VERSION 3.20)

project(APIDebugger)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenSSL REQUIRED)

include_directories(include)

add_executable(api_tester
    main.cpp
    src/http.cpp
)

target_link_libraries(api_tester
    ws2_32
    OpenSSL::SSL
    OpenSSL::Crypto
)
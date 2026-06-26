cmake_minimum_required(VERSION 3.20)
project(order_sentinel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(order_sentinel
    src/main.cpp
    src/ws_client.cpp
)

target_link_libraries(order_sentinel PRIVATE
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

enable_testing()
find_package(Catch2 REQUIRED)
add_executable(tests tests/test_main.cpp)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
add_test(NAME unit_tests COMMAND tests)
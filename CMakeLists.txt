cmake_minimum_required(VERSION 3.14)
project(AnkhMorporkTimes LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(nlohmann_json 3.11.2 REQUIRED)
find_package(asio REQUIRED)
find_package(Threads REQUIRED)

add_executable(server 
    server.cpp
    classes.hpp
    engine.hpp
    server.hpp
    editor.hpp
)

target_link_libraries(server 
    PRIVATE 
        nlohmann_json::nlohmann_json
        asio::asio
        Threads::Threads
)

add_executable(client 
    client.cpp
)

target_link_libraries(client 
    PRIVATE 
        nlohmann_json::nlohmann_json
        asio::asio
        Threads::Threads
)

target_compile_definitions(client PRIVATE ASIO_STANDALONE)

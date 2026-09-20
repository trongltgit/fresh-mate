cmake_minimum_required(VERSION 3.16)
project(FreshMateWeb VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/src
)

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(CURL REQUIRED)

add_executable(freshmate_server
    src/main.cpp
    src/Database.cpp
    src/ApiClient.cpp
)

target_link_libraries(freshmate_server
    PRIVATE
    SQLite::SQLite3
    Threads::Threads
    nlohmann_json::nlohmann_json
    CURL::libcurl
)

# Copy public assets next to binary on build
add_custom_command(TARGET freshmate_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/public
        $<TARGET_FILE_DIR:freshmate_server>/public
)

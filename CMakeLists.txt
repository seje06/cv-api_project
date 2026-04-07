cmake_minimum_required(VERSION 3.16)
project(ImageApiServer VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(ImageApiServer
    src/main.cpp
)

target_include_directories(ImageApiServer PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/external
)

target_link_libraries(ImageApiServer PRIVATE
    ${OpenCV_LIBS}
    CURL::libcurl
    nlohmann_json::nlohmann_json
)
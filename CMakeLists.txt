cmake_minimum_required(VERSION 3.21)
project(CounterForge VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Gui)

add_executable(CounterForge WIN32
    src/main.cpp
    src/counterspec.cpp
    src/counterrenderer.cpp
    src/widgets.cpp
    src/legenddialog.cpp
    src/mainwindow.cpp
    src/jsonapi.cpp
)

target_link_libraries(CounterForge PRIVATE Qt6::Widgets Qt6::Gui)

if (MSVC)
    target_compile_options(CounterForge PRIVATE /utf-8)
endif()

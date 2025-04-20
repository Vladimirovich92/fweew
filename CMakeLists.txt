cmake_minimum_required(VERSION 3.16)
project(WalletGenerator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets Concurrent REQUIRED)
if (NOT Qt6_FOUND)
    message(FATAL_ERROR "Qt6 not found. Please install Qt6 or set Qt6_DIR.")
endif()

find_package(OpenCL REQUIRED)
if (NOT OpenCL_FOUND)
    message(FATAL_ERROR "OpenCL not found. Please install OpenCL libraries.")
endif()

set(SOURCES
    main.cpp
    mainwindow.cpp
    bruteforce.cpp
)

set(HEADERS
    mainwindow.h
    bruteforce.h
)

set(UI_FILES
    mainwindow.ui
)

set(RESOURCE_FILES
    resources.qrc
)

add_executable(WalletGenerator WIN32
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
    ${RESOURCE_FILES}
)

target_include_directories(WalletGenerator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenCL_INCLUDE_DIRS}
)

target_link_libraries(WalletGenerator PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    ${OpenCL_LIBRARIES}
)

if (MINGW)
    target_compile_options(WalletGenerator PRIVATE -Wall -Wextra)
    target_link_libraries(WalletGenerator PRIVATE -mwindows -static-libgcc -static-libstdc++)
endif()

set_target_properties(WalletGenerator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
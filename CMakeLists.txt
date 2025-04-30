cmake_minimum_required(VERSION 3.20)
project(BrainFlyer LANGUAGES CXX)

# Указываем стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Включаем автоматическую генерацию moc-файлов для Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Находим Qt6
find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

# Находим OpenCL
find_package(OpenCL REQUIRED)

# Путь к secp256k1
set(SECP256K1_INCLUDE_DIR "C:/secp256k1/include")
set(SECP256K1_LIBRARY_DIR "D:/brainflyer/build")
set(SECP256K1_LIBRARY "${SECP256K1_LIBRARY_DIR}/secp256k1.lib")

# Проверяем наличие secp256k1
if(NOT EXISTS "${SECP256K1_INCLUDE_DIR}/secp256k1.h")
    message(FATAL_ERROR "secp256k1 headers not found at ${SECP256K1_INCLUDE_DIR}. Please install secp256k1.")
endif()
if(NOT EXISTS "${SECP256K1_LIBRARY}")
    message(FATAL_ERROR "secp256k1 library not found at ${SECP256K1_LIBRARY}. Please build and install secp256k1.")
endif()

# Исходные файлы
set(SOURCES
    main.cpp
    BrainFlyer.cpp
    BloomFilter.cpp
)

# Заголовочные файлы
set(HEADERS
    BrainFlyer.h
)

# Добавляем исполняемый файл
add_executable(BrainFlyer ${SOURCES} ${HEADERS})

# Указываем пути к заголовкам
target_include_directories(BrainFlyer PRIVATE
    ${SECP256K1_INCLUDE_DIR}
    ${OpenCL_INCLUDE_DIRS}
)

# Линкуем библиотеки
target_link_libraries(BrainFlyer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    OpenCL::OpenCL
    ${SECP256K1_LIBRARY}
    -static-libgcc
    -static-libstdc++
)

# Оптимизация и флаги компиляции
target_compile_options(BrainFlyer PRIVATE
    -O3
    -Wall
    -Wextra
)

# Указываем CL_TARGET_OPENCL_VERSION
target_compile_definitions(BrainFlyer PRIVATE CL_TARGET_OPENCL_VERSION=200)

# Копирование kernel.cl в директорию сборки
configure_file(${CMAKE_SOURCE_DIR}/kernel.cl ${CMAKE_BINARY_DIR}/kernel.cl COPYONLY)

# Копирование libsecp256k1-5.dll в директорию сборки
configure_file(${SECP256K1_LIBRARY_DIR}/libsecp256k1-5.dll ${CMAKE_BINARY_DIR}/libsecp256k1-5.dll COPYONLY)

# Комментарий: Проверьте, используется ли статическая (secp256k1.lib) или динамическая (libsecp256k1-5.dll) линковка, чтобы избежать конфликтов.
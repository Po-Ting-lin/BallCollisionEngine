cmake_minimum_required(VERSION 3.15)

project(BallCollisionEngine VERSION 1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenMP REQUIRED COMPONENTS CXX)

add_executable(BallCollision
    BallCollision/Source.cpp
    BallCollision/BallCollisionEngine.cpp
    BallCollision/UI_mainConsole.h
    BallCollision/BallCollisionEngine.h
    BallCollision/objectStructure.h
    BallCollision/utils.h
    BallCollision/olcPixelGameEngine.h
)

target_compile_features(BallCollision PRIVATE cxx_std_17)
target_include_directories(BallCollision PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/BallCollision")
target_link_libraries(BallCollision PRIVATE OpenMP::OpenMP_CXX)

set_target_properties(BallCollision PROPERTIES
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(MSVC)
    target_compile_options(BallCollision PRIVATE /W3)
else()
    target_compile_options(BallCollision PRIVATE
        -Wall
        -Wextra
        -Wno-unknown-pragmas
        -Wno-unused-parameter
    )

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(BallCollision PRIVATE
            -Wno-cast-function-type
            -Wno-missing-field-initializers
        )
    endif()
endif()

if(WIN32)
    target_link_libraries(BallCollision PRIVATE
        user32
        gdi32
        opengl32
        gdiplus
        shlwapi
        dwmapi
    )

    if(MINGW AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
        message(FATAL_ERROR
            "MinGW GCC 9 or newer is required for std::filesystem support."
        )
    endif()
elseif(UNIX AND NOT APPLE)
    find_package(OpenGL REQUIRED)
    find_package(PNG REQUIRED)
    find_package(Threads REQUIRED)
    find_package(X11 REQUIRED)

    target_link_libraries(BallCollision PRIVATE
        OpenGL::GL
        PNG::PNG
        Threads::Threads
        X11::X11
    )

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
       AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
        target_link_libraries(BallCollision PRIVATE stdc++fs)
    endif()
else()
    message(FATAL_ERROR "This CMake configuration currently supports Windows and Linux.")
endif()

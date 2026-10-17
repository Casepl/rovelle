cmake_minimum_required(VERSION 3.21)

project(rovelle VERSION 0.1.0 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED COMPONENTS Crypto)

add_library(rovelle_core STATIC
    src/status.c
    src/version.c
    src/hash.c
    src/blob.c
    src/manifest.c
)

target_link_libraries(rovelle_core PRIVATE OpenSSL::Crypto)

target_compile_features(rovelle_core PUBLIC c_std_17)
target_include_directories(rovelle_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_definitions(rovelle_core PRIVATE
    ROVELLE_VERSION="${PROJECT_VERSION}"
    _POSIX_C_SOURCE=200809L
)

add_executable(rovelle cli/main.c)
target_link_libraries(rovelle PRIVATE rovelle_core)

foreach(target IN ITEMS rovelle_core rovelle)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes -Werror
        )
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    endif()
endforeach()

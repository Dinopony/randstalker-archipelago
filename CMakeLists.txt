cmake_minimum_required(VERSION 3.21)

project(randstalker_archipelago VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SFML_STATIC_LIBRARIES TRUE)
set(SFML_DIR "extlibs/sfml/lib/cmake/SFML")

add_compile_definitions(RELEASE="${PROJECT_VERSION}")
add_compile_definitions(MAJOR_RELEASE=${PROJECT_VERSION_MAJOR}${PROJECT_VERSION_MINOR})

find_package(SFML 2.5.1 COMPONENTS graphics REQUIRED)

if(DEBUG)
    add_compile_definitions(DEBUG)
endif()

add_subdirectory(extlibs/landstalker_lib/landstalker_lib landstalker_lib)

include_directories("extlibs/landstalker_lib")
include_directories("extlibs/apclientpp")
include_directories("extlibs/asio/include")
include_directories("extlibs/json/include")
include_directories("extlibs/valijson/include")
include_directories("extlibs/websocketpp")
include_directories("extlibs/wswrap/include")
include_directories("extlibs/imgui/")

add_compile_definitions(WSWRAP_NO_SSL)
add_compile_definitions(ASIO_STANDALONE)
add_compile_definitions(_WIN32_WINNT=0x0600)
add_compile_definitions(WIN32_LEAN_AND_MEAN)
add_compile_definitions(_WEBSOCKETPP_CPP11_INTERNAL_)

if (MSVC)
    add_compile_options(/bigobj)
else ()
    add_compile_options(-Wa,-mbig-obj)
endif ()

macro(WRAPPED_DEPENDENCY)
    message(STATUS "Defining wrapped dependency for ${CMAKE_CURRENT_SOURCE_DIR}/${ARGV1}")
    add_custom_command(
            OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/${ARGV1}.hxx"
            COMMAND "${CMAKE_COMMAND}"
            ARGS -DINPUT_FILE="${ARGV1}" -DVARIABLE_NAME="${ARGV2}" -DPROCESS_TYPE="${ARGV0}" -P "extlibs/landstalker_lib/landstalker_lib/wrap_file.cmake"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${ARGV1}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endmacro()

# Wrap JSON model files and assets into automated .hxx headers
wrapped_dependency(TEXT src/data/locations.json LOCATIONS_JSON)

set(SOURCES
        "src/main.cpp"
        "src/game_state.hpp"
        "src/game_state.cpp"
        "src/archipelago_interface.hpp"
        "src/archipelago_interface.cpp"
        "src/preset_builder.hpp"
        "src/preset_builder.cpp"
        "src/randstalker_invoker.cpp"
        "src/randstalker_invoker.hpp"
        "src/retroarch_interface.cpp"
        "src/retroarch_interface.hpp"
        "src/data/locations.json.hxx" src/location.hpp src/location.cpp)

add_executable(randstalker_archipelago "${SOURCES}")
target_link_libraries(randstalker_archipelago psapi sfml-graphics opengl32)

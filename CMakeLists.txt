cmake_minimum_required(VERSION 3.21)

project(randstalker_archipelago VERSION 1.3.0 LANGUAGES CXX)

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
include_directories("extlibs/openssl/")
link_directories("extlibs/openssl/lib")

add_compile_definitions(ASIO_STANDALONE)
add_compile_definitions(NOMINMAX)
add_compile_definitions(_WIN32_WINNT=0x0600)
#add_compile_definitions(WIN32_LEAN_AND_MEAN)
add_compile_definitions(WSWRAP_SEND_EXCEPTIONS)

if (MSVC)
    add_compile_options(/bigobj)
    add_compile_definitions(_WEBSOCKETPP_CPP11_STL_)
    add_compile_definitions(HAS_STD_FILESYSTEM)

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
wrapped_dependency(TEXT src/data/item_source.json ITEM_SOURCES_JSON)
wrapped_dependency(TEXT src/data/trackable_regions.json TRACKABLE_REGIONS_JSON)
wrapped_dependency(TEXT src/data/trackable_items.json TRACKABLE_ITEMS_JSON)

set(SOURCES
        extlibs/imgui/imgui.cpp
        extlibs/imgui/imgui_draw.cpp
        extlibs/imgui/imgui_widgets.cpp
        extlibs/imgui/imgui_tables.cpp
        extlibs/imgui/imgui-SFML.cpp
        extlibs/imgui/imgui_demo.cpp

        src/data/item_source.json.hxx

        src/data/trackable_regions.json.hxx
        src/trackable_region.hpp
        src/trackable_region.cpp

        src/data/trackable_items.json.hxx
        src/trackable_item.hpp
        src/trackable_item.cpp

        src/emulator_interfaces/emulator_interface.hpp
        src/emulator_interfaces/retroarch_mem_interface.cpp
        src/emulator_interfaces/retroarch_mem_interface.hpp
        src/emulator_interfaces/bizhawk_mem_interface.cpp
        src/emulator_interfaces/bizhawk_mem_interface.hpp

        src/multiworld_interfaces/multiworld_interface.hpp
        src/multiworld_interfaces/archipelago_interface.hpp
        src/multiworld_interfaces/archipelago_interface.cpp
        src/multiworld_interfaces/offline_play_interface.hpp

        src/main.cpp
        src/client.hpp
        src/game_state.hpp
        src/game_state.cpp
        src/preset_builder.hpp
        src/preset_builder.cpp
        src/location.hpp
        src/location.cpp
        src/user_interface.hpp
        src/user_interface.cpp
        src/logger.hpp
        src/logger.cpp
        src/randstalker_invoker.cpp
        src/randstalker_invoker.hpp
        src/tracker_config.hpp
        src/tracker_config.cpp)

add_executable(randstalker_archipelago "${SOURCES}")
target_link_libraries(randstalker_archipelago psapi sfml-graphics opengl32 libssl libcrypto crypt32)

cmake_minimum_required(VERSION 3.16)
project(GeminiChatMonitor VERSION 1.0.0 LANGUAGES CXX)
add_compile_definitions(_WIN32_WINNT=0x0A00)

# ─── C++ standard ────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ─── FetchContent dependencies ───────────────────────────────────────────────
include(FetchContent)

# Crow (header-only web framework)
FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG        master
    GIT_SHALLOW    ON
)

# nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW ON
)

FetchContent_MakeAvailable(nlohmann_json)

# ─── System dependencies ─────────────────────────────────────────────────────
find_package(CURL    REQUIRED)
find_package(Threads REQUIRED)

# Boost.Asio is used by Crow internally
find_package(Boost 1.71 REQUIRED COMPONENTS system)
set(CROW_USE_BOOST ON CACHE BOOL "Use Boost.Asio for Crow" FORCE)

FetchContent_MakeAvailable(crow)

# ─── Target ──────────────────────────────────────────────────────────────────
add_executable(gemini_monitor
    src/main.cpp
    src/gemini_client.cpp
    src/chat_handler.cpp
    src/stats_manager.cpp
)

target_include_directories(gemini_monitor PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(gemini_monitor PRIVATE
    Crow::Crow
    nlohmann_json::nlohmann_json
    CURL::libcurl
    Threads::Threads
    Boost::system
)

target_compile_options(gemini_monitor PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -O2>
)

# ─── Install ─────────────────────────────────────────────────────────────────
install(TARGETS gemini_monitor DESTINATION bin)

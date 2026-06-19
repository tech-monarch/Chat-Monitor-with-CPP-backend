cmake_minimum_required(VERSION 3.16)

project(GeminiChatMonitor
    VERSION 1.0.0
    LANGUAGES CXX
)

# Windows compatibility only
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()

# Boost.Asio compatibility
add_compile_definitions(BOOST_ASIO_DISABLE_CONCEPTS)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


if(NOT CMAKE_BUILD_TYPE)
    set(
        CMAKE_BUILD_TYPE Release
        CACHE STRING "Build type"
        FORCE
    )
endif()


# ─── Dependencies ────────────────────────────────────────────────────────────

include(FetchContent)


# Crow
FetchContent_Declare(
    crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG master
    GIT_SHALLOW ON
)


# nlohmann json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW ON
)


FetchContent_MakeAvailable(nlohmann_json)


find_package(CURL REQUIRED)

find_package(Threads REQUIRED)

find_package(
    Boost 1.71
    REQUIRED
    COMPONENTS system
)


set(
    CROW_USE_BOOST ON
    CACHE BOOL ""
    FORCE
)


set(
    CROW_BUILD_EXAMPLES OFF
    CACHE BOOL ""
    FORCE
)

set(
    CROW_BUILD_TESTS OFF
    CACHE BOOL ""
    FORCE
)


FetchContent_MakeAvailable(crow)



# ─── Application ─────────────────────────────────────────────────────────────

add_executable(
    gemini_monitor

    src/main.cpp
    src/gemini_client.cpp
    src/chat_handler.cpp
    src/stats_manager.cpp
)


target_include_directories(
    gemini_monitor PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)


target_link_libraries(
    gemini_monitor PRIVATE

    Crow::Crow

    nlohmann_json::nlohmann_json

    CURL::libcurl

    Threads::Threads

    Boost::system
)



if(MSVC)

    target_compile_options(
        gemini_monitor PRIVATE
        /W4
    )

else()

    target_compile_options(
        gemini_monitor PRIVATE
        -Wall
        -Wextra
        -O2
    )

endif()



install(
    TARGETS gemini_monitor
    DESTINATION bin
)
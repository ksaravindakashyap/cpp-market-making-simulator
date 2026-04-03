add_executable(mmsim_server main.cpp)
target_link_libraries(mmsim_server PRIVATE mmsim_core)

option(MMSIM_BUILD_WEBSOCKET "Build mmsim_ws_server (websocketpp + nlohmann/json)" ON)

if(MMSIM_BUILD_WEBSOCKET)
    include(FetchContent)

    FetchContent_Declare(
        nlohmann_json_fc
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_Declare(
        asio_fc
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-30-2
    )
    FetchContent_Declare(
        websocketpp_fc
        GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
        GIT_TAG 0.8.2
    )

    FetchContent_GetProperties(nlohmann_json_fc)
    if(NOT nlohmann_json_fc_POPULATED)
        FetchContent_Populate(nlohmann_json_fc)
    endif()
    FetchContent_GetProperties(asio_fc)
    if(NOT asio_fc_POPULATED)
        FetchContent_Populate(asio_fc)
    endif()
    FetchContent_GetProperties(websocketpp_fc)
    if(NOT websocketpp_fc_POPULATED)
        FetchContent_Populate(websocketpp_fc)
    endif()

    add_library(mmsim_nlohmann_json INTERFACE)
    target_include_directories(mmsim_nlohmann_json INTERFACE
        "${nlohmann_json_fc_SOURCE_DIR}/include"
    )
    add_library(nlohmann_json::nlohmann_json ALIAS mmsim_nlohmann_json)

    add_executable(mmsim_ws_server
        ws_main.cpp
        ws/ws_hub.cpp
        comparison_sim.cpp
    )
    target_include_directories(mmsim_ws_server PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${asio_fc_SOURCE_DIR}/asio/include"
        "${websocketpp_fc_SOURCE_DIR}"
    )
    target_link_libraries(mmsim_ws_server PRIVATE
        mmsim_core
        nlohmann_json::nlohmann_json
    )
    target_compile_definitions(mmsim_ws_server PRIVATE
        ASIO_STANDALONE
        ASIO_HAS_STD_CHRONO
        WEBSOCKETPP_CPP11_STL
        # websocketpp skips C++11 threads on MinGW unless this is set
        _WEBSOCKETPP_CPP11_THREAD_
    )
    if(WIN32)
        target_link_libraries(mmsim_ws_server PRIVATE ws2_32 mswsock)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(mmsim_ws_server PRIVATE -Wno-template-id-cdtor)
    endif()
endif()

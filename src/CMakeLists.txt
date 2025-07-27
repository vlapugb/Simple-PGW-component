include(FetchContent)
FetchContent_Declare(
        boost
        GIT_REPOSITORY https://github.com/boostorg/boost.git
        GIT_TAG        boost-1.87.0
)
set(BOOST_INCLUDE_LIBRARIES system asio date_time)
set(BOOST_ENABLE_CMAKE ON)

set(CROW_USE_BOOST ON CACHE BOOL "Use Boost.Asio for Crow")
set(CROW_INSTALL  OFF CACHE BOOL "Disable Crow install targets" FORCE)

FetchContent_Declare(
        crow
        GIT_REPOSITORY https://github.com/CrowCpp/crow.git
        GIT_TAG v1.2.1.2
)
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
)
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog
        GIT_TAG v1.15.3
)

FetchContent_MakeAvailable(
        nlohmann_json
        crow
        spdlog
        boost
)



add_executable(client
        Client/main.cpp
        Client/UdpClient.cpp
        Client/UdpClient.h
)
add_executable(server
        main.cpp
        Pgw/UdpServer.cpp
        Pgw/SessionManager.cpp
        Pgw/ControlPlaneServer.cpp
        Pgw/BlackListStorer.cpp
        Pgw/CdrWriter.cpp
        Pgw/UdpServer.h
        Pgw/SessionManager.h
        Pgw/ControlPlaneServer.h
        Pgw/BlackListStorer.h
        Pgw/CdrWriter.h
        Common/ConfigLoader.h
)

add_library(pgw_core STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Client/UdpClient.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pgw/UdpServer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pgw/SessionManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pgw/ControlPlaneServer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pgw/BlackListStorer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pgw/CdrWriter.cpp
)

target_include_directories(pgw_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pgw_core PUBLIC
        nlohmann_json::nlohmann_json
        Crow::Crow
        Boost::asio
        Boost::system
        spdlog::spdlog
        pthread
)

target_link_libraries(server PRIVATE nlohmann_json::nlohmann_json Crow::Crow Boost::asio Boost::system spdlog::spdlog)
target_link_libraries(client PRIVATE nlohmann_json::nlohmann_json Crow::Crow Boost::asio Boost::system spdlog::spdlog)
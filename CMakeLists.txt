cmake_minimum_required(VERSION 3.21)
project(DorrieDesktop VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network)

qt_add_executable(dorrie
    src/main.cpp
        src/component/RequestComponent.cpp
        src/component/RequestComponent.h
        src/component/MainComponent.cpp
        src/component/MainComponent.h
        src/component/ResponseComponent.cpp
        src/component/ResponseComponent.h
        src/component/HttpClient.cpp
        src/component/HttpClient.h
        src/component/SettingsDialog.cpp
        src/component/SettingsDialog.h
        src/component/RequestStore.cpp
        src/component/RequestStore.h
        src/component/RequestTreeComponent.cpp
        src/component/RequestTreeComponent.h
        src/component/AppPaths.h
        src/component/EditorStyle.h
        src/component/ResponseData.h
        src/component/SessionStore.cpp
        src/component/SessionStore.h
)

qt_add_resources(dorrie "icons"
    PREFIX "/"
    FILES resources/icons/dorrie.png
)

target_link_libraries(dorrie PRIVATE Qt6::Widgets Qt6::Network)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(dorrie PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
endif()



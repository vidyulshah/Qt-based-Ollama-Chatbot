cmake_minimum_required(VERSION 3.16)

project(OllamaChat VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Multimedia)

qt_add_executable(OllamaChat
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/ollamaclient.h
    src/ollamaclient.cpp
    src/chatbubble.h
    src/chatbubble.cpp
    src/speechinput.h
    src/speechinput.cpp
    src/piperspeaker.h
    src/piperspeaker.cpp
    resources/resources.qrc
)

target_link_libraries(OllamaChat PRIVATE Qt6::Widgets Qt6::Network Qt6::Multimedia)

if(WIN32)
    set_target_properties(OllamaChat PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

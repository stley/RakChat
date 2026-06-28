cmake_minimum_required(VERSION 3.20)

project(RakChat VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- RakNet Sources ---
file(GLOB RAKNET_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/RakNet/Source/*.cpp
)


add_library(raknet STATIC ${RAKNET_SOURCES})

set(PA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)

add_subdirectory(opus)
add_subdirectory(portaudio)
add_subdirectory(SDL)

set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od")

target_include_directories(raknet PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/RakNet/Source
)



# Some RakNet builds require this on Windows
if (WIN32)
    #target_compile_definitions(raknet PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if (WIN32)
    target_link_libraries(raknet PRIVATE ws2_32)
endif()

# --- Server ---
add_executable(SpeakeasyServer
    server/server.cpp
    server/RakChatChannel.cpp
    server/RakChatUser.cpp

)
target_link_libraries(SpeakeasyServer PRIVATE
    raknet
    portaudio
    opus
)

add_library(SpeakeasyCore STATIC
    client/SpeakEngine.cpp
    client/SoundDevice.cpp
    client/Encoder.cpp
)
target_link_libraries(SpeakeasyCore PUBLIC
    raknet
    portaudio
    opus
)


# --- Client ---
add_executable(SpeakeasyClient
    client/Client.cpp
    client/main.cpp
)
target_link_libraries(SpeakeasyClient PRIVATE SpeakeasyCore)


add_executable(SpeakeasyUI
    client/Client.cpp
    guiclient/GUIClient.cpp
    guiclient/main.cpp
    guiclient/CClient.cpp
    microui/render.c    
    microui/microui.c
)


target_link_directories(SpeakeasyUI PRIVATE
    ${CMAKE_SOURCE_DIR}/build/SDL/$<CONFIG>
)

if (WIN32)
    target_link_libraries(SpeakeasyUI PRIVATE
        SpeakeasyCore
        ${CMAKE_SOURCE_DIR}/build/SDL/$<CONFIG>/SDL2-static$<$<CONFIG:Debug>:d>.lib
        opengl32
        imm32
        version
    )
elseif(UNIX)
    find_package(OpenGL REQUIRED)
    target_link_libraries(SpeakeasyUI PRIVATE
        SpeakeasyCore
        SDL2-static
        OpenGL::GL
        pthread
        m
    )
endif()

#set_target_properties(SpeakeasyUI PROPERTIES
#    WIN32_EXECUTABLE OFF
#)

#target_link_options(SpeakeasyUI PRIVATE
#    "/SUBSYSTEM:CONSOLE"
#)



target_include_directories(SpeakeasyCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/client
)
target_include_directories(SpeakeasyUI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/SDL/include
    ${CMAKE_CURRENT_SOURCE_DIR}/microui
)

target_include_directories(SpeakeasyClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common)

target_include_directories(SpeakeasyServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common)




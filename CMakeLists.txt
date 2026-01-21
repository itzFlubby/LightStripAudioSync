cmake_minimum_required(VERSION 3.15)
project(LightStripAudioSync LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFTW REQUIRED fftw3)
    pkg_check_modules(RTAUDIO REQUIRED rtaudio)
endif()

set(SOURCES
    main.cpp
    AudioCapture.cpp
    DataSender.cpp
    $<$<PLATFORM_ID:Windows>:RtAudio/RtAudio.cpp>
)

set(HEADERS
    AudioCapture.hpp
    DataSender.hpp
    Packet.hpp
    $<$<PLATFORM_ID:Windows>:FFTW/fftw3.h>
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}
)

if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/FFTW/libfftw3-3.dll
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/RtAudio>
    $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/FFTW>
    $<$<PLATFORM_ID:Linux>:${FFTW_INCLUDE_DIRS}>
    $<$<PLATFORM_ID:Linux>:${RTAUDIO_INCLUDE_DIRS}>
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/FFTW/libfftw3-3.lib>
    $<$<PLATFORM_ID:Linux>:${FFTW_LIBRARIES}>
    $<$<PLATFORM_ID:Linux>:${RTAUDIO_LIBRARIES}>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<PLATFORM_ID:Windows>:__WINDOWS_WASAPI__>
    $<$<PLATFORM_ID:Linux>:__LINUX_ALSA__>
)
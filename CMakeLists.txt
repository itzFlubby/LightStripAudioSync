cmake_minimum_required(VERSION 3.15)
project(LightStripAudioSync LANGUAGES CXX)

set(BROADCAST_ADDRESS "255.255.255.255")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    main.cpp
    AudioCapture.cpp
    DataSender.cpp
    RtAudio/RtAudio.cpp
)

set(HEADERS
    AudioCapture.hpp
    DataSender.hpp
    FFTW/fftw3.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/FFTW/libfftw3-3.dll
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/RtAudio
    ${CMAKE_CURRENT_SOURCE_DIR}/FFTW
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/FFTW/libfftw3-3.lib
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    BROADCAST_ADDRESS=\"${BROADCAST_ADDRESS}\"
    __WINDOWS_WASAPI__
)
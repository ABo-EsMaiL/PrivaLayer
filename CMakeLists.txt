cmake_minimum_required(VERSION 3.24)
project(PrivaLayer VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "PrivaLayer is a Windows desktop application. Build it on Windows with Visual Studio 2022.")
endif()

if(NOT MSVC)
    message(FATAL_ERROR "This project currently targets MSVC / Visual Studio 2022.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# WebView2 SDK is downloaded from the official NuGet package during configuration.
set(WEBVIEW2_VERSION "1.0.4314.35")
set(WEBVIEW2_DIR "${CMAKE_BINARY_DIR}/webview2-sdk")
set(WEBVIEW2_NATIVE "${WEBVIEW2_DIR}/build/native")
set(WEBVIEW2_HEADER "${WEBVIEW2_NATIVE}/include/WebView2.h")
set(WEBVIEW2_LOADER "${WEBVIEW2_NATIVE}/x64/WebView2LoaderStatic.lib")

if(NOT EXISTS "${WEBVIEW2_HEADER}" OR NOT EXISTS "${WEBVIEW2_LOADER}")
    set(WEBVIEW2_PACKAGE "${CMAKE_BINARY_DIR}/microsoft.web.webview2.${WEBVIEW2_VERSION}.nupkg")
    set(WEBVIEW2_URL "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/${WEBVIEW2_VERSION}/microsoft.web.webview2.${WEBVIEW2_VERSION}.nupkg")
    message(STATUS "Downloading Microsoft WebView2 SDK ${WEBVIEW2_VERSION}...")
    file(DOWNLOAD "${WEBVIEW2_URL}" "${WEBVIEW2_PACKAGE}" STATUS WEBVIEW2_DOWNLOAD_STATUS TLS_VERIFY ON SHOW_PROGRESS)
    list(GET WEBVIEW2_DOWNLOAD_STATUS 0 WEBVIEW2_DOWNLOAD_CODE)
    if(NOT WEBVIEW2_DOWNLOAD_CODE EQUAL 0)
        list(GET WEBVIEW2_DOWNLOAD_STATUS 1 WEBVIEW2_DOWNLOAD_MESSAGE)
        message(FATAL_ERROR "Could not download WebView2 SDK: ${WEBVIEW2_DOWNLOAD_MESSAGE}")
    endif()
    file(ARCHIVE_EXTRACT INPUT "${WEBVIEW2_PACKAGE}" DESTINATION "${WEBVIEW2_DIR}")
endif()

if(NOT EXISTS "${WEBVIEW2_HEADER}" OR NOT EXISTS "${WEBVIEW2_LOADER}")
    message(FATAL_ERROR "The WebView2 SDK package did not contain the expected x64 headers or loader library.")
endif()

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/privalayer.ico" PRIVLAYER_ICON_PATH)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/app.manifest" PRIVLAYER_MANIFEST_PATH)
set(PRIVALAYER_ICON_RC_PATH "${PRIVLAYER_ICON_PATH}")
set(PRIVALAYER_MANIFEST_RC_PATH "${PRIVLAYER_MANIFEST_PATH}")
configure_file(app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)

add_executable(PrivaLayer WIN32
    src/main.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/app.rc"
)

target_include_directories(PrivaLayer PRIVATE
    "${WEBVIEW2_NATIVE}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(PrivaLayer PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
)

target_compile_options(PrivaLayer PRIVATE /W4 /permissive- /utf-8 /EHsc)

target_link_libraries(PrivaLayer PRIVATE
    "${WEBVIEW2_LOADER}"
    Comctl32
    Gdiplus
    Ole32
    Shell32
    Shlwapi
    User32
    Gdi32
)

target_link_options(PrivaLayer PRIVATE "/MANIFEST:NO")

add_custom_command(TARGET PrivaLayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:PrivaLayer>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/privalayer.png"
        "$<TARGET_FILE_DIR:PrivaLayer>/assets/privalayer.png"
    VERBATIM
)

set_target_properties(PrivaLayer PROPERTIES
    OUTPUT_NAME "PrivaLayer"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:PrivaLayer>"
)

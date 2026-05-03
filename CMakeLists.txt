if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")
endif()

cmake_minimum_required(VERSION 3.20)

project(MikuMikuWorld VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE MIKUMIKUWORLD_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/MikuMikuWorld/*.cpp"
)

list(APPEND MIKUMIKUWORLD_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Depends/glad/src/glad.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Depends/stb_vorbis/stb_vorbis.c"
)

add_executable(MikuMikuWorld ${MIKUMIKUWORLD_SOURCES})

target_include_directories(MikuMikuWorld PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Depends/DirectXMath-master/Inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/Depends/glad/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/Depends/GLFW/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/Depends/stb_image"
    "${CMAKE_CURRENT_SOURCE_DIR}/Depends/miniaudio"
    "${CMAKE_CURRENT_SOURCE_DIR}/Depends/json"
    "${CMAKE_CURRENT_SOURCE_DIR}/Depends/stb_vorbis"
    "${CMAKE_CURRENT_SOURCE_DIR}/Depends/zlib/include"
)

target_compile_definitions(MikuMikuWorld PRIVATE
    MIKUMIKUWORLD_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_definitions(MikuMikuWorld PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _ENABLE_EXTENDED_ALIGNED_STORAGE
    )
else()
    target_compile_options(MikuMikuWorld PRIVATE
        -Wno-deprecated-declarations
        -Wno-unknown-pragmas
    )
endif()

if(APPLE)
    set(MAC_APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/MikuMikuWorld/mmw_icon.icns")

    set_target_properties(MikuMikuWorld PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "MikuMikuWorld"
        MACOSX_BUNDLE_ICON_FILE "mmw_icon.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.crash5band.MikuMikuWorld"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
    )

    target_sources(MikuMikuWorld PRIVATE ${MAC_APP_ICON})
    set_source_files_properties(${MAC_APP_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/MikuMikuWorld/main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/MikuMikuWorld/File.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/MikuMikuWorld/IO.cpp"
        PROPERTIES LANGUAGE OBJCXX
    )

    target_compile_definitions(MikuMikuWorld PRIVATE
        GL_SILENCE_DEPRECATION
    )

    find_package(OpenGL REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(glfw3 QUIET)

    if(NOT TARGET glfw)
        include(FetchContent)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4
        )
        FetchContent_MakeAvailable(glfw)
    endif()

    target_link_libraries(MikuMikuWorld PRIVATE
        glfw
        OpenGL::GL
        ZLIB::ZLIB
        "-framework AppKit"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
elseif(WIN32)
    target_link_libraries(MikuMikuWorld PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Depends/GLFW/lib/glfw3.lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/Depends/zlib/lib/zlibstatic.lib"
        opengl32
        Version
        Dwmapi
    )
endif()

add_custom_command(TARGET MikuMikuWorld POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/MikuMikuWorld/res"
        "$<TARGET_FILE_DIR:MikuMikuWorld>/res"
)

if(APPLE)
    add_custom_command(TARGET MikuMikuWorld POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f
            "$<TARGET_FILE_DIR:MikuMikuWorld>/app_config.json"
            "$<TARGET_FILE_DIR:MikuMikuWorld>/imgui_config.ini"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:MikuMikuWorld>/auto_save"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:MikuMikuWorld>/library"
        COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:MikuMikuWorld>"
        VERBATIM
    )
endif()

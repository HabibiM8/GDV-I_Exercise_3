cmake_minimum_required(VERSION 3.5)

project(uebung_03 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS OpenGLWidgets REQUIRED)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        openglview.cpp
        trianglemesh.cpp
        utilities.cpp
        shader.cpp
        mainwindow.h
        openglview.h
        trianglemesh.h
        vec3.h
        shader.h
        utilities.h
        renderstate.h
        stb_image.h
)

set(PROJECT_UI
    mainwindow.ui
)

qt_add_executable(uebung_03
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    ${PROJECT_UI}
)

target_link_libraries(uebung_03 PRIVATE Qt6::OpenGLWidgets)

set_target_properties(uebung_03 PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER gris.informatik.tu-darmstadt.de
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

qt_finalize_executable(uebung_03)

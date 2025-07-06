cmake_minimum_required(VERSION 3.16)
project(AutomodellerCPP VERSION 0.1 LANGUAGES C CXX)

# === Платформозависимые пути ===
if(WIN32)
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()


set(CMAKE_PREFIX_PATH
    "C:/vcpkg/vcpkg/installed/x64-windows"
    "C:/Qt/6.9.1/msvc2022_64"
)
# === CMake и Qt настройки ===
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql LinguistTools)
find_package(colmap REQUIRED)
find_package(Eigen3 REQUIRED NO_MODULE)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
set(TS_FILES AutomodellerCPP_en_US.ts)


# === Переводы ===
set(TS_FILES AutomodellerCPP_en_US.ts)



# === Исходники ===
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp mainwindow.h mainwindow.ui
    imageviewer.cpp imageviewer.h
    tools.cpp tools.h
    filesystem.cpp filesystem.h
    amutilities.cpp amutilities.h
    miniz.c
    ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(AutomodellerCPP
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        resources.qrc
    )
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    add_executable(AutomodellerCPP ${PROJECT_SOURCES})
    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_link_libraries(AutomodellerCPP PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
)

set_source_files_properties(miniz.c PROPERTIES LANGUAGE C)

# === Свойства приложения (Windows/macOS) ===
set_target_properties(AutomodellerCPP PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.AutomodellerCPP
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

# === Установка ===
include(GNUInstallDirs)
install(TARGETS AutomodellerCPP
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# === Завершаем Qt экзешник ===
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(AutomodellerCPP)
endif()

# === Авто-деплой Qt DLL через windeployqt ===
if(WIN32)
    # Пытаемся найти windeployqt динамически
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt
        PATHS
            "${Qt${QT_VERSION_MAJOR}_DIR}/../../../bin"
            "$ENV{Qt6_DIR}/../../../bin"
            "$ENV{Qt5_DIR}/../../../bin"
            "C:/Qt"
        REQUIRED
        NO_DEFAULT_PATH
    )

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET AutomodellerCPP POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-quick-import
                --dir "$<TARGET_FILE_DIR:AutomodellerCPP>"
                "$<TARGET_FILE:AutomodellerCPP>"
            COMMENT "Deploying Qt dependencies with windeployqt..."
        )
    else()
        message(WARNING "windeployqt not found — deployment will not run.")
    endif()
endif()

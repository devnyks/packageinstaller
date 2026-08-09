cmake_minimum_required(VERSION 3.21)
project(cachyos-package-installer
        VERSION 1.0.0
        DESCRIPTION "Redesigned CachyOS Package Installer"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent LinguistTools)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(LIBALPM REQUIRED IMPORTED_TARGET libalpm>=13.0.0)
pkg_check_modules(APPSTREAM REQUIRED IMPORTED_TARGET appstream)
find_package(OpenSSL REQUIRED)

file(GLOB TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/lang/*.ts")
if(TS_FILES)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lang")
    set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/lang")
    qt_add_translation(QM_FILES ${TS_FILES})

    # deterministic .qm paths (qt_add_translation derives them the same way)
    set(QM_PATHS)
    foreach(ts ${TS_FILES})
        get_filename_component(ts_name ${ts} NAME)
        string(REPLACE ".ts" ".qm" qm_name "${ts_name}")
        list(APPEND QM_PATHS "${CMAKE_CURRENT_BINARY_DIR}/lang/${qm_name}")
    endforeach()

    # the list is stored in a script file at configure time (passing it via
    # -D would be mangled by the makefile generator's list escaping)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/lang/qm_list.cmake"
        "set(TRANSLATION_FILES ${QM_PATHS})\n")

    # embed the compiled .qm files through a generated resource file that is
    # (re)generated at build time after the .qm files exist
    set(QRC_OUT "${CMAKE_CURRENT_BINARY_DIR}/lang/translations_generated.qrc")
    add_custom_command(
        OUTPUT "${QRC_OUT}"
        DEPENDS ${QM_PATHS}
        COMMAND ${CMAKE_COMMAND}
            -DQM_LIST=${CMAKE_CURRENT_BINARY_DIR}/lang/qm_list.cmake
            -DOUT=${QRC_OUT}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_qrc.cmake
        VERBATIM)
endif()

set(APP_SOURCES
    resources/app.qrc
    ${CMAKE_CURRENT_BINARY_DIR}/lang/translations_generated.qrc
    src/logging.cpp src/logging.h
    src/cmd.cpp src/cmd.h
    src/pacman_conf.cpp src/pacman_conf.h
    src/alpm_manager.cpp src/alpm_manager.h
    src/pacman_cache.cpp src/pacman_cache.h
    src/flatpak.cpp src/flatpak.h
    src/pkglist.cpp src/pkglist.h
    src/appstream_provider.cpp src/appstream_provider.h
    src/versionnumber.h
    src/theme.cpp src/theme.h
    src/models/package_model.cpp src/models/package_model.h
    src/models/flatpak_model.cpp src/models/flatpak_model.h
    src/ui/package_card.cpp src/ui/package_card.h
    src/ui/package_detail.cpp src/ui/package_detail.h
    src/ui/confirm_dialog.cpp src/ui/confirm_dialog.h
    src/ui/remotes_dialog.cpp src/ui/remotes_dialog.h
    src/ui/progress_overlay.cpp src/ui/progress_overlay.h
    src/ui/navbar.cpp src/ui/navbar.h
    src/ui/discover_page.cpp src/ui/discover_page.h
    src/ui/repo_page.cpp src/ui/repo_page.h
    src/ui/flatpak_page.cpp src/ui/flatpak_page.h
    src/ui/console_page.cpp src/ui/console_page.h
    src/ui/settings_page.cpp src/ui/settings_page.h
    src/ui/mainwindow.cpp src/ui/mainwindow.h
    src/ui_test_harness.cpp src/ui_test_harness.h
)

set(APP_LIBS
    Qt6::Widgets Qt6::Network Qt6::Concurrent Threads::Threads OpenSSL::SSL
    PkgConfig::LIBALPM PkgConfig::APPSTREAM)

qt_add_executable(cachyos-pi ${APP_SOURCES} src/main.cpp)
target_include_directories(cachyos-pi PRIVATE src src/models)
target_link_libraries(cachyos-pi PRIVATE ${APP_LIBS})

if(CMAKE_BUILD_TYPE STREQUAL "Release" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cachyos-pi PRIVATE -O2)
endif()

enable_testing()
add_test(NAME ui COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.sh $<TARGET_FILE:cachyos-pi>)

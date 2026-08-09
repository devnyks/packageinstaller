cmake_minimum_required(VERSION 3.22)

project(cachyos-catalog VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(PkgConfig REQUIRED)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Test LinguistTools)
pkg_check_modules(ALPM REQUIRED IMPORTED_TARGET libalpm>=13.0.0)
pkg_check_modules(APPSTREAM REQUIRED IMPORTED_TARGET appstream>=1.0)

if(COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(cachyos-catalog
    src/main.cpp
    src/types.hpp
    src/catalogmodel.hpp src/catalogmodel.cpp
    src/flatpakparser.hpp src/flatpakparser.cpp
    src/curatedcatalog.hpp src/curatedcatalog.cpp
    src/processrunner.hpp src/processrunner.cpp
    src/operationcommands.hpp src/operationcommands.cpp
    src/alpmbackend.hpp src/alpmbackend.cpp
    src/repositoryworker.hpp src/repositoryworker.cpp
    src/appcontroller.hpp src/appcontroller.cpp
)

qt_add_qml_module(cachyos-catalog
    URI CachyOS.Catalog
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/AppCard.qml
        qml/CatalogRow.qml
)

qt_add_resources(cachyos-catalog app_data
    PREFIX "/data"
    FILES resources/pkglist.yaml
)

qt_add_resources(cachyos-catalog app_icons
    PREFIX "/icons"
    FILES resources/cachyos-catalog.svg
)

file(GLOB CATALOG_TS_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/translations/catalog_*.ts")
qt_add_translations(cachyos-catalog
    TS_FILES ${CATALOG_TS_FILES}
    RESOURCE_PREFIX "/i18n"
)

target_include_directories(cachyos-catalog PRIVATE src)
target_link_libraries(cachyos-catalog PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    PkgConfig::ALPM
    PkgConfig::APPSTREAM
)

enable_testing()
qt_add_executable(cachyos-catalog-tests
    tests/test_catalog.cpp
    src/types.hpp
    src/catalogmodel.hpp src/catalogmodel.cpp
    src/flatpakparser.hpp src/flatpakparser.cpp
    src/curatedcatalog.hpp src/curatedcatalog.cpp
    src/processrunner.hpp src/processrunner.cpp
    src/operationcommands.hpp src/operationcommands.cpp
    src/alpmbackend.hpp src/alpmbackend.cpp
    src/repositoryworker.hpp src/repositoryworker.cpp
    src/appcontroller.hpp src/appcontroller.cpp
)
qt_add_resources(cachyos-catalog-tests app_data_test
    PREFIX "/data"
    FILES resources/pkglist.yaml
)
target_include_directories(cachyos-catalog-tests PRIVATE src)
target_link_libraries(cachyos-catalog-tests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Test
    PkgConfig::ALPM
    PkgConfig::APPSTREAM
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cachyos-catalog-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME cachyos-catalog-tests COMMAND cachyos-catalog-tests)
set_tests_properties(cachyos-catalog-tests PROPERTIES
    ENVIRONMENT "CACHYOS_CATALOG_OFFLINE=1;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-cache")
find_program(QMLLINT_EXECUTABLE qmllint)
if(QMLLINT_EXECUTABLE)
    add_test(NAME cachyos-catalog-qml-lint
        COMMAND ${QMLLINT_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/qml/Main.qml
            ${CMAKE_CURRENT_SOURCE_DIR}/qml/Theme.qml
            ${CMAKE_CURRENT_SOURCE_DIR}/qml/AppCard.qml
            ${CMAKE_CURRENT_SOURCE_DIR}/qml/CatalogRow.qml)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cachyos-catalog PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
set(POLKIT_POLICY_FILE "${CMAKE_CURRENT_BINARY_DIR}/org.cachyos.cachyos-catalog.policy")
configure_file(resources/org.cachyos.cachyos-catalog.policy.in "${POLKIT_POLICY_FILE}" @ONLY)
install(TARGETS cachyos-catalog RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES resources/cachyos-catalog.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME cachyos-catalog.svg)
install(FILES resources/cachyos-catalog.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES "${POLKIT_POLICY_FILE}" DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions)
install(FILES resources/pkglist.yaml DESTINATION lib/cachyos-catalog)
install(FILES resources/cachyos-catalog.html resources/license.html LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/cachyos-catalog)

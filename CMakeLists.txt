cmake_minimum_required(VERSION 3.21)

project(lighthouse-pm-kde VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick Bluetooth)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6Config REQUIRED)
find_package(KF6I18n REQUIRED)
find_package(KF6Kirigami REQUIRED)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

add_executable(lighthouse-pm
    src/main.cpp
    src/blemanager.cpp
    src/lighthousedevice.cpp
    src/lighthousev2device.cpp
    src/vivebasestation.cpp
    src/settingsstore.cpp
    src/appmodel.cpp
)

qt6_add_qml_module(lighthouse-pm
    URI lighthousepm
    VERSION 1.0
    QML_FILES
        src/qml/main.qml
        src/qml/MetadataPage.qml
        src/qml/PairPage.qml
        src/qml/SettingsPage.qml
        src/qml/HelpPage.qml
)

target_link_libraries(lighthouse-pm
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Bluetooth
        KF6::CoreAddons
        KF6::ConfigCore
        KF6::I18n
        KF6::Kirigami
)

# QML import path for the Kirigami module (cross build).
set_target_properties(lighthouse-pm PROPERTIES
    QT_QML_IMPORT_PATH "/opt/aarch64-sysroot/usr/lib/qt6/qml"
)

install(TARGETS lighthouse-pm RUNTIME DESTINATION bin)

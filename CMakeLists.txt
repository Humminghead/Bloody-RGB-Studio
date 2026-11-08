cmake_minimum_required(VERSION 3.21)
project(BloodyRGB VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Qml DBus)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

qt_add_executable(bloody-rgb
    src/main.cpp
    src/MainWindow.cpp   src/MainWindow.hpp
    src/KeyboardView.hpp
    src/UsbDevice.cpp    src/UsbDevice.hpp
    src/Protocol.hpp
    src/Effects.hpp
    src/Layout.hpp
    src/Style.hpp
    src/ScreenWatcher.cpp src/ScreenWatcher.hpp
    src/UserEffect.cpp   src/UserEffect.hpp
    src/EffectEditor.cpp src/EffectEditor.hpp)

target_link_libraries(bloody-rgb PRIVATE Qt6::Widgets Qt6::Qml Qt6::DBus PkgConfig::LIBUSB)
target_compile_options(bloody-rgb PRIVATE -Wall -Wextra)

install(TARGETS bloody-rgb RUNTIME DESTINATION bin)
install(FILES packaging/99-bloody.rules DESTINATION lib/udev/rules.d)
install(FILES packaging/bloody-rgb.desktop DESTINATION share/applications)
install(FILES assets/logo.svg DESTINATION share/icons/hicolor/scalable/apps RENAME bloody-rgb.svg)

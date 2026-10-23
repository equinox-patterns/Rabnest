cmake_minimum_required(VERSION 3.15)
project(Rabnest VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent Svg SvgWidgets PrintSupport)
find_package(ZLIB REQUIRED)

# ══════════════════════════════════════════════════════════════════════════
# Trame — socle partagé (bibliothèque STATIQUE), construite dans ce build
# ══════════════════════════════════════════════════════════════════════════
# add_subdirectory HORS ARBRE : le source dir est en ABSOLU et le binary dir
# est EXPLICITE. Sans ce second argument, CMake prendrait
# ${CMAKE_CURRENT_BINARY_DIR}/../Trame, soit un dossier de build hors de
# Rabnest/build/.
add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/../Trame"
    "${CMAKE_CURRENT_BINARY_DIR}/Trame"
)

# ══════════════════════════════════════════════════════════════════════════
# Application
# ══════════════════════════════════════════════════════════════════════════
# Posé APRÈS le add_subdirectory : CMAKE_AUTOMOC se propage aux sous-dossiers
# ajoutés ensuite. Trame n'a aucun Q_OBJECT à mocr — c'est ici, et ici
# seulement, que NestingDialog.h en a un.
set(CMAKE_AUTOMOC ON)

set(CMAKE_AUTORCC ON)

add_executable(Rabnest MACOSX_BUNDLE
    src/main.cpp
    src/app/checklistpage.cpp
    src/app/checklistpage.h
    src/app/importpage.cpp
    src/app/importpage.h
    src/app/layerspage.cpp
    src/app/layerspage.h
    src/app/cutplanpage.cpp
    src/app/cutplanpage.h
    src/app/importoverlay.cpp
    src/app/importoverlay.h
    src/app/outputinfodialog.cpp
    src/app/outputinfodialog.h
    src/app/rabnesttheme.cpp
    src/app/rabnesttheme.h
    src/app/runsdock.cpp
    src/app/runsdock.h
    src/app/rabnestwindow.cpp
    src/app/rabnestwindow.h
    src/app/workspacepage.cpp
    src/app/workspacepage.h
    src/core/cutplan.cpp
    src/core/cutplan.h
    src/core/cutplanrequest.h
    src/core/importeddocument.cpp
    src/core/importeddocument.h
    src/core/layerreducer.cpp
    src/core/layerreducer.h
    src/core/pattern.cpp
    src/core/pattern.h
    src/ui/NestingDialog.cpp
    src/ui/NestingDialog.h
    share/rabnest.qrc
)

set_target_properties(Rabnest PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Rabnest"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.sophiedenys.rabnest"
    MACOSX_BUNDLE_BUNDLE_VERSION "0.3.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.3"
)

# src/ local : NestingDialog.cpp fait #include "ui/NestingDialog.h"
target_include_directories(Rabnest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Trame apporte ${Trame}/src + Qt6::Core/Xml + Clipper2 en PUBLIC, et
# Qt6::Svg/PrintSupport/Gui + ZLIB en PRIVATE (remontés sur la ligne de link
# d'une bibliothèque statique). Rien à répéter ici.
target_link_libraries(Rabnest PRIVATE
    Trame
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Svg
    Qt6::SvgWidgets
    Qt6::PrintSupport
    ZLIB::ZLIB
)

# The layer reducer, as a tool: the same code the window calls, reachable without a click.
add_executable(rabnest_reduce
    src/tools/reduce.cpp
    src/core/layerreducer.cpp
    src/core/layerreducer.h
)
target_include_directories(rabnest_reduce PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(rabnest_reduce PRIVATE Qt6::Core ZLIB::ZLIB)

# The plan's figures without a click — it writes nothing, it prints a height.
add_executable(rabnest_plan
    src/tools/plan.cpp
    src/core/cutplan.cpp
    src/core/importeddocument.cpp
    src/core/layerreducer.cpp
    src/core/pattern.cpp
)
target_include_directories(rabnest_plan PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(rabnest_plan PRIVATE Trame Qt6::Widgets Qt6::Svg ZLIB::ZLIB)

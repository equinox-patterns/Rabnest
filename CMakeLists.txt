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
    src/app/busydialog.cpp
    src/app/busydialog.h
    src/app/importoverlay.cpp
    src/app/importoverlay.h
    src/app/outputinfodialog.cpp
    src/app/outputinfodialog.h
    src/app/exportimagedialog.cpp
    src/app/exportimagedialog.h
    src/app/exportpdfdialog.cpp
    src/app/exportpdfdialog.h
    src/app/printplandialog.cpp
    src/app/printplandialog.h
    src/app/printrundialog.cpp
    src/app/printrundialog.h
    src/app/factorywidgets.h
    src/app/rabnesttheme.cpp
    src/app/rabnesttheme.h
    src/app/runsdock.cpp
    src/app/runsdock.h
    src/app/rabnestwindow.cpp
    src/app/rabnestwindow.h
    src/app/workspacepage.cpp
    src/app/workspacepage.h
    src/app/print/exportmarkpainter.cpp
    src/app/print/exportmarkpainter.h
    src/app/print/printitems.cpp
    src/app/print/printitems.h
    src/app/print/printpanel.cpp
    src/app/print/printpanel.h
    src/core/cutplan.cpp
    src/core/cutplan.h
    src/core/export/planimage.cpp
    src/core/export/planimage.h
    src/core/export/planimagewriter.cpp
    src/core/export/planimagewriter.h
    src/core/export/plansheet.cpp
    src/core/export/plansheet.h
    src/core/cutplanrequest.h
    src/core/importeddocument.cpp
    src/core/importeddocument.h
    src/core/layerreducer.cpp
    src/core/layerreducer.h
    src/core/pattern.cpp
    src/core/pattern.h
    src/core/print/dashpen.h
    src/core/print/exportartboards.cpp
    src/core/print/exportartboards.h
    src/core/print/exportcoverpage.cpp
    src/core/print/exportcoverpage.h
    src/core/print/exportframesettings.h
    src/core/print/exportmarksettings.cpp
    src/core/print/exportmarksettings.h
    src/core/print/exportpaginationsettings.cpp
    src/core/print/exportpaginationsettings.h
    src/core/print/exportpaper.cpp
    src/core/print/exportpaper.h
    src/core/print/exportprintmodel.cpp
    src/core/print/exportprintmodel.h
    src/core/print/exportsheets.cpp
    src/core/print/exportsheets.h
    src/core/print/exporttemplate.cpp
    src/core/print/exporttemplate.h
    src/core/print/printmetrics.h
    src/core/print/printpdf.cpp
    src/core/print/printpdf.h
    src/core/print/printplan.cpp
    src/core/print/printplan.h
    src/core/print/printrecipe.cpp
    src/core/print/printrecipe.h
    src/ui/GroupOverlapDialog.cpp
    src/ui/GroupOverlapDialog.h
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

# The print run's figures without a click — it writes nothing, it prints a tiling.
add_executable(rabnest_print
    src/tools/print.cpp
    src/app/print/exportmarkpainter.cpp
    src/app/print/printpanel.cpp
    src/core/importeddocument.cpp
    src/core/layerreducer.cpp
    src/core/pattern.cpp
    src/core/print/exportartboards.cpp
    src/core/print/exportcoverpage.cpp
    src/core/print/exportmarksettings.cpp
    src/core/print/exportpaginationsettings.cpp
    src/core/print/exportprintmodel.cpp
    src/core/print/exportsheets.cpp
    src/core/print/exporttemplate.cpp
    src/core/print/printplan.cpp
    src/core/print/printpdf.cpp
    src/core/print/printrecipe.cpp
    share/rabnest.qrc
)
target_include_directories(rabnest_print PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
# PrintSupport : `--svg=` écrit par le MÊME écrivain que le PDF, qui peint aussi sur une imprimante.
target_link_libraries(rabnest_print PRIVATE Trame Qt6::Widgets Qt6::Svg Qt6::PrintSupport
                                          ZLIB::ZLIB)

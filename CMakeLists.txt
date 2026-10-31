cmake_minimum_required(VERSION 3.15)
project(Rabnest VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent Svg SvgWidgets PrintSupport LinguistTools)
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
    src/app/aboutdialog.cpp
    src/app/aboutdialog.h
    src/app/licencedialog.cpp
    src/app/licencedialog.h
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
    src/app/rabnestlanguage.cpp
    src/app/rabnestlanguage.h
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
    src/core/projector.cpp
    src/core/projector.h
    src/ui/GroupOverlapDialog.cpp
    src/ui/GroupOverlapDialog.h
    src/ui/NestingDialog.cpp
    src/ui/NestingDialog.h
    share/rabnest.qrc
)

set_target_properties(Rabnest PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Rabnest"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.sophiedenys.rabnest"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "Copyright © 2026 Sophie Denys"
    MACOSX_BUNDLE TRUE
    # L'icône du bundle : le .icns est copié dans Contents/Resources juste après la construction,
    # et la clé du plist la nomme.
    MACOSX_BUNDLE_ICON_FILE "rabnest.icns"
    # ⚠️ **Sans ce plist, l'interface est en anglais sur une machine française.** macOS donne sa
    # langue au processus d'après les langues que le bundle déclare parler ; un bundle qui n'en
    # déclare aucune est réputé ne parler que l'anglais, et `QLocale::system()` répond `en_US` quoi
    # que dise le système. Le plist porte donc les dix-sept codes de RABNEST_LOCALES.
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
    # Le bundle installé cherche ses frameworks à côté de lui. Posé ici pour que `cmake --install`
    # l'écrive avant macdeployqt : sans rpath, macdeployqt copie les frameworks mais laisse le
    # binaire incapable de les trouver.
    INSTALL_RPATH "@executable_path/../Frameworks"
)

# src/ local : NestingDialog.cpp fait #include "ui/NestingDialog.h"
target_include_directories(Rabnest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# La version vient du project(), une seule fois : le bundle, le plist et
# QCoreApplication::applicationVersion() lisent tous les trois la même valeur.
target_compile_definitions(Rabnest PRIVATE RABNEST_VERSION="${PROJECT_VERSION}")

# ── Windows ────────────────────────────────────────────────────────────────────────────────────
# L'icône n'est pas posée à côté de l'exécutable : elle est compilée dedans par le script de
# ressources, sinon l'explorateur affiche l'icône générique. Et WIN32_EXECUTABLE évite la console
# noire qui s'ouvre derrière la fenêtre.
if(WIN32)
    set_target_properties(Rabnest PROPERTIES WIN32_EXECUTABLE TRUE)
    target_sources(Rabnest PRIVATE share/icons/app/rabnest.rc)
    set_source_files_properties(share/icons/app/rabnest.rc PROPERTIES
        COMPILE_FLAGS "-I\"${CMAKE_CURRENT_SOURCE_DIR}/share/icons/app\"")
endif()

# ── Linux ──────────────────────────────────────────────────────────────────────────────────────
# Rien n'est compilé dedans : l'icône s'installe dans l'arbre hicolor et le fichier .desktop la
# nomme. Le gestionnaire de fenêtres la trouve par ce nom, jamais par un chemin.
if(UNIX AND NOT APPLE)
    install(TARGETS Rabnest RUNTIME DESTINATION bin)
    install(FILES share/rabnest.desktop DESTINATION share/applications)
    foreach(icon_size 16 24 32 48 64 128 256 512)
        install(FILES share/icons/hicolor/${icon_size}x${icon_size}/apps/rabnest.png
            DESTINATION share/icons/hicolor/${icon_size}x${icon_size}/apps)
    endforeach()
    install(FILES share/icons/app/rabnest.svg DESTINATION share/icons/hicolor/scalable/apps)

    # Les textes de licence en vrais fichiers, comme dans le paquet macOS : le .qrc les rend
    # lisibles dans l'application, ces fichiers les rendent lisibles sans l'ouvrir.
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE"
                  "${CMAKE_CURRENT_SOURCE_DIR}/AUTHORS.txt"
                  "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        DESTINATION "share/doc/rabnest"
    )
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/share/licenses/"
        DESTINATION "share/doc/rabnest/licenses"
        FILES_MATCHING PATTERN "*.txt"
    )
endif()


# Trame apporte ${Trame}/src + Qt6::Core/Xml + Clipper2 en PUBLIC, et
# Qt6::Svg/PrintSupport/Gui + ZLIB en PRIVATE (remontés sur la ligne de link
# d'une bibliothèque statique). Rien à répéter ici.
# Les textes de licence, en VRAIS fichiers dans le paquet, à côté de leur copie embarquée dans le
# binaire. La licence MIT des icônes et la SIL Open Font License de la police exigent l'une et
# l'autre que leur texte accompagne l'application ; l'OFL dit même explicitement que des fichiers
# texte séparés suffisent. Le dialogue les lit dans le binaire, ces fichiers sont là pour la lettre
# de la licence — et pour qui veut les lire sans ouvrir l'application.
if(APPLE)
    # ⚠️ **Une commande POST_BUILD ne tourne qu'au moment où la cible est reliée.** Sans cette
    # dépendance, remplacer le .icns sans toucher au code laissait l'ancienne icône dans le paquet,
    # et le Finder montrait celle d'avant. Le fichier entre donc dans les sources de la cible, en
    # HEADER_FILE_ONLY : il ne se compile pas, mais son changement déclenche la liaison, donc la copie.
    set_source_files_properties(share/icons/app/rabnest.icns PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(Rabnest PRIVATE share/icons/app/rabnest.icns)

    add_custom_command(TARGET Rabnest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:Rabnest>/Contents/Resources/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/share/icons/app/rabnest.icns"
            "$<TARGET_BUNDLE_DIR:Rabnest>/Contents/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/share/licenses/GPL-3.0.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/share/licenses/phosphor-MIT.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/share/licenses/inter-OFL.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/share/licenses/clipper2-BSL-1.0.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/share/licenses/zlib.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/share/licenses/LGPL-3.0.txt"
            "$<TARGET_BUNDLE_DIR:Rabnest>/Contents/Resources/licenses"
        COMMENT "Textes de licence dans le paquet"
        VERBATIM)

    # ══════════════════════════════════════════════════════════════════════════════════════════
    # Le bundle distribuable.
    #
    # Le paquet de l'arbre de construction ne contient QUE l'exécutable : il se lance sur cette
    # machine parce que Qt est dans ~/Qt, et sur aucune autre. `cmake --install` copie le bundle,
    # puis `qt_generate_deploy_app_script` fait passer macdeployqt : les frameworks Qt entrent
    # dans Contents/Frameworks et les rpaths sont réécrits vers @executable_path/../Frameworks.
    # C'est ce bundle-là que deploy.sh synchronise, pas celui de build/.
    # ══════════════════════════════════════════════════════════════════════════════════════════
    install(TARGETS Rabnest BUNDLE DESTINATION .)

    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/share/icons/app/rabnest.icns"
        DESTINATION "Rabnest.app/Contents/Resources"
    )
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/share/licenses/"
        DESTINATION "Rabnest.app/Contents/Resources/licenses"
        FILES_MATCHING PATTERN "*.txt"
    )
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE"
                  "${CMAKE_CURRENT_SOURCE_DIR}/AUTHORS.txt"
        DESTINATION "Rabnest.app/Contents/Resources"
    )

    # macdeployqt, appelé directement. Le script engendré par qt_generate_deploy_app_script ne
    # passe pas -libpath, et sur ce bundle il échoue à résoudre @rpath/QtCore : il copie les
    # frameworks mais laisse le binaire sans rpath. Avec le préfixe Qt, il réécrit le rpath en
    # @executable_path/../Frameworks et le paquet devient autonome.
    get_target_property(_rabnest_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_rabnest_qt_bin "${_rabnest_qmake}" DIRECTORY)
    get_filename_component(_rabnest_qt_lib "${_rabnest_qt_bin}/../lib" ABSOLUTE)

    install(CODE "
        execute_process(
            COMMAND \"${_rabnest_qt_bin}/macdeployqt\" \"\${CMAKE_INSTALL_PREFIX}/Rabnest.app\"
                    \"-libpath=${_rabnest_qt_lib}\" -always-overwrite
            RESULT_VARIABLE _rabnest_deploy_rc
            OUTPUT_VARIABLE _rabnest_deploy_out
            ERROR_VARIABLE _rabnest_deploy_out)
        if(NOT _rabnest_deploy_rc EQUAL 0)
            message(FATAL_ERROR \"macdeployqt a échoué : \${_rabnest_deploy_out}\")
        endif()
    ")
endif()

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
    src/core/projector.cpp
    src/core/projector.h
    share/rabnest.qrc
)
target_include_directories(rabnest_print PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
# PrintSupport : `--svg=` écrit par le MÊME écrivain que le PDF, qui peint aussi sur une imprimante.
target_link_libraries(rabnest_print PRIVATE Trame Qt6::Widgets Qt6::Svg Qt6::PrintSupport
                                          ZLIB::ZLIB)

# ══════════════════════════════════════════════════════════════════════════
# Traductions — embarquées dans le binaire (ressources Qt), comme dans Ostara
# MERGE_QT_TRANSLATIONS fusionne les traductions de Qt lui-même dans les .qm
# de l'application : un seul traducteur à installer, et les dialogues natifs
# (QFileDialog, les boutons Yes/No) parlent aussi la langue choisie.
# ══════════════════════════════════════════════════════════════════════════
set(RABNEST_TS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/share/translations")

# Les dix-sept langues d'Ostara. L'anglais n'est pas ici : c'est la langue des
# chaînes sources, il n'a pas de fichier.
set(RABNEST_LOCALES
    cs_CZ de_DE el_GR es_ES fi_FI fr_FR hr_HR hu_HU
    id_ID it_IT ja_JP nl_NL pl_PL pt_BR ro_RO uk_UA zh_CN
)

set(RABNEST_TS_FILES)
foreach(locale ${RABNEST_LOCALES})
    list(APPEND RABNEST_TS_FILES ${RABNEST_TS_DIR}/rabnest_${locale}.ts)
endforeach()

foreach(locale ${RABNEST_LOCALES})
    set_source_files_properties(
        ${RABNEST_TS_DIR}/rabnest_${locale}.ts
        PROPERTIES QT_I18N_TRANSLATED_LANGUAGES ${locale}
    )
endforeach()

# Certaines locales n'ont pas de catalogue Qt (id_ID, ro_RO, el_GR) : ce n'est
# pas une erreur, c'est un dialogue natif qui restera en anglais.
set(QT_NO_MISSING_CATALOG_LANGUAGE_WARNING ON)

qt_add_translations(Rabnest
    TS_FILES ${RABNEST_TS_FILES}
    MERGE_QT_TRANSLATIONS
)

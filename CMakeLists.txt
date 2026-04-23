cmake_minimum_required(VERSION 3.16)

project(deepin-terminal-ghostty
  VERSION 0.3.0
  LANGUAGES CXX
)

add_compile_definitions(PROJECT_VERSION_STR="${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_BUILD_RPATH "$ORIGIN")
set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
set(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH OFF)

set(GHOSTTY_INCLUDE_DIR "" CACHE PATH
  "Path to the Ghostty headers")
set(GHOSTTY_STAGED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/ghostty-include")

find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Dtk6 COMPONENTS Widget Core Gui REQUIRED)
find_package(Dtk6Tools REQUIRED)

DTK_CREATE_I18N_FROM_JSON(APP_SETTINGS_I18N_SOURCES
  src/app/settings/default-config.json
  settings_translation.cpp
)

set(APP_TRANSLATABLE_SOURCES
  src/app/main.cpp
  src/app/ApplicationMetadata.cpp
  src/app/MainWindow.cpp
  src/app/AppSettings.cpp
  src/app/SettingsDialog.cpp
  src/app/TermPane.cpp
  src/app/VerticalTabSidebar.cpp
  src/app/remote/ServerConfigManager.cpp
  src/app/remote/ServerConfigOptDlg.cpp
  src/app/remote/RemoteManagementPanel.cpp
  src/app/remote/ServerListItem.cpp
  src/libqtghostty/TerminalWidget.cpp
  ${APP_SETTINGS_I18N_SOURCES}
)

set(APP_TRANSLATION_TS_FILES
  translations/deepin-terminal-ghostty_zh_CN.ts
  translations/deepin-terminal-ghostty_es.ts
)

qt_add_lrelease(TS_FILES
  ${APP_TRANSLATION_TS_FILES}
  QM_FILES_OUTPUT_VARIABLE APP_TRANSLATION_QM_FILES
)

add_custom_target(update_translations
  COMMAND $<TARGET_FILE:Qt6::lupdate> ${APP_TRANSLATABLE_SOURCES} -ts ${APP_TRANSLATION_TS_FILES}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ${APP_TRANSLATABLE_SOURCES} ${APP_TRANSLATION_TS_FILES}
  COMMENT "Updating translation source files"
  VERBATIM
)

add_custom_target(app_translations DEPENDS ${APP_TRANSLATION_QM_FILES})

set(GHOSTTY_SOURCE_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/lib/libghostty-vt.so")
if(NOT EXISTS "${GHOSTTY_SOURCE_LIBRARY}")
  message(FATAL_ERROR "Missing Ghostty runtime library: ${GHOSTTY_SOURCE_LIBRARY}")
endif()
set_property(DIRECTORY APPEND PROPERTY
  CMAKE_CONFIGURE_DEPENDS "${GHOSTTY_SOURCE_LIBRARY}"
)

set(_ghostty_include_source "")
if(GHOSTTY_INCLUDE_DIR)
  if(IS_DIRECTORY "${GHOSTTY_INCLUDE_DIR}")
    set(_ghostty_include_source "${GHOSTTY_INCLUDE_DIR}")
  else()
    message(FATAL_ERROR
      "GHOSTTY_INCLUDE_DIR is set but does not exist: ${GHOSTTY_INCLUDE_DIR}")
  endif()
elseif(DEFINED ENV{GHOSTTY_INCLUDE_DIR} AND NOT "$ENV{GHOSTTY_INCLUDE_DIR}" STREQUAL "")
  if(IS_DIRECTORY "$ENV{GHOSTTY_INCLUDE_DIR}")
    set(_ghostty_include_source "$ENV{GHOSTTY_INCLUDE_DIR}")
  else()
    message(FATAL_ERROR
      "Environment GHOSTTY_INCLUDE_DIR points to a missing directory: $ENV{GHOSTTY_INCLUDE_DIR}")
  endif()
else()
  foreach(_ghostty_candidate IN ITEMS
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../g/ghostty/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ghostty/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/ghostty/include"
    "/usr/include/ghostty"
    "/usr/local/include/ghostty"
  )
    if(IS_DIRECTORY "${_ghostty_candidate}")
      set(_ghostty_include_source "${_ghostty_candidate}")
      break()
    endif()
  endforeach()
endif()
if(_ghostty_include_source STREQUAL "")
  message(FATAL_ERROR
    "Could not locate Ghostty headers. Set GHOSTTY_INCLUDE_DIR to the include root.")
endif()
set(GHOSTTY_INCLUDE_DIR "${_ghostty_include_source}" CACHE PATH
  "Path to the Ghostty headers" FORCE)

if(NOT EXISTS "${GHOSTTY_INCLUDE_DIR}/ghostty/vt/build_info.h")
  message(FATAL_ERROR
    "Ghostty headers at ${GHOSTTY_INCLUDE_DIR} do not contain ghostty/vt/build_info.h")
endif()

set(GHOSTTY_SONAME "" CACHE STRING
  "Ghostty SONAME (leave empty to auto-detect with readelf)")
find_program(READELF_EXECUTABLE readelf)
if(GHOSTTY_SONAME STREQUAL "" AND READELF_EXECUTABLE)
  execute_process(
    COMMAND "${READELF_EXECUTABLE}" -d "${GHOSTTY_SOURCE_LIBRARY}"
    OUTPUT_VARIABLE GHOSTTY_SOURCE_LIBRARY_DYNAMIC
    RESULT_VARIABLE GHOSTTY_SOURCE_LIBRARY_DYNAMIC_RESULT
  )
  if(GHOSTTY_SOURCE_LIBRARY_DYNAMIC_RESULT EQUAL 0)
    string(REGEX MATCH "SONAME.*\\[([^]]+)\\]" _ghostty_soname_match
      "${GHOSTTY_SOURCE_LIBRARY_DYNAMIC}")
    if(CMAKE_MATCH_1)
      set(GHOSTTY_SONAME "${CMAKE_MATCH_1}")
    endif()
  endif()
endif()
if(GHOSTTY_SONAME STREQUAL "")
  message(FATAL_ERROR
    "Could not determine Ghostty SONAME for ${GHOSTTY_SOURCE_LIBRARY}. "
    "Set GHOSTTY_SONAME explicitly.")
endif()
set(GHOSTTY_STAGED_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/${GHOSTTY_SONAME}")

file(GLOB_RECURSE GHOSTTY_HEADER_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false
  "${GHOSTTY_INCLUDE_DIR}/*"
)

set(GHOSTTY_HEADER_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/ghostty-headers.manifest")
file(WRITE "${GHOSTTY_HEADER_MANIFEST}" "")
foreach(_ghostty_header IN LISTS GHOSTTY_HEADER_SOURCES)
  file(APPEND "${GHOSTTY_HEADER_MANIFEST}" "${_ghostty_header}\n")
endforeach()

set(GHOSTTY_STAGED_INCLUDE_STAMP "${CMAKE_CURRENT_BINARY_DIR}/ghostty-include.stamp")

add_custom_command(
  OUTPUT "${GHOSTTY_STAGED_INCLUDE_STAMP}"
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${GHOSTTY_STAGED_INCLUDE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${GHOSTTY_INCLUDE_DIR}"
          "${GHOSTTY_STAGED_INCLUDE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E touch "${GHOSTTY_STAGED_INCLUDE_STAMP}"
  DEPENDS
    "${GHOSTTY_HEADER_MANIFEST}"
    ${GHOSTTY_HEADER_SOURCES}
  COMMENT "Staging Ghostty headers"
  VERBATIM
)

add_custom_target(ghostty-headers-stage DEPENDS "${GHOSTTY_STAGED_INCLUDE_STAMP}")

add_custom_command(
  OUTPUT "${GHOSTTY_STAGED_LIBRARY}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${GHOSTTY_SOURCE_LIBRARY}"
          "${GHOSTTY_STAGED_LIBRARY}"
  DEPENDS "${GHOSTTY_SOURCE_LIBRARY}"
  VERBATIM
)

add_custom_target(ghostty-vt-stage DEPENDS "${GHOSTTY_STAGED_LIBRARY}")

add_library(ghostty-vt SHARED IMPORTED)
set_target_properties(ghostty-vt PROPERTIES
  IMPORTED_LOCATION "${GHOSTTY_STAGED_LIBRARY}"
)

add_library(qtghostty SHARED
  src/logging/Logging.cpp
  src/libqtghostty/PtySession.cpp
  src/libqtghostty/TerminalWidget.cpp
)

target_include_directories(qtghostty PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/libqtghostty>"
  "$<INSTALL_INTERFACE:include>"
  "${GHOSTTY_STAGED_INCLUDE_DIR}"
)

target_link_libraries(qtghostty PUBLIC
  Qt6::Widgets
  Dtk6::Widget
  Dtk6::Core
  Dtk6::Gui
  ghostty-vt
  util
)

add_dependencies(qtghostty ghostty-vt-stage ghostty-headers-stage)

set_target_properties(qtghostty PROPERTIES
  OUTPUT_NAME qtghostty
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
)

add_executable(deepin-terminal-ghostty
  src/app/ApplicationMetadata.cpp
  src/app/StartupOptions.cpp
  src/app/main.cpp
  src/app/MainWindow.cpp
  src/app/AppSettings.cpp
  src/app/SettingsDialog.cpp
  src/app/TermPane.cpp
  src/app/VerticalTabSidebar.cpp
  src/app/PageSearchBar.cpp
  src/app/remote/ServerConfigManager.cpp
  src/app/remote/ServerConfigOptDlg.cpp
  src/app/remote/RemoteManagementPanel.cpp
  src/app/remote/ServerListItem.cpp
  src/app/settings/resources.qrc
)

target_link_libraries(deepin-terminal-ghostty PRIVATE
  qtghostty
  Dtk6::Widget
  Dtk6::Core
  Dtk6::Gui
)

add_dependencies(deepin-terminal-ghostty ghostty-vt-stage ghostty-headers-stage)
add_dependencies(deepin-terminal-ghostty app_translations)

add_custom_command(TARGET deepin-terminal-ghostty POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${GHOSTTY_STAGED_LIBRARY}"
          "$<TARGET_FILE_DIR:deepin-terminal-ghostty>/${GHOSTTY_SONAME}"
  VERBATIM
)

add_custom_command(TARGET deepin-terminal-ghostty POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_FILE:qtghostty>"
          "$<TARGET_FILE_DIR:deepin-terminal-ghostty>/"
  VERBATIM
)

add_custom_command(TARGET deepin-terminal-ghostty POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "$<TARGET_FILE_DIR:deepin-terminal-ghostty>/translations"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${APP_TRANSLATION_QM_FILES}
          "$<TARGET_FILE_DIR:deepin-terminal-ghostty>/translations"
  VERBATIM
)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

set_target_properties(deepin-terminal-ghostty PROPERTIES
  INSTALL_RPATH ""
)
set_target_properties(qtghostty PROPERTIES
  INSTALL_RPATH ""
)

install(TARGETS deepin-terminal-ghostty
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS qtghostty
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES ${GHOSTTY_SOURCE_LIBRARY}
  DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RENAME ${GHOSTTY_SONAME}
)

install(FILES
  src/libqtghostty/PtySession.h
  src/libqtghostty/TerminalWidget.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qtghostty
)

install(FILES ${APP_TRANSLATION_QM_FILES}
  DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/translations
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
option(BUILD_TESTING "Build tests" OFF)
include(CTest)
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

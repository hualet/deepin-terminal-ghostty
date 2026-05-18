find_package(Qt6 REQUIRED COMPONENTS Test)

include_directories(
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/libqtghostty
  ${CMAKE_SOURCE_DIR}/src/app
  ${GHOSTTY_STAGED_INCLUDE_DIR}
)

# ---------------------------------------------------------------------------
# test_appsettings
# ---------------------------------------------------------------------------
add_executable(test_appsettings
  test_appsettings.cpp
  ${CMAKE_SOURCE_DIR}/src/app/AppSettings.cpp
  ${CMAKE_SOURCE_DIR}/src/app/ThemeLoader.cpp
  ${CMAKE_SOURCE_DIR}/src/app/settings/resources.qrc
)

target_link_libraries(test_appsettings PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Test
  Dtk6::Core
)

add_test(NAME AppSettings COMMAND test_appsettings)

# ---------------------------------------------------------------------------
# test_startup_options
# ---------------------------------------------------------------------------
add_executable(test_startup_options
  test_startup_options.cpp
  ${CMAKE_SOURCE_DIR}/src/app/StartupOptions.cpp
)

target_link_libraries(test_startup_options PRIVATE
  Qt6::Core
  Qt6::Test
)

add_test(NAME StartupOptions COMMAND test_startup_options)

# ---------------------------------------------------------------------------
# test_pty_session
# ---------------------------------------------------------------------------
add_executable(test_pty_session
  test_pty_session.cpp
  ${CMAKE_SOURCE_DIR}/src/logging/Logging.cpp
  ${CMAKE_SOURCE_DIR}/src/libqtghostty/PtySession.cpp
)

target_link_libraries(test_pty_session PRIVATE
  Qt6::Core
  Qt6::Test
  Dtk6::Core
  util
)

add_dependencies(test_pty_session ghostty-headers-stage)

add_test(NAME PtySession COMMAND test_pty_session)

# ---------------------------------------------------------------------------
# test_terminal_widget
# ---------------------------------------------------------------------------
add_executable(test_terminal_widget
  test_terminal_widget.cpp
  ${CMAKE_SOURCE_DIR}/src/logging/Logging.cpp
  ${CMAKE_SOURCE_DIR}/src/libqtghostty/PtySession.cpp
  ${CMAKE_SOURCE_DIR}/src/libqtghostty/TerminalWidget.cpp
)

target_link_libraries(test_terminal_widget PRIVATE
  Qt6::Widgets
  Qt6::Test
  Dtk6::Core
  ghostty-vt
  util
)

target_compile_definitions(test_terminal_widget PRIVATE QTGHOSTTY_TESTING)

add_dependencies(test_terminal_widget ghostty-vt-stage ghostty-headers-stage)

# Stage libghostty-vt.so next to the test binary so it resolves at runtime
add_custom_command(TARGET test_terminal_widget POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${GHOSTTY_STAGED_LIBRARY}"
          "$<TARGET_FILE_DIR:test_terminal_widget>/${GHOSTTY_SONAME}"
  VERBATIM
)

add_test(NAME TerminalWidget COMMAND test_terminal_widget)

# ---------------------------------------------------------------------------
# test_main_window
# ---------------------------------------------------------------------------
add_executable(test_main_window
  test_main_window.cpp
  ${CMAKE_SOURCE_DIR}/src/app/ApplicationMetadata.cpp
  ${CMAKE_SOURCE_DIR}/src/app/StartupOptions.cpp
  ${CMAKE_SOURCE_DIR}/src/logging/Logging.cpp
  ${CMAKE_SOURCE_DIR}/src/app/MainWindow.cpp
  ${CMAKE_SOURCE_DIR}/src/app/QuakeWindow.cpp
  ${CMAKE_SOURCE_DIR}/src/app/AppSettings.cpp
  ${CMAKE_SOURCE_DIR}/src/app/ThemeLoader.cpp
  ${CMAKE_SOURCE_DIR}/src/app/SettingsDialog.cpp
  ${CMAKE_SOURCE_DIR}/src/app/TerminalScrollContainer.cpp
  ${CMAKE_SOURCE_DIR}/src/app/TermPane.cpp
  ${CMAKE_SOURCE_DIR}/src/app/VerticalTabSidebar.cpp
  ${CMAKE_SOURCE_DIR}/src/app/PageSearchBar.cpp
  ${CMAKE_SOURCE_DIR}/src/app/SessionManager.cpp
  ${CMAKE_SOURCE_DIR}/src/app/SessionSnapshot.cpp
  ${CMAKE_SOURCE_DIR}/src/app/remote/ServerConfigManager.cpp
  ${CMAKE_SOURCE_DIR}/src/app/remote/ServerConfigOptDlg.cpp
  ${CMAKE_SOURCE_DIR}/src/app/remote/RemoteManagementPanel.cpp
  ${CMAKE_SOURCE_DIR}/src/app/remote/ServerListItem.cpp
  ${CMAKE_SOURCE_DIR}/src/app/settings/resources.qrc
  ${CMAKE_SOURCE_DIR}/src/libqtghostty/PtySession.cpp
  ${CMAKE_SOURCE_DIR}/src/libqtghostty/TerminalWidget.cpp
)

target_compile_definitions(test_main_window PRIVATE QTGHOSTTY_TESTING)

target_include_directories(test_main_window PRIVATE
  ${CMAKE_SOURCE_DIR}/src/app
)

target_link_libraries(test_main_window PRIVATE
  Qt6::Widgets
  Qt6::Test
  Dtk6::Widget
  Dtk6::Core
  Dtk6::Gui
  ghostty-vt
  util
)

add_dependencies(test_main_window ghostty-vt-stage ghostty-headers-stage)

add_custom_command(TARGET test_main_window POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${GHOSTTY_STAGED_LIBRARY}"
          "$<TARGET_FILE_DIR:test_main_window>/${GHOSTTY_SONAME}"
  VERBATIM
)

add_test(NAME MainWindow COMMAND test_main_window)

# ---------------------------------------------------------------------------
# test_server_config_manager
# ---------------------------------------------------------------------------
add_executable(test_server_config_manager
  test_server_config_manager.cpp
  ${CMAKE_SOURCE_DIR}/src/app/remote/ServerConfigManager.cpp
)

target_include_directories(test_server_config_manager PRIVATE
  ${CMAKE_SOURCE_DIR}/src/app
)

target_link_libraries(test_server_config_manager PRIVATE
  Qt6::Core
  Qt6::Test
)

add_test(NAME ServerConfigManager COMMAND test_server_config_manager)

# ---------------------------------------------------------------------------
# test_session_snapshot
# ---------------------------------------------------------------------------
add_executable(test_session_snapshot
  test_session_snapshot.cpp
  ${CMAKE_SOURCE_DIR}/src/app/SessionSnapshot.cpp
)

target_link_libraries(test_session_snapshot PRIVATE
  Qt6::Core
  Qt6::Test
)

add_test(NAME SessionSnapshot COMMAND test_session_snapshot)

# ---------------------------------------------------------------------------
# test_session_manager
# ---------------------------------------------------------------------------
add_executable(test_session_manager
  test_session_manager.cpp
  ${CMAKE_SOURCE_DIR}/src/app/SessionManager.cpp
  ${CMAKE_SOURCE_DIR}/src/app/SessionSnapshot.cpp
  ${CMAKE_SOURCE_DIR}/src/logging/Logging.cpp
  ${CMAKE_SOURCE_DIR}/src/libqtghostty/PtySession.cpp
  ${CMAKE_SOURCE_DIR}/src/libqtghostty/TerminalWidget.cpp
)

target_include_directories(test_session_manager PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/app
  ${GHOSTTY_STAGED_INCLUDE_DIR}
)

target_link_libraries(test_session_manager PRIVATE
  Qt6::Widgets
  Qt6::Test
  Dtk6::Core
  ghostty-vt
  util
)

add_dependencies(test_session_manager ghostty-vt-stage ghostty-headers-stage)

add_custom_command(TARGET test_session_manager POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${GHOSTTY_STAGED_LIBRARY}"
          "$<TARGET_FILE_DIR:test_session_manager>/${GHOSTTY_SONAME}"
  VERBATIM
)

add_test(NAME SessionManager COMMAND test_session_manager)

add_test(NAME StartupCommandIntegration
  COMMAND /bin/bash ${CMAKE_SOURCE_DIR}/tests/test_startup_command_integration.sh
)

add_test(NAME EsctestWrapperSmoke
  COMMAND /bin/bash ${CMAKE_SOURCE_DIR}/tests/test_esctest_wrapper.sh
)

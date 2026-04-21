find_package(Qt6 REQUIRED COMPONENTS Test)

include_directories(
  ${CMAKE_SOURCE_DIR}/src/libqtghostty
  ${GHOSTTY_STAGED_INCLUDE_DIR}
)

# ---------------------------------------------------------------------------
# test_pty_session
# ---------------------------------------------------------------------------
add_executable(test_pty_session
  test_pty_session.cpp
  ${CMAKE_SOURCE_DIR}/src/libqtghostty/PtySession.cpp
)

target_link_libraries(test_pty_session PRIVATE
  Qt6::Core
  Qt6::Test
  util
)

add_dependencies(test_pty_session ghostty-headers-stage)

add_test(NAME PtySession COMMAND test_pty_session)

# ---------------------------------------------------------------------------
# test_terminal_widget
# ---------------------------------------------------------------------------
add_executable(test_terminal_widget
  test_terminal_widget.cpp
  ${CMAKE_SOURCE_DIR}/src/libqtghostty/PtySession.cpp
  ${CMAKE_SOURCE_DIR}/src/libqtghostty/TerminalWidget.cpp
)

target_link_libraries(test_terminal_widget PRIVATE
  Qt6::Widgets
  Qt6::Test
  ghostty-vt
  util
)

add_dependencies(test_terminal_widget ghostty-vt-stage ghostty-headers-stage)

# Stage libghostty-vt.so next to the test binary so it resolves at runtime
add_custom_command(TARGET test_terminal_widget POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${GHOSTTY_STAGED_LIBRARY}"
          "$<TARGET_FILE_DIR:test_terminal_widget>/${GHOSTTY_SONAME}"
  VERBATIM
)

add_test(NAME TerminalWidget COMMAND test_terminal_widget)

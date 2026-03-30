find_package(Qt5 REQUIRED COMPONENTS Test Widgets)
find_package(GTest QUIET)

enable_testing()

# Source files needed by tests (core + utils, no UI or platform impl)
set(TESTABLE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/region_selector.cpp
    ${CMAKE_SOURCE_DIR}/src/core/window_detector.cpp
    ${CMAKE_SOURCE_DIR}/src/core/control_detector.cpp
    ${CMAKE_SOURCE_DIR}/src/core/screen_capture.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/clipboard_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/file_saver.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/hotkey_manager.cpp
)

# Helper function to create a QtTest-based test target
function(add_unit_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE} ${TESTABLE_SOURCES})
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(${TEST_NAME} PRIVATE
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::Test
    )
    if(MSVC)
        target_compile_options(${TEST_NAME} PRIVATE /utf-8)
        target_compile_definitions(${TEST_NAME} PRIVATE UNICODE _UNICODE)
    endif()
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# Helper function to create a GTest-based test target
function(add_gtest_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE} ${TESTABLE_SOURCES})
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(${TEST_NAME} PRIVATE
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        GTest::gtest
        GTest::gtest_main
    )
    if(MSVC)
        target_compile_options(${TEST_NAME} PRIVATE /utf-8)
        target_compile_definitions(${TEST_NAME} PRIVATE UNICODE _UNICODE)
    endif()
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# Individual test targets (QtTest framework)
add_unit_test(test_region_selector  test_region_selector.cpp)
add_unit_test(test_window_detector  test_window_detector.cpp)
add_unit_test(test_control_detector test_control_detector.cpp)
add_unit_test(test_screen_capture   test_screen_capture.cpp)
add_unit_test(test_clipboard_manager test_clipboard_manager.cpp)
add_unit_test(test_hotkey_manager   test_hotkey_manager.cpp)

# GTest-based tests (only if GTest found)
if(GTest_FOUND)
    add_gtest_test(test_file_saver_gtest utils/test_file_saver.cpp)
endif()

set(CORE_SOURCES
    core/screen_capture.cpp
    core/window_detector.cpp
    core/control_detector.cpp
    core/region_selector.cpp
)

set(UI_SOURCES
    ui/main_window.cpp
    ui/capture_overlay.cpp
    ui/toolbar_widget.cpp
    ui/preview_widget.cpp
    ui/annotation_item.cpp
    ui/save_menu_widget.cpp
    ui/style_panel_widget.cpp
)

set(UTILS_SOURCES
    utils/clipboard_manager.cpp
    utils/file_saver.cpp
    utils/hotkey_manager.cpp
)

set(PLATFORM_SOURCES
    platform/platform_api.cpp
)
if(WIN32)
    list(APPEND PLATFORM_SOURCES
        platform/win/win_platform_api.cpp
    )
elseif(UNIX AND NOT APPLE)
    list(APPEND PLATFORM_SOURCES
        platform/linux/linux_platform_api.cpp
    )
endif()

set(RESOURCE_FILES
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc
)
if(WIN32)
    set(_RC_FILE ${CMAKE_SOURCE_DIR}/resources/app_icon.rc)
    list(APPEND RESOURCE_FILES ${_RC_FILE})
    # Ensure windres (MinGW) can find app_icon.ico relative to the .rc file
    set_source_files_properties(${_RC_FILE} PROPERTIES
        INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/resources
    )
endif()

add_executable(SimpleShotter WIN32
    main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${UTILS_SOURCES}
    ${PLATFORM_SOURCES}
    ${RESOURCE_FILES}
)

target_include_directories(SimpleShotter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(SimpleShotter PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Svg
    Qt5::Concurrent
)

if(WIN32)
    target_link_libraries(SimpleShotter PRIVATE dwmapi user32 gdi32 ole32 oleaut32)
elseif(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(XCB REQUIRED xcb xcb-ewmh xcb-icccm)
    pkg_check_modules(X11 REQUIRED x11)
    pkg_check_modules(ATSPI REQUIRED atspi-2 gobject-2.0)

    target_include_directories(SimpleShotter PRIVATE
        ${XCB_INCLUDE_DIRS}
        ${X11_INCLUDE_DIRS}
        ${ATSPI_INCLUDE_DIRS}
    )
    target_link_libraries(SimpleShotter PRIVATE
        Qt5::X11Extras
        ${XCB_LIBRARIES}
        ${X11_LIBRARIES}
        ${ATSPI_LIBRARIES}
    )
endif()

if(UNIX AND NOT APPLE)
    set_target_properties(SimpleShotter PROPERTIES OUTPUT_NAME simpleshotter)
endif()

if(WIN32)
    add_custom_command(TARGET SimpleShotter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/resources/app_icon.ico
            $<TARGET_FILE_DIR:SimpleShotter>/app_icon.ico
    )
endif()

install(TARGETS SimpleShotter RUNTIME DESTINATION bin)
if(UNIX AND NOT APPLE)
    install(FILES ${CMAKE_SOURCE_DIR}/resources/app-icon.svg
            DESTINATION share/icons/hicolor/scalable/apps
            RENAME simpleshotter.svg)
    foreach(_size 16 24 32 48 128 256 512)
        install(FILES ${CMAKE_SOURCE_DIR}/resources/simpleshotter_${_size}.png
                DESTINATION share/icons/hicolor/${_size}x${_size}/apps
                RENAME simpleshotter.png)
    endforeach()
    install(FILES ${CMAKE_SOURCE_DIR}/debian/simpleshotter.desktop
            DESTINATION share/applications)
endif()

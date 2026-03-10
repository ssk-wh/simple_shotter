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

set(RESOURCE_FILES)
if(WIN32)
    list(APPEND RESOURCE_FILES ${CMAKE_SOURCE_DIR}/resources/app_icon.rc)
endif()

add_executable(EasyShotter WIN32
    main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${UTILS_SOURCES}
    ${PLATFORM_SOURCES}
    ${RESOURCE_FILES}
)

target_include_directories(EasyShotter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(EasyShotter PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
)

if(WIN32)
    target_link_libraries(EasyShotter PRIVATE dwmapi user32 gdi32 ole32 oleaut32)
endif()

# Cài đặt file Addon configuration
install(FILES vicplex.conf
        DESTINATION ${FCITX_INSTALL_PKGDATADIR}/addon)

# Cài đặt file Input Method configuration
install(FILES vi-vicplex.conf
        DESTINATION ${FCITX_INSTALL_PKGDATADIR}/inputmethod)

# Install the icon used by Fcitx5's input-method status item.
install(FILES ${CMAKE_SOURCE_DIR}/assets/icon.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME fcitx-vicplex.svg)

# Status icons are also installed in the standard status context. The engine
# resolves these to absolute paths to avoid desktop-theme lookup differences.
install(FILES ${CMAKE_SOURCE_DIR}/assets/icon.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/status
        RENAME fcitx-vicplex.svg)

find_program(RSVG_CONVERT rsvg-convert REQUIRED)
set(VICPLEX_PNG_OUTPUTS)
foreach(SIZE 22 24)
    set(OUTPUT
            ${CMAKE_CURRENT_BINARY_DIR}/icons/hicolor/${SIZE}x${SIZE}/status/fcitx-vicplex.png)
    add_custom_command(
            OUTPUT ${OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    ${CMAKE_CURRENT_BINARY_DIR}/icons/hicolor/${SIZE}x${SIZE}/status
            COMMAND ${RSVG_CONVERT} -w ${SIZE} -h ${SIZE}
                    ${CMAKE_SOURCE_DIR}/assets/icon.svg -o ${OUTPUT}
            DEPENDS ${CMAKE_SOURCE_DIR}/assets/icon.svg
            COMMENT "Generating ${SIZE}x${SIZE} Vicplex status icon")
    install(FILES ${OUTPUT}
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${SIZE}x${SIZE}/status)
    list(APPEND VICPLEX_PNG_OUTPUTS ${OUTPUT})
endforeach()
add_custom_target(vicplex_icons ALL DEPENDS ${VICPLEX_PNG_OUTPUTS})

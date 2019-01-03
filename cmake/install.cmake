# install systemd service configuration for appimagelauncherfs
configure_file(
    ${PROJECT_SOURCE_DIR}/resources/appimagelauncherfs.service.in
    ${PROJECT_BINARY_DIR}/resources/appimagelauncherfs.service
    @ONLY
)
install(
    FILES ${PROJECT_BINARY_DIR}/resources/appimagelauncherfs.service
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/user/ COMPONENT APPIMAGELAUNCHERFS
)

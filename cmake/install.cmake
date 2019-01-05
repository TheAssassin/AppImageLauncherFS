# install systemd service configuration for appimagelauncherfs
configure_file(
    ${PROJECT_SOURCE_DIR}/resources/appimagelauncherfs.service.in
    ${PROJECT_BINARY_DIR}/resources/appimagelauncherfs.service
    @ONLY
)
# caution: don't use ${CMAKE_INSTALL_LIBDIR} here, it's really just lib/systemd/user
install(
    FILES ${PROJECT_BINARY_DIR}/resources/appimagelauncherfs.service
    DESTINATION lib/systemd/user/ COMPONENT APPIMAGELAUNCHERFS
)

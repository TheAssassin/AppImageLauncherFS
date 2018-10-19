#! /bin/bash

set -x
set -e

# use RAM disk if possible
if [ -d /dev/shm ]; then
    TEMP_BASE=/dev/shm
else
    TEMP_BASE=/tmp
fi

BUILD_DIR=$(mktemp -d -p "$TEMP_BASE" AppImageLauncherFS-build-XXXXXX)

cleanup () {
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
}

trap cleanup EXIT

# store repo root as variable
REPO_ROOT=$(readlink -f $(dirname $(dirname $0)))
OLD_CWD=$(readlink -f .)

pushd "$BUILD_DIR"

cmake "$REPO_ROOT" -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo

# create AppDir
mkdir -p AppDir

# now, compile and install to AppDir
make -j$(nproc) install DESTDIR=AppDir

# determine Git commit ID
# linuxdeployqt uses this for naming the file
export VERSION=$(cd "$REPO_ROOT" && git rev-parse --short HEAD)

# prepend Travis build number if possible
if [ "$TRAVIS_BUILD_NUMBER" != "" ]; then
    export VERSION="$TRAVIS_BUILD_NUMBER-$VERSION"
fi

# get linuxdeploy
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage

cat > AppImageLauncherFS.desktop <<\EOF
[Desktop Entry]
Name=AppImageLauncherFS
Exec=appimagelauncherfs
Icon=appimagelauncherfs
Type=Application
Terminal=true
NoDisplay=true
X-AppImage-Integrate=false
EOF

touch appimagelauncherfs.svg

# bundle applications
./linuxdeploy-x86_64.AppImage --appdir AppDir --output appimage -d AppImageLauncherFS.desktop -i appimagelauncherfs.svg

mv AppImageLauncher*.AppImage "$OLD_CWD"

popd

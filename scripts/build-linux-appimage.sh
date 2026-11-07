#!/usr/bin/env bash
# Builds XylarJava-Linux-x86_64.AppImage from the current build, with everything it needs inside:
# Qt and its plugins, every library the launcher links, and the C library with its loader, so it
# starts on distributions older than the one it was built on.
#
# The graphics driver libraries (libGL, EGL, Vulkan, DRM) are left out on purpose: they have to
# match the computer's own driver. Needs appimagetool (https://github.com/AppImage/appimagetool).
set -euo pipefail

PROJECT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Fixed paths inside the project: AppImages export APPDIR, so generic
# names taken from the environment could point the clean-up below at someone else's folder.
BUILD="$PROJECT/build"
APPDIR="$PROJECT/install-appimage"
OUT="$PROJECT/XylarJava-Linux-x86_64.AppImage"
JOBS="${XYLARJAVA_BUILD_JOBS:-6}"  # more than this runs out of memory on 16 GB while linking
QT_PLUGINS="$(qtpaths6 --plugin-dir 2>/dev/null || qtpaths --plugin-dir 2>/dev/null || qmake6 -query QT_INSTALL_PLUGINS)"
LOADER="$(readlink -f /lib64/ld-linux-x86-64.so.2)"

command -v appimagetool >/dev/null || { echo "appimagetool is not installed" >&2; exit 1; }

cmake --build "$BUILD" --config Release -j"$JOBS"
case "$APPDIR" in "$PROJECT"/install-appimage) rm -rf "$APPDIR" ;; *) echo "refusing to delete $APPDIR" >&2; exit 1 ;; esac
cmake --install "$BUILD" --config Release --prefix "$APPDIR"

# The XylarJava Client jar: "Set up XylarJava Client" copies it from next to the launcher.
CLIENT_JAR="$PROJECT/client/build/libs/xylarjava-client-0.1.0.jar"
if [ -f "$CLIENT_JAR" ]; then
    cp "$CLIENT_JAR" "$APPDIR/bin/"
else
    echo "warning: $CLIENT_JAR is missing (build it in client/ first); the AppImage won't include the client" >&2
fi

# Qt plugins the launcher uses: windows on X11 and Wayland, file dialogs through the desktop
# portal, input methods, images and icons, TLS, and network status.
mkdir -p "$APPDIR/plugins" "$APPDIR/lib"
for dir in platforms platforminputcontexts imageformats iconengines tls networkinformation xcbglintegrations \
           wayland-shell-integration wayland-decoration-client wayland-graphics-integration-client; do
    [ -d "$QT_PLUGINS/$dir" ] && cp -a "$QT_PLUGINS/$dir" "$APPDIR/plugins/"
done
rm -f "$APPDIR"/plugins/platforms/{libqlinuxfb,libqvnc,libqvkkhrdisplay}.so
mkdir -p "$APPDIR/plugins/platformthemes"
if [ -f "$QT_PLUGINS/platformthemes/libqxdgdesktopportal.so" ]; then
    cp "$QT_PLUGINS/platformthemes/libqxdgdesktopportal.so" "$APPDIR/plugins/platformthemes/"
fi

# Tells Qt (which looks next to the program) to use the plugins above and not the system's.
printf '[Paths]\nPrefix=..\nPlugins=plugins\n' > "$APPDIR/bin/qt.conf"

# Libraries that must come from the computer the AppImage runs on.
is_excluded() {
    case "$1" in
        linux-vdso.so.*|ld-linux*|libGL.so.*|libGLX.so.*|libOpenGL.so.*|libEGL.so.*|libGLESv2.so.*|libGLdispatch.so.*|\
        libdrm.so.*|libgbm.so.*|libvulkan.so.*|libwayland-egl.so.*) return 0 ;;
    esac
    return 1
}

# Copies what $1 links, then what those link, until nothing new turns up.
bundle_deps() {
    ldd "$1" 2>/dev/null | awk '/=> \// { print $1, $3 }' | while read -r name path; do
        is_excluded "$name" && continue
        [ -e "$APPDIR/lib/$name" ] && continue
        cp -L "$path" "$APPDIR/lib/$name"
        bundle_deps "$path"
    done
}

bundle_deps "$APPDIR/bin/XylarJava"
find "$APPDIR/plugins" -name '*.so' | while read -r plugin; do bundle_deps "$plugin"; done
# Loaded at run time rather than linked: OpenSSL for the TLS plugin.
for lib in libssl.so.3 libcrypto.so.3; do
    path="$(ldconfig -p | awk -v l="$lib" '$1 == l && /x86-64/ && !found { print $NF; found = 1 }')"
    [ -n "$path" ] && [ ! -e "$APPDIR/lib/$lib" ] && cp -L "$path" "$APPDIR/lib/$lib" && bundle_deps "$path"
done

# The loader sits next to the launcher so the launcher still finds its own folder (themes, jars).
cp -L "$LOADER" "$APPDIR/bin/ld-linux-x86-64.so.2"
mkdir -p "$APPDIR/share/xylarjava"
cp "$PROJECT/scripts/integrate-linux.sh" "$APPDIR/share/xylarjava/"

cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
if [ -n "${APPIMAGE:-}" ]; then
    sh "$HERE/share/xylarjava/integrate-linux.sh" "$APPIMAGE" "$HERE" || \
        echo 'XylarJava: could not register the application menu entry' >&2
else
    sh "$HERE/share/xylarjava/integrate-linux.sh" "$HERE/AppRun" "$HERE" native || \
        echo 'XylarJava: could not register the application menu entry' >&2
fi
# Same as the official AppImage: OpenGL/Vulkan probing off, file pickers through the desktop portal.
export XYLARJAVA_DISABLE_GLVULKAN="${XYLARJAVA_DISABLE_GLVULKAN:-1}"
export QT_QPA_PLATFORMTHEME="${QT_QPA_PLATFORMTHEME:-xdgdesktopportal}"
# --library-path only applies to the launcher: Java and the game use the computer's own libraries.
exec "$HERE/bin/ld-linux-x86-64.so.2" --library-path "$HERE/lib" "$HERE/bin/XylarJava" "$@"
EOF
chmod +x "$APPDIR/AppRun"

ln -sf share/applications/io.xylarjava.XylarJava.desktop "$APPDIR/io.xylarjava.XylarJava.desktop"
ln -sf share/icons/hicolor/256x256/apps/io.xylarjava.XylarJava.png "$APPDIR/io.xylarjava.XylarJava.png"
ln -sf share/icons/hicolor/256x256/apps/io.xylarjava.XylarJava.png "$APPDIR/.DirIcon"
ln -sf io.xylarjava.XylarJava.metainfo.xml "$APPDIR/share/metainfo/io.xylarjava.XylarJava.appdata.xml"

runtime_options=()
if [ -n "${XYLARJAVA_APPIMAGE_RUNTIME:-}" ]; then
    runtime_options=(--runtime-file "$XYLARJAVA_APPIMAGE_RUNTIME")
fi
ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 appimagetool --no-appstream "${runtime_options[@]}" "$APPDIR" "$OUT"
chmod +x "$OUT"
mkdir -p "$PROJECT/dist"
tar -czf "$PROJECT/dist/XylarJava-Linux-x86_64.tar.gz" -C "$APPDIR" .
echo "AppImage: $OUT"

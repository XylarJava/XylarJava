#!/bin/sh
# Register an AppImage in the current user's XDG applications menu on first launch.
set -eu
image=${1:-}
appdir=${2:-}
mode=${3:-appimage}
[ -n "$image" ] && [ -f "$image" ] && [ -d "$appdir" ] || exit 0
[ "$(id -u)" != 0 ] || exit 0

data=${XDG_DATA_HOME:-"$HOME/.local/share"}
case "$data" in /*) ;; *) echo 'XDG_DATA_HOME must be absolute' >&2; exit 1 ;; esac
install_dir="$data/XylarJava/application"
target="$install_dir/XylarJava.AppImage"
applications="$data/applications"
icons="$data/icons/hicolor/256x256/apps"
mkdir -p "$install_dir" "$applications" "$icons"

# Keep a stable copy so moving or deleting the download does not break the menu entry.
if [ "$mode" = native ]; then
    version=$(sha256sum "$appdir/bin/XylarJava" | cut -d ' ' -f 1)
    install_dir="$install_dir/native-$version"
    if [ "$(readlink -f "$appdir")" != "$(readlink -f "$install_dir")" ]; then
        if [ ! -d "$install_dir" ]; then
            tmp=$(mktemp -d "$data/XylarJava/application/.native.XXXXXX")
            trap 'rm -rf "$tmp"' EXIT HUP INT TERM
            cp -a "$appdir/." "$tmp/"
            mv "$tmp" "$install_dir"
            trap - EXIT HUP INT TERM
        fi
    fi
    target="$install_dir/AppRun"
elif [ "$(readlink -f "$image")" != "$(readlink -f "$target")" ]; then
    tmp=$(mktemp "$install_dir/.download.XXXXXX")
    trap 'rm -f "$tmp"' EXIT HUP INT TERM
    cp "$image" "$tmp"
    chmod 755 "$tmp"
    mv -f "$tmp" "$target"
    trap - EXIT HUP INT TERM
fi
cp "$appdir/share/icons/hicolor/256x256/apps/io.xylarjava.XylarJava.png" "$icons/io.xylarjava.XylarJava.png"

# Desktop Entry quoting has two escape layers (string values, then Exec arguments).
exec_path=$(printf '%s' "$target" | sed 's/\\/\\\\\\\\/g; s/"/\\\\"/g; s/`/\\\\`/g; s/\$/\\\\$/g; s/%/%%/g')
desktop="$applications/io.xylarjava.XylarJava.desktop"
# A fixed interpreter also lets desktop launchers resolve paths containing literal percent signs.
exec_command='/bin/sh -c "exec \\"\\$0\\" --appimage-extract-and-run \\"\\$@\\""'
if [ "$mode" = native ]; then
    exec_command='/bin/sh -c "exec \\"\\$0\\" \\"\\$@\\""'
fi
tmp=$(mktemp "$applications/.xylarjava.XXXXXX")
trap 'rm -f "$tmp"' EXIT HUP INT TERM
cat > "$tmp" <<DESKTOP
[Desktop Entry]
Version=1.0
Type=Application
Name=XylarJava
GenericName=Minecraft Launcher
Comment=Manage and launch Minecraft instances with XylarJava
Exec=$exec_command "$exec_path" %U
Icon=io.xylarjava.XylarJava
Terminal=false
StartupNotify=true
StartupWMClass=XylarJava
Categories=Game;
Keywords=Minecraft;launcher;
MimeType=application/x-modrinth-modpack+zip;x-scheme-handler/curseforge;x-scheme-handler/xylarjava;
DESKTOP
chmod 644 "$tmp"
mv -f "$tmp" "$desktop"
trap - EXIT HUP INT TERM
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$applications" >/dev/null 2>&1 || true
fi

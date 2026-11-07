#!/bin/bash
# Regenerates XylarJava.ico, XylarJava.icns and the in-app logo from io.xylarjava.XylarJava.svg.

LAUNCHER_APPID="io.xylarjava.XylarJava"

svg2png() {
    input_file="$1"
    output_file="$2"
    width="$3"
    height="$4"

    inkscape -w "$width" -h "$height" -o "$output_file" "$input_file"
}

if command -v "inkscape" && command -v "icotool" && command -v "oxipng"; then
    # Windows ICO
    d=$(mktemp -d)

    for size in 16 24 32 48 64 128 256; do
        svg2png ${LAUNCHER_APPID}.svg "$d/xylarjava_${size}.png" $size $size
    done

    oxipng --opt max --strip all --alpha --interlace 0 "$d/xylarjava_"*".png"

    rm -f XylarJava.ico && icotool -o XylarJava.ico -c \
        "$d/xylarjava_256.png"  \
        "$d/xylarjava_128.png"  \
        "$d/xylarjava_64.png"   \
        "$d/xylarjava_48.png"   \
        "$d/xylarjava_32.png"   \
        "$d/xylarjava_24.png"   \
        "$d/xylarjava_16.png"
else
    echo "ERROR: Windows icons were NOT generated!" >&2
    echo "ERROR: requires inkscape, icotool and oxipng in PATH"
fi

if command -v "inkscape" && command -v "iconutil" && command -v "oxipng"; then
    # macOS ICNS
    d=$(mktemp -d)

    d="$d/XylarJava.iconset"

    mkdir -p "$d"

    for size in 16 32 128 256 512; do
        svg2png ${LAUNCHER_APPID}.svg "$d/icon_${size}x${size}.png" $size $size
        svg2png ${LAUNCHER_APPID}.svg "$d/icon_${size}x${size}@2x.png" $((size * 2)) $((size * 2))
    done

    oxipng --opt max --strip all --alpha --interlace 0 "$d/icon_"*".png"

    iconutil -c icns "$d"
    cp -v "${d%.iconset}.icns" XylarJava.icns
else
    echo "ERROR: macOS icons were NOT generated!" >&2
    echo "ERROR: requires inkscape, iconutil and oxipng in PATH"
fi

# replace icon in themes
cp -v "${LAUNCHER_APPID}.svg" "../launcher/resources/polymc/scalable/launcher.svg"

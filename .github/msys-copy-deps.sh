#!/bin/bash -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"

cp $SCRIPT_DIR/msys-deps-list.txt $SCRIPT_DIR/msys-deps-list.tmp

# Python runtime
pacman -Ql mingw-w64-x86_64-python | awk '{print $2}' | awk 'gsub(/^\/mingw64/, "$MSYSTEM_PREFIX") && /\/lib\/.*(\.pyd?|LICENSE\.txt)$/ && !/curses|distutils|ensurepip|idlelib|lib2to3|pydoc_data|test|tkinter|Tools|turtledemo/ {print}' >> $SCRIPT_DIR/msys-deps-list.tmp
pacman -Ql mingw-w64-x86_64-python-gobject | awk '{print $2}' | awk 'gsub(/^\/mingw64/, "$MSYSTEM_PREFIX") && /\/lib\/.*\.pyd?$/ && !/pygtkcompat/ {print}' >> $SCRIPT_DIR/msys-deps-list.tmp

while IFS="" read -u 10 -r x || [ -n "$x" ]
do
    if [ ! -d $(dirname "${x#\$MSYSTEM_PREFIX/}") ]; then
        mkdir -p $(dirname "${x#\$MSYSTEM_PREFIX/}")
    fi
    cp "${x/'$MSYSTEM_PREFIX'/$MSYSTEM_PREFIX}" "${x#\$MSYSTEM_PREFIX/}"
done 10<$SCRIPT_DIR/msys-deps-list.tmp

rm $SCRIPT_DIR/msys-deps-list.tmp

mkdir -p lib/gdk-pixbuf-2.0/2.10.0/loaders
cp -v $MSYSTEM_PREFIX/lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-*.dll lib/gdk-pixbuf-2.0/2.10.0/loaders

cp -v $MSYSTEM_PREFIX/bin/libunrar.dll .

cp -v $MSYSTEM_PREFIX/bin/gdbus.exe .
cp -v $MSYSTEM_PREFIX/bin/gspawn-win64-helper.exe .
cp -v $MSYSTEM_PREFIX/bin/gspawn-win64-helper-console.exe .

cp /usr/ssl/certs/ca-bundle.crt .

ldd lib/gstreamer-1.0/*.dll | grep "=> $MSYSTEM_PREFIX" | awk '{print $3}' | xargs -I '{}' cp -vn '{}' .
ldd lib/gdk-pixbuf-2.0/2.10.0/loaders/*.dll | grep "=> $MSYSTEM_PREFIX" | awk '{print $3}' | xargs -I '{}' cp -vn '{}' .
ldd lib/libpeas-1.0/loaders/*.dll | grep "=> $MSYSTEM_PREFIX" | awk '{print $3}' | xargs -I '{}' cp -vn '{}' .

ldd ahoviewer.exe | grep "=> $MSYSTEM_PREFIX" | awk '{print $3}' | xargs -I '{}' cp -vn '{}' .

# Remove all debug symbols
echo "Removing debug symbols"
find . -name '*.exe' -or -name '*.dll' | xargs -I '{}' strip '{}'

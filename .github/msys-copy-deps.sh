#!/bin/bash -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"

while IFS="" read -u 10 -r x || [ -n "$x" ]
do
    if [ ! -d $(dirname "${x#\$MSYSTEM_PREFIX/}") ]; then
        mkdir -p $(dirname "${x#\$MSYSTEM_PREFIX/}")
    fi
    cp "${x/'$MSYSTEM_PREFIX'/$MSYSTEM_PREFIX}" "${x#\$MSYSTEM_PREFIX/}"
done 10<$SCRIPT_DIR/msys-deps-list.txt

cp -v $MSYSTEM_PREFIX/bin/libssl-1_1-x64.dll .
cp -v $MSYSTEM_PREFIX/bin/libcrypto-1_1-x64.dll .

cp $MSYSTEM_PREFIX/bin/gdbus.exe .
cp $MSYSTEM_PREFIX/bin/gspawn-win64-helper.exe .
cp $MSYSTEM_PREFIX/bin/gspawn-win64-helper-console.exe .

cp $MSYSTEM_PREFIX/ssl/certs/ca-bundle.crt .

ldd lib/gstreamer-1.0/*.dll | grep "=> $MSYSTEM_PREFIX" | awk '{print $3}' | xargs -I '{}' cp -n '{}' .
ldd lib/gdk-pixbuf-2.0/2.10.0/loaders/*.dll | grep "=> $MSYSTEM_PREFIX" | awk '{print $3}' | xargs -I '{}' cp -n '{}' .
ldd lib/libpeas-1.0/loaders/*.dll | grep "=> $MSYSTEM_PREFIX" | awk '{print $3}' | xargs -I '{}' cp -n '{}' .

ldd ahoviewer.exe | grep "=> $MSYSTEM_PREFIX" | awk '{print $3}' | xargs -I '{}' cp -n '{}' .

# Remove all debug symbols
find . -name '*.exe' -or -name '*.dll' | xargs -I '{}' strip '{}'

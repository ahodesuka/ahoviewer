#!/bin/bash -e

git clone https://github.com/ahodesuka/ahoviewer-windows-assets.git "${GITHUB_WORKSPACE}/ahoviewer"
cd "${GITHUB_WORKSPACE}/ahoviewer"
rm -rf .git
cp ../build/src/ahoviewer.exe .
cp -r ../data/icons share
sh "${GITHUB_WORKSPACE}/.github/msys-copy-deps.sh"
cp ../build/src/Ahoviewer-*.typelib lib/girepository-1.0
zip -qr9 ../ahoviewer.zip .
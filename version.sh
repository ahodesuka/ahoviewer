#!/bin/sh

if [ -n "$1" ]
then
    VERSION="$1"
elif [ -d '.git' ]
then
    git update-index --refresh > /dev/null 2>&1
    VERSION=$(git describe --match='[0-9]*' --dirty 2> /dev/null)
fi

[ -n "$VERSION" ] && echo -n "$VERSION" > VERSION

if [ -f "VERSION" ]
then
    VERSION=$(cat VERSION)
else
    VERSION="UNKNOWN"
fi

cat <<EOF > src/version.h
#ifndef _VERSION_
#define _VERSION_ "$VERSION"
#endif
EOF

echo $VERSION

#!/bin/sh

ver=($(git describe --tags --long | \
       sed "s/\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\).*/\1\n\2\n\3/"))

case $1 in
    ma*) # major
        VERSION="$((ver[0] + 1)).${ver[1]}.${ver[2]}"
        ;;
    mi*) # minor
        VERSION="${ver[0]}.$((ver[1] + 1)).${ver[2]}"
        ;;
    *) # patch
        VERSION="${ver[0]}.${ver[1]}.$((ver[2] + 1))"
        ;;
esac

git update-index --no-assume-unchanged VERSION
./version.sh $VERSION > /dev/null 2>&1
git commit -am "Release $VERSION" && git tag -a "$VERSION" -m "$VERSION"
git update-index --assume-unchanged VERSION

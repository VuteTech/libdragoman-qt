#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
# SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Assembles the OBS package directory for one release: the source tarball
# plus one build recipe per package format, with the version stamped in.
# OBS picks the recipe matching each repository:
#
#   libdragoman-qt.spec                    openSUSE, Fedora
#   libdragoman-qt.dsc + debian.*          Debian, Ubuntu (via debtransform)
#   PKGBUILD                               Arch Linux
#
# The runtime library's package name carries the SO version, which
# CMakeLists.txt sets to the minor version.
#
# Usage: packaging/obs/prepare.sh <version> <tarball> <out-dir>
# The tarball comes from scripts/make-release-tarball.sh.

set -euo pipefail

usage="usage: prepare.sh <version> <tarball> <out-dir>"
version="${1:?$usage}"
tarball="${2:?$usage}"
out="${3:?$usage}"
here="$(cd -- "$(dirname -- "$0")" && pwd)"

[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
    echo "version must look like 1.2.3, got: $version" >&2
    exit 2
}
[ "$(basename "$tarball")" = "libdragoman-qt-$version.tar.gz" ] || {
    echo "tarball must be named libdragoman-qt-$version.tar.gz" >&2
    exit 2
}
tar -tzf "$tarball" "libdragoman-qt-$version/.tarball-version" >/dev/null 2>&1 || {
    echo "$tarball lacks .tarball-version; build it with scripts/make-release-tarball.sh" >&2
    exit 2
}

sover="${version#*.}"
sover="${sover%%.*}"

# Changelog dates: the commit time when reproducing a release, else now.
date="$(LC_ALL=C date -R ${SOURCE_DATE_EPOCH:+-d @$SOURCE_DATE_EPOCH})"
rpm_date="$(LC_ALL=C date -u ${SOURCE_DATE_EPOCH:+-d @$SOURCE_DATE_EPOCH} '+%a %b %d %Y')"

# The .dsc repeats the Build-Depends of debian.control; derive it so the
# two cannot drift apart.
build_depends="$(sed -n '/^Build-Depends:/,/^[A-Z]/p' "$here/debian.control" |
    sed '$d' | sed 's/^Build-Depends://' | tr -d '\n' | sed 's/^ *//; s/  */ /g')"

mkdir -p "$out"
stamp() { # <source> <destination>
    sed -e "s/@VERSION@/$version/g" \
        -e "s/@SOVERSION@/$sover/g" \
        -e "s/@DATE@/$date/g" \
        -e "s/@RPM_DATE@/$rpm_date/g" \
        -e "s/@BUILD_DEPENDS@/$build_depends/g" \
        "$1" >"$2"
}

for f in libdragoman-qt.spec libdragoman-qt.dsc debian.control debian.rules debian.changelog PKGBUILD; do
    stamp "$here/$f" "$out/$f"
done
# debtransform installs debian.<name> as debian/<name>, and these names
# depend on the SO version.
stamp "$here/debian.lib.install" "$out/debian.libdragoman-qt$sover.install"
stamp "$here/debian.dev.install" "$out/debian.libdragoman-qt-dev.install"
chmod +x "$out/debian.rules"
cp "$tarball" "$out/"

if grep -l '@[A-Z_]*@' "$out"/libdragoman-qt.spec "$out"/libdragoman-qt.dsc "$out"/debian.* "$out"/PKGBUILD; then
    echo "unstamped placeholders left in the files above" >&2
    exit 1
fi
ls -l "$out"

#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
# SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Regenerates po/libdragoman-qt.pot through src/Messages.sh, with the
# xgettext keywords of KDE's scripty, and merges it into every
# po/<lang>/libdragoman-qt.po.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
podir="$root/po"
export podir

keywords=(
    -ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3
    -kki18n:1 -kki18nc:1c,2 -kki18np:1,2 -kki18ncp:1c,2,3
    -kI18N_NOOP:1 -kI18NC_NOOP:1c,2
)
XGETTEXT="xgettext --from-code=UTF-8 --c++ --kde --add-comments=i18n --no-location --package-name=libdragoman-qt --msgid-bugs-address=https://github.com/VuteTech/libdragoman-qt/issues ${keywords[*]}"
export XGETTEXT

cd "$root/src"
bash ./Messages.sh

for po in "$podir"/*/libdragoman-qt.po; do
    [ -e "$po" ] || continue
    msgmerge --quiet --update --backup=none --no-location "$po" "$podir/libdragoman-qt.pot"
done

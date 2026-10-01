#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
# SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Extracts the translatable strings into $podir/libdragoman-qt.pot, following
# KDE's Messages.sh convention (XGETTEXT and podir come from the
# environment; scripts/update-translations.sh sets them).
# The file list and XGETTEXT are split into words on purpose.
# shellcheck disable=SC2154,SC2046,SC2086
$XGETTEXT $(find . -name '*.cpp' -o -name '*.h' | sort) -o "$podir/libdragoman-qt.pot"

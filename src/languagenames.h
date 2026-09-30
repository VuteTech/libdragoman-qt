/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include "dragomanqt_export.h"

#include <QString>

namespace Dragoman
{

/**
 * The name of the language with BCP 47 code @p code ("bg", "zh-Hant") in
 * the user's language, taken from the iso-codes translations when they are
 * installed and from QLocale's English names otherwise. A script subtag is
 * named in parentheses; codes QLocale does not know stay as they are.
 */
[[nodiscard]] DRAGOMANQT_EXPORT QString languageName(const QString &code);

/**
 * languageName() for use inside a sentence: languages whose grammar does not
 * capitalise language names (Bulgarian, French, Russian, ...) get it in
 * lower case when they are the user interface language ("от английски",
 * "de l'anglais"); in English and German it is languageName() unchanged.
 */
[[nodiscard]] DRAGOMANQT_EXPORT QString languageNameInSentence(const QString &code);

/// Whether the user interface language @p uiLanguage (a code such as "bg"
/// or "de_DE") writes language names in lower case inside a sentence.
[[nodiscard]] DRAGOMANQT_EXPORT bool lowercasesLanguageNames(QStringView uiLanguage);

} // namespace Dragoman

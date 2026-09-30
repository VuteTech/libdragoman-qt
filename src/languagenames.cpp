/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "languagenames.h"

#include <KLocalizedString>

#include <QLocale>

using namespace Qt::StringLiterals;

namespace Dragoman
{

namespace
{

/// The iso-codes (ISO 639-3) msgid of a language, where it differs from QLocale's name.
QString isoName(QLocale::Language language, const QString &qtName)
{
    switch (language) {
    case QLocale::Greek:
        return u"Modern Greek (1453-)"_s;
    case QLocale::NorwegianBokmal:
        return u"Norwegian Bokmål"_s;
    default:
        return qtName;
    }
}

QString scriptName(QLocale::Script script)
{
    switch (script) {
    case QLocale::SimplifiedHanScript:
        return i18nc("Chinese script variant", "Simplified");
    case QLocale::TraditionalHanScript:
        return i18nc("Chinese script variant", "Traditional");
    default:
        return QLocale::scriptToString(script);
    }
}

} // namespace

QString languageName(const QString &code)
{
    const QLocale locale(code);
    if (locale.language() == QLocale::C || locale.language() == QLocale::AnyLanguage) {
        return code;
    }
    const QString qtName = QLocale::languageToString(locale.language());
    const QString msgid = isoName(locale.language(), qtName);
    QString name = ki18nd("iso_639-3", msgid.toUtf8().constData()).toString();
    if (name == msgid) {
        name = qtName; // no translation installed
    } else if (const auto year = name.indexOf(u" (1453-)"); year > 0) {
        name.truncate(year);
    }
    // Only an explicit script subtag is worth naming ("zh-Hant", not "sr").
    if (code.contains(u'-') || code.contains(u'_')) {
        name = i18nc("language name (script name)", "%1 (%2)", name, scriptName(locale.script()));
    }
    return name;
}

} // namespace Dragoman

/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "languagenames.h"

#include <KLocalizedString>

#include <QLocale>

#include <algorithm>
#include <array>

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

bool lowercasesLanguageNames(QStringView uiLanguage)
{
    // Languages that capitalise language names mid-sentence are the
    // exception (English, German, ...); these are the common others.
    static constexpr std::array lowercasing{u"bg", u"be", u"bs", u"ca", u"cs", u"da", u"el", u"es", u"et", u"eu", u"fi", u"fr",
                                            u"gl", u"hr", u"hu", u"is", u"it", u"lt", u"lv", u"mk", u"nb", u"nl", u"nn", u"no",
                                            u"pl", u"pt", u"ro", u"ru", u"sk", u"sl", u"sq", u"sr", u"sv", u"tr", u"uk"};
    // "bg", "bg_BG", "sr@latin", "pt-BR": the language is the first part.
    const auto end = std::ranges::find_if(uiLanguage, [](QChar c) {
        return c == u'_' || c == u'-' || c == u'@' || c == u'.';
    });
    const QStringView primary = uiLanguage.first(std::distance(uiLanguage.begin(), end));
    return std::ranges::any_of(lowercasing, [primary](const char16_t *code) {
        return primary == QStringView(code);
    });
}

QString languageNameInSentence(const QString &code)
{
    QString name = languageName(code);
    const QStringList uiLanguages = KLocalizedString::languages();
    const QString ui = uiLanguages.isEmpty() ? QLocale().name() : uiLanguages.constFirst();
    if (!name.isEmpty() && lowercasesLanguageNames(ui)) {
        name[0] = name.at(0).toLower();
    }
    return name;
}

} // namespace Dragoman

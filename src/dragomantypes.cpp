/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "dragomantypes.h"

#include <QDBusArgument>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace Dragoman
{

bool Error::isDaemonMissing() const
{
    return name == QLatin1StringView("org.freedesktop.DBus.Error.ServiceUnknown") || name == QLatin1StringView("org.freedesktop.DBus.Error.NameHasNoOwner")
        || name.startsWith(QLatin1StringView("org.freedesktop.DBus.Error.Spawn"));
}

bool Error::isUnsupported() const
{
    return name == QLatin1StringView("org.freedesktop.DBus.Error.UnknownMethod");
}

QStringList Reply::translations() const
{
    return toStringList(results.value(u"translations"_s));
}

QString Reply::pivot() const
{
    return results.value(u"pivot"_s).toString();
}

QList<QList<SentenceSpan>> Reply::sentences() const
{
    QList<QList<SentenceSpan>> segments;
    const QVariant value = results.value(u"sentences"_s);
    if (!value.canConvert<QDBusArgument>()) {
        return segments;
    }
    const auto argument = value.value<QDBusArgument>();
    argument.beginArray();
    while (!argument.atEnd()) {
        QList<SentenceSpan> spans;
        argument.beginArray();
        while (!argument.atEnd()) {
            uint sourceBegin = 0;
            uint sourceEnd = 0;
            uint targetBegin = 0;
            uint targetEnd = 0;
            argument.beginStructure();
            argument >> sourceBegin >> sourceEnd >> targetBegin >> targetEnd;
            argument.endStructure();
            spans.append({int(sourceBegin), int(sourceEnd), int(targetBegin), int(targetEnd)});
        }
        argument.endArray();
        segments.append(spans);
    }
    argument.endArray();
    return segments;
}

QString Reply::document() const
{
    return results.value(u"document"_s).toString();
}

Detection Detection::fromMap(const QVariantMap &map)
{
    Detection detection;
    detection.language = map.value(u"language"_s).toString();
    detection.confidence = std::clamp(map.value(u"confidence"_s).toDouble(), 0.0, 1.0);
    detection.reliable = map.value(u"reliable"_s).toBool();
    return detection;
}

PairInfo PairInfo::fromRecord(const QVariantMap &record)
{
    PairInfo info;
    info.source = record.value(u"source"_s).toString();
    info.target = record.value(u"target"_s).toString();
    info.installedVersion = record.value(u"installed_version"_s).toString();
    info.availableVersion = record.value(u"available_version"_s).toString();
    info.origin = record.value(u"origin"_s).toString();
    info.architecture = record.value(u"architecture"_s).toString();
    bool ok = false;
    const qint64 size = record.value(u"size"_s).toLongLong(&ok);
    info.size = ok ? size : -1;
    info.releaseStatus = record.value(u"release_status"_s).toString();
    const double quality = record.value(u"quality"_s).toDouble(&ok);
    info.quality = ok && quality >= 0 && quality <= 1 ? quality : -1;
    return info;
}

bool PairInfo::hasUpdate() const
{
    return isInstalled() && !availableVersion.isEmpty() && compareVersions(availableVersion, installedVersion) > 0;
}

DaemonStatus DaemonStatus::fromMap(const QVariantMap &map)
{
    const auto number = [&map](const QString &key) -> qint64 {
        bool ok = false;
        const qint64 value = map.value(key).toLongLong(&ok);
        return ok ? value : -1;
    };
    DaemonStatus status;
    status.version = map.value(u"version"_s).toString();
    status.loaded = toStringList(map.value(u"loaded"_s));
    status.queued = number(u"queued"_s);
    status.residentMb = number(u"rss_mb"_s);
    status.modelCostMb = number(u"model_cost_mb"_s);
    return status;
}

namespace
{

/// One dot-separated part: its number and the pre-release suffix after it.
struct VersionPart {
    qint64 number = 0;
    QStringView suffix;
};

VersionPart parsePart(QStringView part)
{
    VersionPart parsed;
    qsizetype digits = 0;
    while (digits < part.size() && part.at(digits).isDigit()) {
        ++digits;
    }
    parsed.number = part.first(digits).toLongLong();
    parsed.suffix = part.sliced(digits);
    return parsed;
}

int compareSuffixes(QStringView a, QStringView b)
{
    // No suffix is a release, which sorts after any pre-release ("a1").
    if (a.isEmpty() || b.isEmpty()) {
        return int(a.isEmpty()) - int(b.isEmpty());
    }
    if (const int letters = a.first(1).compare(b.first(1)); letters != 0) {
        return letters;
    }
    const VersionPart restA = parsePart(a.sliced(1));
    const VersionPart restB = parsePart(b.sliced(1));
    if (restA.number != restB.number) {
        return restA.number < restB.number ? -1 : 1;
    }
    return restA.suffix.compare(restB.suffix);
}

} // namespace

int compareVersions(QStringView a, QStringView b)
{
    const auto partsA = a.split(u'.');
    const auto partsB = b.split(u'.');
    const auto count = std::max(partsA.size(), partsB.size());
    for (qsizetype i = 0; i < count; ++i) {
        const VersionPart partA = i < partsA.size() ? parsePart(partsA.at(i)) : VersionPart{};
        const VersionPart partB = i < partsB.size() ? parsePart(partsB.at(i)) : VersionPart{};
        if (partA.number != partB.number) {
            return partA.number < partB.number ? -1 : 1;
        }
        if (const int suffix = compareSuffixes(partA.suffix, partB.suffix); suffix != 0) {
            return suffix < 0 ? -1 : 1;
        }
    }
    return 0;
}

QStringList toStringList(const QVariant &value)
{
    if (value.canConvert<QDBusArgument>()) {
        return qdbus_cast<QStringList>(value.value<QDBusArgument>());
    }
    return value.toStringList();
}

int toUtf16(QStringView text, int codePoint)
{
    int index = 0;
    for (int seen = 0; seen < codePoint && index < text.size(); ++seen) {
        index += text.at(index).isHighSurrogate() && index + 1 < text.size() ? 2 : 1;
    }
    return index;
}

QString requestPath(QStringView uniqueName, QStringView token)
{
    QString escaped = uniqueName.toString();
    std::ranges::replace_if(
        escaped,
        [](QChar c) {
            const char16_t u = c.unicode();
            return !((u >= u'0' && u <= u'9') || (u >= u'a' && u <= u'z') || (u >= u'A' && u <= u'Z'));
        },
        u'_');
    return u"/dev/l10n_bg/dragomand/request/"_s + escaped + u'/' + token;
}

} // namespace Dragoman

/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include "dragomanqt_export.h"

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace Dragoman
{

/// Error names the daemon returns from its methods.
namespace Errors
{
inline constexpr QLatin1StringView InvalidArgument{"dev.l10n_bg.dragomand.Error.InvalidArgument"};
inline constexpr QLatin1StringView UnsupportedPair{"dev.l10n_bg.dragomand.Error.UnsupportedPair"};
inline constexpr QLatin1StringView NotInstalled{"dev.l10n_bg.dragomand.Error.NotInstalled"};
inline constexpr QLatin1StringView LimitExceeded{"dev.l10n_bg.dragomand.Error.LimitExceeded"};
inline constexpr QLatin1StringView NetworkDisabled{"dev.l10n_bg.dragomand.Error.NetworkDisabled"};
inline constexpr QLatin1StringView EngineFailure{"dev.l10n_bg.dragomand.Error.EngineFailure"};
}

/**
 * One sentence of a segment and its rendering in the translation, as
 * [begin, end) offsets in Unicode code points (not QString's UTF-16 units;
 * see toUtf16()).
 */
struct DRAGOMANQT_EXPORT SentenceSpan {
    int sourceBegin = 0;
    int sourceEnd = 0;
    int targetBegin = 0;
    int targetEnd = 0;

    friend bool operator==(const SentenceSpan &, const SentenceSpan &) = default;
};

/// The outcome of one request (or of a chain of them, see Client::translate()).
struct DRAGOMANQT_EXPORT Reply {
    enum class Status {
        Success,
        Cancelled,
        Failed,
    };

    Status status = Status::Failed;
    /// The Response results: "translations", "pivot", "version", "updates", ...
    QVariantMap results;
    /// The D-Bus error name when the method call itself failed, else empty.
    QString errorName;
    /// A user-presentable failure message.
    QString error;
    /// Whether a missing pair was installed and loaded before the result.
    bool prepared = false;

    [[nodiscard]] bool ok() const
    {
        return status == Status::Success;
    }
    [[nodiscard]] bool cancelled() const
    {
        return status == Status::Cancelled;
    }
    /// results["translations"], one per input segment.
    [[nodiscard]] QStringList translations() const;
    /// results["pivot"]: the pivot language when the daemon pivoted.
    [[nodiscard]] QString pivot() const;
    /// results["sentences"], per segment, when Translate asked for them.
    [[nodiscard]] QList<QList<SentenceSpan>> sentences() const;
    /// For Client::translateDocument(): the translated document.
    [[nodiscard]] QString document() const;
};

/// One record of ListLanguagePairs. Unknown values are empty, or -1.
struct DRAGOMANQT_EXPORT PairInfo {
    QString source;
    QString target;
    QString installedVersion;
    QString availableVersion;
    QString origin; ///< "system" or "user"
    QString architecture;
    qint64 size = -1; ///< installed bytes
    /// Mozilla's label for the model ("Release", "Nightly", ...), when known.
    QString releaseStatus;
    /// The model's COMET-22 score (0 to 1, higher is better), or -1.
    double quality = -1;

    [[nodiscard]] static PairInfo fromRecord(const QVariantMap &record);
    [[nodiscard]] bool isValid() const
    {
        return !source.isEmpty() && !target.isEmpty();
    }
    [[nodiscard]] bool isInstalled() const
    {
        return !installedVersion.isEmpty();
    }
    /// Whether the provider has a newer version than the installed one.
    [[nodiscard]] bool hasUpdate() const;
    /// Whether RemovePair would delete something (system copies stay).
    [[nodiscard]] bool isRemovable() const
    {
        return isInstalled() && origin == QLatin1StringView("user");
    }

    friend bool operator==(const PairInfo &, const PairInfo &) = default;
};

/// The result of DetectLanguage.
struct DRAGOMANQT_EXPORT Detection {
    /// The language code, empty when nothing was detected.
    QString language;
    double confidence = 0; ///< 0 to 1
    bool reliable = false;

    [[nodiscard]] static Detection fromMap(const QVariantMap &map);
};

/// The result of GetStatus. Unknown values are -1.
struct DRAGOMANQT_EXPORT DaemonStatus {
    QString version;
    QStringList loaded; ///< loaded routes, "src-trg"
    qint64 queued = -1;
    qint64 residentMb = -1;
    qint64 modelCostMb = -1;

    [[nodiscard]] static DaemonStatus fromMap(const QVariantMap &map);
};

/**
 * Orders model versions the way Mozilla's toolkit does for the versions the
 * provider publishes: numeric parts first, then a release sorts after its
 * pre-releases ("3.0a1" < "3.0a2" < "3.0"). Returns <0, 0 or >0.
 */
[[nodiscard]] DRAGOMANQT_EXPORT int compareVersions(QStringView a, QStringView b);

/// A string list from an a{sv} value: nested arrays arrive as QDBusArgument.
[[nodiscard]] DRAGOMANQT_EXPORT QStringList toStringList(const QVariant &value);

/// The UTF-16 index (QString position) of code point @p codePoint in @p text.
[[nodiscard]] DRAGOMANQT_EXPORT int toUtf16(QStringView text, int codePoint);

/**
 * The request object path for @p token on the connection named
 * @p uniqueName: every character of the unique name that is not an ASCII
 * letter or digit becomes '_' (so the leading ':' does too).
 */
[[nodiscard]] DRAGOMANQT_EXPORT QString requestPath(QStringView uniqueName, QStringView token);

} // namespace Dragoman

Q_DECLARE_METATYPE(Dragoman::Reply)
Q_DECLARE_METATYPE(Dragoman::SentenceSpan)

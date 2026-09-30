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

/**
 * The request object path for @p token on the connection named
 * @p uniqueName: every character of the unique name that is not an ASCII
 * letter or digit becomes '_' (so the leading ':' does too).
 */
[[nodiscard]] DRAGOMANQT_EXPORT QString requestPath(QStringView uniqueName, QStringView token);

} // namespace Dragoman

Q_DECLARE_METATYPE(Dragoman::Reply)

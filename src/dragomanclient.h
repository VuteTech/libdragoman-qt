/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include "dragomanqt_export.h"
#include "dragomantypes.h"

#include <QList>
#include <QObject>
#include <QStringList>

#include <functional>
#include <memory>

namespace Dragoman
{

class JobPrivate;

/**
 * One asynchronous operation on the daemon: a single request object, or a
 * chain of them (translating after installing a missing pair).
 *
 * finished() is emitted exactly once, never from inside the Client call
 * that created the job, so connecting right after the call is safe. The
 * job deletes itself afterwards; keep a QPointer to it.
 */
class DRAGOMANQT_EXPORT Job : public QObject
{
    Q_OBJECT
public:
    ~Job() override;

    /**
     * Asks the daemon to cancel the request in flight and finishes the job
     * as cancelled at once. Does nothing once the job has finished.
     */
    void cancel();

    [[nodiscard]] bool isFinished() const;

Q_SIGNALS:
    /// Download, load or document progress, with @p fraction clamped to [0, 1].
    void progress(double fraction, const QString &stage);
    void finished(const Dragoman::Reply &reply);

private:
    friend class Client;
    friend class JobPrivate;
    friend class ClientPrivate;
    explicit Job(QObject *parent);

    std::unique_ptr<JobPrivate> d;
};

/**
 * A QtDBus client for the dragomand daemon (API dev.l10n_bg.dragomand.Translator1).
 *
 * The daemon follows the xdg-desktop-portal request pattern: slow methods
 * return a request object path immediately, and the outcome arrives exactly
 * once as a Response(u code, a{sv} results) signal on that object. The
 * client picks a handle_token, derives the request path itself, subscribes
 * to the request's signals before calling the method (the subscription and
 * the call travel on the same connection, so the bus sees them in order and
 * no signal can be missed), and then waits.
 *
 * Callbacks run on this object's thread; destroying the client drops every
 * pending callback and job.
 */
class DRAGOMANQT_EXPORT Client : public QObject
{
    Q_OBJECT
public:
    struct TranslateOptions {
        /// On NotInstalled, run PreparePair (with progress) and retry once.
        bool installOnDemand = true;
        bool allowPivot = true;
        /// The segments are HTML; the markup is kept in the translation.
        bool html = false;
        /// "interactive" or "batch".
        QString priority = QStringLiteral("interactive");
    };

    using PairsCallback = std::function<void(const QList<PairInfo> &pairs, const QString &error)>;
    using StatusCallback = std::function<void(const DaemonStatus &status, const QString &error)>;
    using ErrorCallback = std::function<void(const QString &error)>;

    explicit Client(QObject *parent = nullptr);
    ~Client() override;

    /// Translates @p segments. Success carries translations() and pivot().
    [[nodiscard]] Job *translate(const QString &source, const QString &target, const QStringList &segments, const TranslateOptions &options);
    [[nodiscard]] Job *translate(const QString &source, const QString &target, const QStringList &segments)
    {
        return translate(source, target, segments, TranslateOptions{});
    }
    /// Installs the pair if missing (network), then loads it.
    [[nodiscard]] Job *preparePair(const QString &source, const QString &target, bool allowPivot = true);
    /// Installs or upgrades one pair from the provider (network).
    [[nodiscard]] Job *installPair(const QString &source, const QString &target);
    /// Refreshes the daemon's record cache (network); results["updates"].
    [[nodiscard]] Job *checkForUpdates();

    /// Every pair the daemon knows, installed or available.
    void listPairs(PairsCallback callback);
    /// Removes every user-store copy of the pair.
    void removePair(const QString &source, const QString &target, ErrorCallback callback);
    void status(StatusCallback callback);

private:
    std::unique_ptr<class ClientPrivate> d;
};

} // namespace Dragoman

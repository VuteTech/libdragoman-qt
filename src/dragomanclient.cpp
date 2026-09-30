/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "dragomanclient.h"
#include "dragoman_debug.h"

#include <KLocalizedString>

#include <QAtomicInteger>
#include <QCoreApplication>
#include <QDBusAbstractInterface>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace Dragoman
{

namespace
{

constexpr auto serviceName = "dev.l10n_bg.dragomand.Translator1";
constexpr auto objectPath = "/dev/l10n_bg/dragomand/Translator1";
constexpr auto translatorInterface = "dev.l10n_bg.dragomand.Translator1";
constexpr auto requestInterface = "dev.l10n_bg.dragomand.Request1";

// Response codes of the request pattern.
constexpr uint responseSuccess = 0;
constexpr uint responseCancelled = 1;

// The daemon replies fast once a model is warm, but a cold load of a base
// model takes seconds, and installing downloads tens of megabytes.
constexpr int translateTimeoutMs = 120 * 1000;
constexpr int installTimeoutMs = 600 * 1000;
constexpr int updatesTimeoutMs = 120 * 1000;
constexpr int methodTimeoutMs = 25 * 1000;

QDBusConnection bus()
{
    return QDBusConnection::sessionBus();
}

QDBusMessage methodCall(const QString &method)
{
    return QDBusMessage::createMethodCall(QString::fromLatin1(serviceName), QString::fromLatin1(objectPath), QString::fromLatin1(translatorInterface), method);
}

QString nextToken()
{
    static QAtomicInteger<quint64> counter;
    return u"krakoman_%1_%2"_s.arg(QCoreApplication::applicationPid()).arg(counter.fetchAndAddRelaxed(1));
}

Reply failure(const QString &message, const QString &errorName = {})
{
    Reply reply;
    reply.error = message;
    reply.errorName = errorName;
    return reply;
}

QString messageOf(const QDBusError &error)
{
    return error.message().isEmpty() ? error.name() : error.message();
}

} // namespace

/**
 * The Request1 interface of one request object. QtDBus relays the object's
 * D-Bus signals to the Qt signals of the same name and signature, which
 * connect like any other Qt signal. Connecting to one adds the bus match
 * rule, before the method call that creates the request is sent.
 */
class RequestProxy : public QDBusAbstractInterface
{
    Q_OBJECT
public:
    RequestProxy(const QString &path, QObject *parent)
        : QDBusAbstractInterface(QString::fromLatin1(serviceName), path, requestInterface, bus(), parent)
    {
    }

Q_SIGNALS:
    void Response(uint code, const QVariantMap &results);
    void Progress(double fraction, const QString &stage);
};

class JobPrivate
{
public:
    explicit JobPrivate(Job *job)
        : q(job)
    {
    }

    /// Starts @p method with @p options (handle_token is added) as a leaf job.
    void call(const QString &method, QVariantList arguments, QVariantMap options, int timeoutMs);
    /// Makes @p child the step in flight; @p next receives its reply.
    void follow(Job *child, std::function<void(const Reply &)> next);
    void finish(Reply reply);
    /// finish(), from the event loop.
    void finishLater(Reply reply);

    Job *const q;
    QString requestPath;
    QPointer<Job> child;
    bool done = false;
};

Job::Job(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<JobPrivate>(this))
{
}

Job::~Job() = default;

bool Job::isFinished() const
{
    return d->done;
}

void JobPrivate::call(const QString &method, QVariantList arguments, QVariantMap options, int timeoutMs)
{
    const QString token = nextToken();
    requestPath = Dragoman::requestPath(bus().baseService(), token);
    auto *proxy = new RequestProxy(requestPath, q);
    if (!proxy->connection().isConnected()) {
        qCWarning(DRAGOMAN_LOG) << "no session bus connection";
        finishLater(failure(i18n("Cannot connect to the session bus.")));
        return;
    }
    QObject::connect(proxy, &RequestProxy::Response, q, [this, method](uint code, const QVariantMap &results) {
        Reply reply;
        reply.results = results;
        if (code == responseSuccess) {
            reply.status = Reply::Status::Success;
        } else if (code == responseCancelled) {
            reply.status = Reply::Status::Cancelled;
            reply.error = i18n("The request was cancelled.");
        } else {
            const QString message = results.value(u"error"_s).toString();
            reply.error = message.isEmpty() ? i18n("The translation daemon reported a failure.") : message;
            qCWarning(DRAGOMAN_LOG) << method << "failed:" << reply.error;
        }
        finish(std::move(reply));
    });
    QObject::connect(proxy, &RequestProxy::Progress, q, [this](double fraction, const QString &stage) {
        // The daemon is another process: clamp before anything renders it.
        Q_EMIT q->progress(std::clamp(fraction, 0.0, 1.0), stage);
    });
    QTimer::singleShot(timeoutMs, q, [this] {
        finish(failure(i18n("No answer from the translation daemon.")));
    });

    options.insert(u"handle_token"_s, token);
    arguments.append(QVariant::fromValue(options));
    QDBusMessage message = methodCall(method);
    message.setArguments(arguments);
    auto *watcher = new QDBusPendingCallWatcher(bus().asyncCall(message, methodTimeoutMs), q);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, q, [this, method](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (!reply.isError()) {
            return; // the Response signal carries the outcome
        }
        const QDBusError error = reply.error();
        if (error.name() != Errors::NotInstalled) {
            qCWarning(DRAGOMAN_LOG) << method << "call failed:" << error.name() << error.message();
        }
        finish(failure(messageOf(error), error.name()));
    });
}

void JobPrivate::follow(Job *next, std::function<void(const Reply &)> then)
{
    child = next;
    QObject::connect(next, &Job::progress, q, &Job::progress);
    QObject::connect(next, &Job::finished, q, [this, then = std::move(then)](const Reply &reply) {
        child = nullptr;
        if (!done) {
            then(reply);
        }
    });
}

void Job::cancel()
{
    if (d->done) {
        return;
    }
    if (d->child) {
        d->child->cancel(); // its reply reaches follow()'s callback
        if (d->done) {
            return;
        }
    } else if (!d->requestPath.isEmpty()) {
        // Fire and forget: the object is gone if the request just finished.
        bus().asyncCall(QDBusMessage::createMethodCall(QString::fromLatin1(serviceName), d->requestPath, QString::fromLatin1(requestInterface), u"Cancel"_s));
    }
    Reply reply;
    reply.status = Reply::Status::Cancelled;
    reply.error = i18n("The request was cancelled.");
    d->finish(std::move(reply));
}

void JobPrivate::finish(Reply reply)
{
    if (std::exchange(done, true)) {
        return;
    }
    Q_EMIT q->finished(reply);
    q->deleteLater();
}

void JobPrivate::finishLater(Reply reply)
{
    QTimer::singleShot(0, q, [this, reply = std::move(reply)] {
        finish(reply);
    });
}

class ClientPrivate
{
public:
    explicit ClientPrivate(Client *client)
        : q(client)
    {
    }

    Job *newJob()
    {
        return new Job(q);
    }
    static JobPrivate *of(Job *job)
    {
        return job->d.get();
    }
    Job *start(const QString &method, const QVariantList &arguments, const QVariantMap &options, int timeoutMs)
    {
        auto *job = newJob();
        of(job)->call(method, arguments, options, timeoutMs);
        return job;
    }

    Client *const q;
};

Client::Client(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<ClientPrivate>(this))
{
    qRegisterMetaType<Dragoman::Reply>();
}

Client::~Client() = default;

Job *Client::translate(const QString &source, const QString &target, const QStringList &segments, const TranslateOptions &options)
{
    const QVariantMap callOptions{
        {u"priority"_s, options.priority},
        {u"allow_pivot"_s, options.allowPivot},
        {u"html"_s, options.html},
    };
    const QVariantList arguments{source, target, segments};
    auto *job = d->newJob();
    auto *jd = ClientPrivate::of(job);
    jd->follow(d->start(u"Translate"_s, arguments, callOptions, translateTimeoutMs),
               [this, jd, source, target, arguments, callOptions, options](const Reply &reply) {
                   if (!options.installOnDemand || reply.errorName != Errors::NotInstalled) {
                       jd->finish(reply);
                       return;
                   }
                   jd->follow(preparePair(source, target, options.allowPivot), [this, jd, arguments, callOptions](const Reply &prepared) {
                       if (!prepared.ok()) {
                           jd->finish(prepared);
                           return;
                       }
                       jd->follow(d->start(u"Translate"_s, arguments, callOptions, translateTimeoutMs), [jd](Reply retried) {
                           retried.prepared = true;
                           jd->finish(std::move(retried));
                       });
                   });
               });
    return job;
}

Job *Client::preparePair(const QString &source, const QString &target, bool allowPivot)
{
    return d->start(u"PreparePair"_s, {source, target}, {{u"allow_pivot"_s, allowPivot}}, installTimeoutMs);
}

Job *Client::installPair(const QString &source, const QString &target)
{
    return d->start(u"InstallPair"_s, {source, target}, {}, installTimeoutMs);
}

Job *Client::checkForUpdates()
{
    return d->start(u"CheckForUpdates"_s, {}, {}, updatesTimeoutMs);
}

void Client::listPairs(PairsCallback callback)
{
    auto *watcher = new QDBusPendingCallWatcher(bus().asyncCall(methodCall(u"ListLanguagePairs"_s), methodTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [callback = std::move(callback)](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusMessage reply = watcher->reply();
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            qCWarning(DRAGOMAN_LOG) << "ListLanguagePairs failed:" << reply.errorName() << reply.errorMessage();
            callback({}, reply.errorMessage().isEmpty() ? i18n("No answer from the translation daemon.") : reply.errorMessage());
            return;
        }
        QList<PairInfo> pairs;
        const auto array = reply.arguments().at(0).value<QDBusArgument>();
        array.beginArray();
        while (!array.atEnd()) {
            QVariantMap record;
            array >> record;
            if (PairInfo info = PairInfo::fromRecord(record); info.isValid()) {
                pairs.append(std::move(info));
            }
        }
        array.endArray();
        callback(pairs, QString());
    });
}

void Client::removePair(const QString &source, const QString &target, ErrorCallback callback)
{
    QDBusMessage message = methodCall(u"RemovePair"_s);
    message.setArguments({source, target});
    auto *watcher = new QDBusPendingCallWatcher(bus().asyncCall(message, methodTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [callback = std::move(callback)](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusPendingReply<> reply = *watcher;
        if (reply.isError()) {
            qCWarning(DRAGOMAN_LOG) << "RemovePair failed:" << reply.error().name() << reply.error().message();
            callback(messageOf(reply.error()));
        } else {
            callback(QString());
        }
    });
}

void Client::status(StatusCallback callback)
{
    auto *watcher = new QDBusPendingCallWatcher(bus().asyncCall(methodCall(u"GetStatus"_s), methodTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [callback = std::move(callback)](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusPendingReply<QVariantMap> reply = *watcher;
        if (reply.isError()) {
            qCWarning(DRAGOMAN_LOG) << "GetStatus failed:" << reply.error().name() << reply.error().message();
            callback({}, messageOf(reply.error()));
        } else {
            callback(DaemonStatus::fromMap(reply.value()), QString());
        }
    });
}

} // namespace Dragoman

#include "dragomanclient.moc"

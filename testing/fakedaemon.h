/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

/*
 * A fake dragomand on a private bus. start() runs its own dbus-daemon, with
 * no service directories so that nothing can be activated, points the
 * session bus at it, and registers the fake Translator1 service on a second
 * connection, so every message really travels through the bus. The fake
 * emits Response before it even returns the method reply, which only works
 * when the client subscribed first.
 */

#pragma once

#include "dragomantypes.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <optional>

#include <unistd.h>

namespace Fake
{

/// One (uuuu) entry of Translate's "sentences" result.
struct SentenceSpan {
    uint sourceBegin = 0;
    uint sourceEnd = 0;
    uint targetBegin = 0;
    uint targetEnd = 0;
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const SentenceSpan &span)
{
    argument.beginStructure();
    argument << span.sourceBegin << span.sourceEnd << span.targetBegin << span.targetEnd;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, SentenceSpan &span)
{
    argument.beginStructure();
    argument >> span.sourceBegin >> span.sourceEnd >> span.targetBegin >> span.targetEnd;
    argument.endStructure();
    return argument;
}

} // namespace Fake

Q_DECLARE_METATYPE(Fake::SentenceSpan)

namespace Fake
{

using namespace Qt::StringLiterals;

inline constexpr auto serviceName = "dev.l10n_bg.dragomand.Translator1";
inline constexpr auto objectPath = "/dev/l10n_bg/dragomand/Translator1";
inline constexpr auto requestInterface = "dev.l10n_bg.dragomand.Request1";

/// A request object held open until the client cancels it.
class HeldRequest : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "dev.l10n_bg.dragomand.Request1")
public:
    HeldRequest(const QDBusConnection &connection, const QString &path, int *cancelCount)
        : m_connection(connection)
        , m_path(path)
        , m_cancelCount(cancelCount)
    {
    }

public Q_SLOTS:
    Q_SCRIPTABLE void Cancel()
    {
        ++*m_cancelCount;
        QDBusMessage response = QDBusMessage::createSignal(m_path, QString::fromLatin1(requestInterface), u"Response"_s);
        response << 1U << QVariantMap();
        m_connection.send(response);
        m_connection.unregisterObject(m_path);
        deleteLater();
    }

private:
    QDBusConnection m_connection;
    QString m_path;
    int *m_cancelCount;
};

class Translator : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "dev.l10n_bg.dragomand.Translator1")
public:
    explicit Translator(const QDBusConnection &connection)
        : m_connection(connection)
    {
    }

    QSet<QString> installed{u"bg-en"_s};
    QString failWith; ///< when set, Translate answers with this error
    bool hold = false; ///< when set, Translate never answers until cancelled
    int prepareCalls = 0;
    int cancelCalls = 0;
    QList<qsizetype> batchSizes; ///< segments per Translate call
    /// The daemon's defaults, with its types (t, u, b).
    QVariantMap config{
        {u"memory_budget_mb"_s, QVariant::fromValue(qulonglong(512))},
        {u"keep_warm"_s, QVariant::fromValue(uint(2))},
        {u"keep_warm_seconds"_s, QVariant::fromValue(qulonglong(600))},
        {u"idle_exit_seconds"_s, QVariant::fromValue(qulonglong(60))},
        {u"network"_s, true},
        {u"allow_prerelease"_s, false},
    };

public Q_SLOTS:
    Q_SCRIPTABLE QDBusObjectPath Translate(const QString &source, const QString &target, const QStringList &segments, const QVariantMap &options)
    {
        if (!installed.contains(source + u'-' + target)) {
            sendErrorReply(u"dev.l10n_bg.dragomand.Error.NotInstalled"_s, u"%1-%2 is not installed"_s.arg(source, target));
            return {};
        }
        batchSizes.append(segments.size());
        const QString path = pathFor(options);
        if (hold) {
            auto *request = new HeldRequest(m_connection, path, &cancelCalls);
            m_connection.registerObject(path, request, QDBusConnection::ExportScriptableSlots);
        } else if (!failWith.isEmpty()) {
            respond(path, 2, {{u"error"_s, failWith}});
        } else {
            // Upper case keeps every length, so each segment is one
            // sentence covering the same code points on both sides.
            QStringList translations;
            QList<QList<SentenceSpan>> sentences;
            for (const QString &segment : segments) {
                translations.append(segment.toUpper());
                const auto length = uint(segment.toUcs4().size());
                sentences.append({SentenceSpan{0, length, 0, length}});
            }
            QVariantMap results{{u"translations"_s, translations}};
            if (options.value(u"sentences"_s).toBool()) {
                results.insert(u"sentences"_s, QVariant::fromValue(sentences));
            }
            respond(path, 0, results);
        }
        return QDBusObjectPath(path);
    }

    /// Upper-cases every line with a letter or digit, like the real daemon
    /// translates them.
    Q_SCRIPTABLE QDBusObjectPath TranslateFd(const QString &source,
                                             const QString &target,
                                             const QDBusUnixFileDescriptor &input,
                                             const QDBusUnixFileDescriptor &output,
                                             const QVariantMap &options)
    {
        if (!installed.contains(source + u'-' + target)) {
            sendErrorReply(u"dev.l10n_bg.dragomand.Error.NotInstalled"_s, u"%1-%2 is not installed"_s.arg(source, target));
            return {};
        }
        QByteArray bytes;
        char buffer[4096];
        for (;;) {
            const auto got = ::read(input.fileDescriptor(), buffer, sizeof buffer);
            if (got <= 0) {
                break;
            }
            bytes.append(buffer, got);
        }
        QStringList lines = QString::fromUtf8(bytes).split(u'\n');
        uint translated = 0;
        for (QString &line : lines) {
            if (std::ranges::any_of(line, [](QChar c) {
                    return c.isLetterOrNumber();
                })) {
                line = line.toUpper();
                ++translated;
            }
        }
        const QByteArray result = lines.join(u'\n').toUtf8();
        if (::write(output.fileDescriptor(), result.constData(), size_t(result.size())) != result.size()) {
            qWarning("fake daemon: short write");
        }
        const QString path = pathFor(options);
        QDBusMessage progress = QDBusMessage::createSignal(path, QString::fromLatin1(requestInterface), u"Progress"_s);
        progress << 1.0 << u"translating"_s;
        m_connection.send(progress);
        respond(path, 0, {{u"lines"_s, translated}});
        return QDBusObjectPath(path);
    }

    /// Cyrillic text is Bulgarian, anything else English, within the candidates.
    Q_SCRIPTABLE QVariantMap DetectLanguage(const QString &text, const QVariantMap &options)
    {
        const bool cyrillic = std::ranges::any_of(text, [](QChar c) {
            return c.script() == QChar::Script_Cyrillic;
        });
        const QString language = cyrillic ? u"bg"_s : u"en"_s;
        const QStringList candidates = qdbus_cast<QStringList>(options.value(u"candidates"_s));
        QVariantMap results{{u"confidence"_s, 0.9}, {u"reliable"_s, true}};
        if (candidates.isEmpty() || candidates.contains(language)) {
            results.insert(u"language"_s, language);
        }
        return results;
    }

    Q_SCRIPTABLE QVariantMap GetConfig()
    {
        return config;
    }

    Q_SCRIPTABLE void SetConfig(const QVariantMap &changes)
    {
        for (auto it = changes.cbegin(); it != changes.cend(); ++it) {
            if (!config.contains(it.key())) {
                sendErrorReply(u"dev.l10n_bg.dragomand.Error.InvalidArgument"_s, u"unknown setting %1"_s.arg(it.key()));
                return;
            }
        }
        for (auto it = changes.cbegin(); it != changes.cend(); ++it) {
            config.insert(it.key(), it.value());
        }
        QDBusMessage changed = QDBusMessage::createSignal(QString::fromLatin1(objectPath), QString::fromLatin1(serviceName), u"ConfigChanged"_s);
        changed << config;
        m_connection.send(changed);
    }

    Q_SCRIPTABLE QDBusObjectPath PreparePair(const QString &source, const QString &target, const QVariantMap &options)
    {
        ++prepareCalls;
        return install(source, target, options);
    }

    Q_SCRIPTABLE QDBusObjectPath InstallPair(const QString &source, const QString &target, const QVariantMap &options)
    {
        return install(source, target, options);
    }

    Q_SCRIPTABLE void RemovePair(const QString &source, const QString &target)
    {
        if (!installed.remove(source + u'-' + target)) {
            sendErrorReply(u"dev.l10n_bg.dragomand.Error.NotInstalled"_s, u"%1-%2 is not installed"_s.arg(source, target));
        }
    }

    Q_SCRIPTABLE QDBusObjectPath CheckForUpdates(const QVariantMap &options)
    {
        const QString path = pathFor(options);
        respond(path, 0, {{u"updates"_s, QStringList{u"bg-en 3.0 -> 3.1"_s}}});
        return QDBusObjectPath(path);
    }

    Q_SCRIPTABLE QList<QVariantMap> ListLanguagePairs()
    {
        QList<QVariantMap> pairs;
        for (const auto &[source, target] : {std::pair(u"bg"_s, u"en"_s), std::pair(u"en"_s, u"bg"_s), std::pair(u"de"_s, u"en"_s)}) {
            QVariantMap record{{u"source"_s, source}, {u"target"_s, target}, {u"available_version"_s, u"3.1"_s}, {u"architecture"_s, u"base"_s}};
            if (installed.contains(source + u'-' + target)) {
                record.insert(u"installed_version"_s, u"3.0"_s);
                record.insert(u"origin"_s, u"user"_s);
                record.insert(u"size"_s, QVariant::fromValue(quint64(32 * 1024 * 1024)));
            }
            if (source == u"bg") {
                record.insert(u"release_status"_s, u"Release"_s);
                record.insert(u"quality"_s, 0.8719);
            }
            pairs.append(record);
        }
        return pairs;
    }

    Q_SCRIPTABLE QVariantMap GetStatus()
    {
        return {
            {u"version"_s, u"0.1.0"_s},
            {u"loaded"_s, QStringList{u"bg-en"_s, u"zh-Hant-en"_s}},
            {u"queued"_s, 2U},
        };
    }

private:
    QDBusObjectPath install(const QString &source, const QString &target, const QVariantMap &options)
    {
        const QString path = pathFor(options);
        QDBusMessage progress = QDBusMessage::createSignal(path, QString::fromLatin1(requestInterface), u"Progress"_s);
        progress << 1.5 << u"download"_s; // out of range on purpose: the client clamps
        m_connection.send(progress);
        installed.insert(source + u'-' + target);
        respond(path, 0, {{u"version"_s, u"3.1"_s}});
        return QDBusObjectPath(path);
    }

    QString pathFor(const QVariantMap &options) const
    {
        return Dragoman::requestPath(message().service(), options.value(u"handle_token"_s).toString());
    }

    // Sent before the method returns: the client must already listen.
    void respond(const QString &path, uint code, const QVariantMap &results)
    {
        QDBusMessage response = QDBusMessage::createSignal(path, QString::fromLatin1(requestInterface), u"Response"_s);
        response << code << results;
        m_connection.send(response);
    }

    QDBusConnection m_connection;
};

/// The private bus and the fake service on it.
class Daemon
{
public:
    /// Call from initTestCase(), before anything touches the session bus.
    void start()
    {
        const QString dbusDaemon = QStandardPaths::findExecutable(u"dbus-daemon"_s);
        if (dbusDaemon.isEmpty()) {
            QSKIP("dbus-daemon is not installed");
        }
        QVERIFY(m_dir.isValid());
        const QString config = m_dir.filePath(u"bus.conf"_s);
        QFile file(config);
        QVERIFY(file.open(QIODevice::WriteOnly));
        // Ordinary literals: moc's preprocessor misreads "//" inside raw strings.
        file.write(
            "<busconfig>\n"
            "  <type>session</type>\n"
            "  <listen>unix:tmpdir=/tmp</listen>\n"
            "  <policy context=\"default\">\n"
            "    <allow send_destination=\"*\" eavesdrop=\"true\"/>\n"
            "    <allow eavesdrop=\"true\"/>\n"
            "    <allow own=\"*\"/>\n"
            "  </policy>\n"
            "</busconfig>\n");
        file.close();

        m_bus.start(dbusDaemon, {u"--config-file"_s, config, u"--nofork"_s, u"--print-address=1"_s});
        QVERIFY(m_bus.waitForStarted());
        QVERIFY(m_bus.waitForReadyRead(10000));
        const QByteArray address = m_bus.readLine().trimmed();
        QVERIFY(!address.isEmpty());
        qputenv("DBUS_SESSION_BUS_ADDRESS", address);
        QVERIFY(QDBusConnection::sessionBus().isConnected());

        qDBusRegisterMetaType<QList<QVariantMap>>();
        qDBusRegisterMetaType<SentenceSpan>();
        qDBusRegisterMetaType<QList<SentenceSpan>>();
        qDBusRegisterMetaType<QList<QList<SentenceSpan>>>();
        m_connection.emplace(QDBusConnection::connectToBus(QString::fromUtf8(address), u"fake-dragomand"_s));
        QVERIFY(m_connection->isConnected());
        translator = new Translator(*m_connection);
        QVERIFY(m_connection->registerObject(QString::fromLatin1(objectPath), translator, QDBusConnection::ExportScriptableSlots));
        QVERIFY(setRegistered(true));
    }

    /// Call from cleanupTestCase().
    void stop()
    {
        if (m_connection) {
            QDBusConnection::disconnectFromBus(u"fake-dragomand"_s);
        }
        delete translator;
        translator = nullptr;
        // Only the dbus-daemon this test started.
        m_bus.kill();
        m_bus.waitForFinished();
    }

    /// Owns or releases the service name, to simulate a missing daemon.
    bool setRegistered(bool registered)
    {
        return registered ? m_connection->registerService(QString::fromLatin1(serviceName)) : m_connection->unregisterService(QString::fromLatin1(serviceName));
    }

    Translator *translator = nullptr;

private:
    QTemporaryDir m_dir;
    QProcess m_bus;
    std::optional<QDBusConnection> m_connection;
};

} // namespace Fake

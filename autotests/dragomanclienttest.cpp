/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

// Dragoman::Client against the fake daemon on a private bus.

#include "dragomanclient.h"
#include "fakedaemon.h"

#include <QPointer>
#include <QSignalSpy>

using namespace Qt::StringLiterals;

namespace
{

/// Waits for @p job and returns its reply, counting the finished() signals.
std::optional<Dragoman::Reply> waitFor(Dragoman::Job *job, int *emissions = nullptr)
{
    std::optional<Dragoman::Reply> reply;
    QObject::connect(job, &Dragoman::Job::finished, job, [&reply, emissions](const Dragoman::Reply &r) {
        reply = r;
        if (emissions) {
            ++*emissions;
        }
    });
    if (!QTest::qWaitFor([&reply] {
            return reply.has_value();
        })) {
        return std::nullopt;
    }
    return reply;
}

} // namespace

class DragomanClientTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        m_daemon.start();
        if (QTest::currentTestFailed() || QTest::currentTestResolved()) {
            return; // skipped without dbus-daemon, or failed
        }
    }

    void cleanupTestCase()
    {
        m_daemon.stop();
    }

    void translatesInstalledPair()
    {
        Dragoman::Client client;
        int emissions = 0;
        QPointer<Dragoman::Job> job = client.translate(u"bg"_s, u"en"_s, {u"добро"_s, u"утро"_s});
        const auto reply = waitFor(job, &emissions);
        QVERIFY(reply.has_value());
        QVERIFY2(reply->ok(), qPrintable(reply->error));
        QCOMPARE(reply->translations(), QStringList({u"ДОБРО"_s, u"УТРО"_s}));
        QVERIFY(!reply->prepared);
        QTest::qWait(200);
        QCOMPARE(emissions, 1); // exactly once, even after the method reply arrives
        QVERIFY(!job); // and the job cleaned up after itself
    }

    void installsMissingPairOnDemand()
    {
        Dragoman::Client client;
        const int prepareBefore = m_daemon.translator->prepareCalls;
        auto *job = client.translate(u"de"_s, u"en"_s, {u"guten morgen"_s});
        QSignalSpy progress(job, &Dragoman::Job::progress);
        const auto reply = waitFor(job);
        QVERIFY(reply.has_value());
        QVERIFY2(reply->ok(), qPrintable(reply->error));
        QCOMPARE(reply->translations(), QStringList({u"GUTEN MORGEN"_s}));
        QVERIFY(reply->prepared);
        QCOMPARE(m_daemon.translator->prepareCalls, prepareBefore + 1);
        QCOMPARE(progress.count(), 1);
        QCOMPARE(progress.at(0).at(0).toDouble(), 1.0); // clamped
        QCOMPARE(progress.at(0).at(1).toString(), u"download"_s);
        m_daemon.translator->installed.remove(u"de-en"_s);
    }

    void reportsMissingPairWithoutInstalling()
    {
        Dragoman::Client client;
        Dragoman::Client::TranslateOptions options;
        options.installOnDemand = false;
        const int prepareBefore = m_daemon.translator->prepareCalls;
        const auto reply = waitFor(client.translate(u"de"_s, u"en"_s, {u"hallo"_s}, options));
        QVERIFY(reply.has_value());
        QVERIFY(!reply->ok());
        QCOMPARE(reply->errorName, Dragoman::Errors::NotInstalled);
        QCOMPARE(m_daemon.translator->prepareCalls, prepareBefore);
    }

    void reportsErrorResponses()
    {
        m_daemon.translator->failWith = u"the engine failed"_s;
        Dragoman::Client client;
        const auto reply = waitFor(client.translate(u"bg"_s, u"en"_s, {u"текст"_s}));
        m_daemon.translator->failWith.clear();
        QVERIFY(reply.has_value());
        QVERIFY(!reply->ok());
        QVERIFY(!reply->cancelled());
        QCOMPARE(reply->error, u"the engine failed"_s);
    }

    void cancelsRequestInFlight()
    {
        m_daemon.translator->hold = true;
        m_daemon.translator->batchSizes.clear();
        Dragoman::Client client;
        const int cancelBefore = m_daemon.translator->cancelCalls;
        int emissions = 0;
        auto *job = client.translate(u"bg"_s, u"en"_s, {u"текст"_s});
        std::optional<Dragoman::Reply> reply;
        connect(job, &Dragoman::Job::finished, this, [&](const Dragoman::Reply &r) {
            reply = r;
            ++emissions;
        });
        // Let the request object appear before cancelling it.
        QTRY_COMPARE(m_daemon.translator->batchSizes.isEmpty(), false);
        job->cancel();
        m_daemon.translator->hold = false;
        QVERIFY(reply.has_value());
        QVERIFY(reply->cancelled());
        QTRY_COMPARE(m_daemon.translator->cancelCalls, cancelBefore + 1);
        QTest::qWait(200); // the daemon's own Response(1) must not repeat it
        QCOMPARE(emissions, 1);
    }

    void installsAndRemovesPairs()
    {
        Dragoman::Client client;
        auto *job = client.installPair(u"de"_s, u"en"_s);
        QSignalSpy progress(job, &Dragoman::Job::progress);
        const auto installed = waitFor(job);
        QVERIFY(installed.has_value());
        QVERIFY2(installed->ok(), qPrintable(installed->error));
        QCOMPARE(installed->results.value(u"version"_s).toString(), u"3.1"_s);
        QCOMPARE(progress.count(), 1);
        QVERIFY(m_daemon.translator->installed.contains(u"de-en"_s));

        std::optional<QString> error;
        client.removePair(u"de"_s, u"en"_s, [&error](const QString &e) {
            error = e;
        });
        QTRY_VERIFY(error.has_value());
        QVERIFY2(error->isEmpty(), qPrintable(*error));
        QVERIFY(!m_daemon.translator->installed.contains(u"de-en"_s));

        error.reset();
        client.removePair(u"de"_s, u"en"_s, [&error](const QString &e) {
            error = e;
        });
        QTRY_VERIFY(error.has_value());
        QVERIFY(!error->isEmpty());
    }

    void listsPairs()
    {
        Dragoman::Client client;
        std::optional<QList<Dragoman::PairInfo>> pairs;
        QString error;
        client.listPairs([&](const QList<Dragoman::PairInfo> &p, const QString &e) {
            pairs = p;
            error = e;
        });
        QTRY_VERIFY(pairs.has_value());
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(pairs->size(), 3);
        const auto &bgEn = pairs->at(0);
        QCOMPARE(bgEn.source, u"bg"_s);
        QCOMPARE(bgEn.installedVersion, u"3.0"_s);
        QCOMPARE(bgEn.size, 32 * 1024 * 1024);
        QVERIFY(bgEn.hasUpdate());
        QVERIFY(!pairs->at(2).isInstalled());
    }

    void reportsStatus()
    {
        Dragoman::Client client;
        std::optional<Dragoman::DaemonStatus> status;
        client.status([&](const Dragoman::DaemonStatus &s, const QString &e) {
            QVERIFY2(e.isEmpty(), qPrintable(e));
            status = s;
        });
        QTRY_VERIFY(status.has_value());
        QCOMPARE(status->version, u"0.1.0"_s);
        QCOMPARE(status->loaded, QStringList({u"bg-en"_s, u"zh-Hant-en"_s}));
        QCOMPARE(status->queued, 2);
    }

    void checksForUpdates()
    {
        Dragoman::Client client;
        const auto reply = waitFor(client.checkForUpdates());
        QVERIFY(reply.has_value());
        QVERIFY(reply->ok());
        QCOMPARE(Dragoman::toStringList(reply->results.value(u"updates"_s)).size(), 1);
    }

    void failsCleanlyWithoutDaemon()
    {
        QVERIFY(m_daemon.setRegistered(false));
        Dragoman::Client client;
        const auto reply = waitFor(client.translate(u"bg"_s, u"en"_s, {u"текст"_s}));
        std::optional<QString> listError;
        client.listPairs([&listError](const QList<Dragoman::PairInfo> &, const QString &e) {
            listError = e;
        });
        QTRY_VERIFY(listError.has_value());
        QVERIFY(m_daemon.setRegistered(true));
        QVERIFY(reply.has_value());
        QVERIFY(!reply->ok());
        QVERIFY(!reply->error.isEmpty());
        QVERIFY(!listError->isEmpty());
    }

private:
    Fake::Daemon m_daemon;
};

QTEST_GUILESS_MAIN(DragomanClientTest)

#include "dragomanclienttest.moc"

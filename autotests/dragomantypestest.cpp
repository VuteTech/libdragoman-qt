/*
    SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
    SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "dragomantypes.h"
#include "languagenames.h"

#include <QLocale>
#include <QTest>

using namespace Qt::StringLiterals;

class DragomanTypesTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        // English names, whatever the machine's language.
        qputenv("LANGUAGE", "C");
        QLocale::setDefault(QLocale::c());
    }

    void compareVersions_data()
    {
        QTest::addColumn<QString>("a");
        QTest::addColumn<QString>("b");
        QTest::addColumn<int>("expected");
        QTest::newRow("equal") << u"3.0"_s << u"3.0"_s << 0;
        QTest::newRow("missing part is zero") << u"3"_s << u"3.0"_s << 0;
        QTest::newRow("minor") << u"3.0"_s << u"3.1"_s << -1;
        QTest::newRow("numeric, not lexical") << u"3.10"_s << u"3.9"_s << 1;
        QTest::newRow("major") << u"4.0"_s << u"3.9"_s << 1;
        QTest::newRow("pre-release before release") << u"3.0a1"_s << u"3.0"_s << -1;
        QTest::newRow("pre-releases in order") << u"3.0a1"_s << u"3.0a2"_s << -1;
        QTest::newRow("alpha before beta") << u"3.0b1"_s << u"3.0a9"_s << 1;
        QTest::newRow("pre-release after older release") << u"3.1a1"_s << u"3.0"_s << 1;
    }

    void compareVersions()
    {
        QFETCH(QString, a);
        QFETCH(QString, b);
        QFETCH(int, expected);
        QCOMPARE(Dragoman::compareVersions(a, b), expected);
        QCOMPARE(Dragoman::compareVersions(b, a), -expected);
    }

    void requestPath()
    {
        QCOMPARE(Dragoman::requestPath(u":1.42", u"krakoman_7_0"), u"/dev/l10n_bg/dragomand/request/_1_42/krakoman_7_0"_s);
    }

    void pairInfo()
    {
        const auto installed = Dragoman::PairInfo::fromRecord({
            {u"source"_s, u"bg"_s},
            {u"target"_s, u"en"_s},
            {u"installed_version"_s, u"3.0"_s},
            {u"available_version"_s, u"3.1"_s},
            {u"origin"_s, u"user"_s},
            {u"size"_s, QVariant::fromValue(quint64(1234))},
        });
        QVERIFY(installed.isValid());
        QVERIFY(installed.isInstalled());
        QVERIFY(installed.hasUpdate());
        QVERIFY(installed.isRemovable());
        QCOMPARE(installed.size, 1234);

        auto system = installed;
        system.origin = u"system"_s;
        system.installedVersion = u"3.2"_s; // newer than the provider's
        QVERIFY(!system.hasUpdate());
        QVERIFY(!system.isRemovable());

        const auto available = Dragoman::PairInfo::fromRecord({{u"source"_s, u"de"_s}, {u"target"_s, u"en"_s}, {u"available_version"_s, u"3.0"_s}});
        QVERIFY(!available.isInstalled());
        QVERIFY(!available.hasUpdate());
        QVERIFY(!available.isRemovable());
        QCOMPARE(available.size, -1);

        QVERIFY(!Dragoman::PairInfo::fromRecord({{u"source"_s, u"de"_s}}).isValid());
    }

    void daemonStatus()
    {
        const auto status = Dragoman::DaemonStatus::fromMap({
            {u"version"_s, u"0.1.0"_s},
            {u"loaded"_s, QStringList{u"bg-en"_s}},
            {u"queued"_s, 3U},
        });
        QCOMPARE(status.version, u"0.1.0"_s);
        QCOMPARE(status.loaded, QStringList{u"bg-en"_s});
        QCOMPARE(status.queued, 3);
        QCOMPARE(status.residentMb, -1);
    }

    void languageName_data()
    {
        QTest::addColumn<QString>("code");
        QTest::addColumn<QString>("expected");
        QTest::newRow("plain") << u"bg"_s << u"Bulgarian"_s;
        QTest::newRow("Greek") << u"el"_s << u"Greek"_s;
        QTest::newRow("script subtag") << u"zh-Hant"_s << u"Chinese (Traditional)"_s;
        QTest::newRow("unknown") << u"qqq"_s << u"qqq"_s;
    }

    void languageName()
    {
        QFETCH(QString, code);
        QFETCH(QString, expected);
        QCOMPARE(Dragoman::languageName(code), expected);
    }
};

QTEST_GUILESS_MAIN(DragomanTypesTest)

#include "dragomantypestest.moc"

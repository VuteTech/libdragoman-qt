# SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
# SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
# SPDX-License-Identifier: GPL-3.0-or-later

include(ECMAddTests)

ecm_add_test(dragomantypestest.cpp
    LINK_LIBRARIES DragomanQt Qt6::Test)

# Against the fake daemon on a private bus (testing/fakedaemon.h).
ecm_add_test(dragomanclienttest.cpp ../testing/fakedaemon.h
    TEST_NAME dragomanclienttest
    LINK_LIBRARIES DragomanQt Qt6::Test)
target_include_directories(dragomanclienttest PRIVATE ../testing)

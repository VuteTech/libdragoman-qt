#
# spec file for package libdragoman-qt (openSUSE and Fedora targets on OBS)
#
# SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
# SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# @VERSION@ is stamped by packaging/obs/prepare.sh from the release tag; the
# release tarball carries it in .tarball-version as well, for CMake.

# The SO version is the minor version until 1.0 (see CMakeLists.txt).
%define sover @SOVERSION@

Name:           libdragoman-qt
Version:        @VERSION@
Release:        0
Summary:        Qt client library for the Dragomand translation daemon
License:        GPL-3.0-or-later
URL:            https://dragomand.l10n-bg.dev
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.24
BuildRequires:  gcc-c++
BuildRequires:  gettext
BuildRequires:  cmake(KF6I18n) >= 6.13
BuildRequires:  cmake(Qt6Core) >= 6.8
BuildRequires:  cmake(Qt6DBus) >= 6.8
BuildRequires:  cmake(Qt6Test) >= 6.8
%if 0%{?fedora}
BuildRequires:  extra-cmake-modules >= 6.13
BuildRequires:  ninja-build
BuildRequires:  dbus-daemon
%else
BuildRequires:  kf6-extra-cmake-modules >= 6.13
BuildRequires:  ninja
BuildRequires:  dbus-1
%endif

%description
A Qt 6 library for applications that translate text offline through the
Dragomand daemon and Mozilla's Firefox translation models. It wraps the
daemon's D-Bus API in asynchronous jobs: translation of segments and
documents, language detection, sentence alignment, model installation
and the daemon's settings.

%if 0%{?suse_version}
%package -n libdragoman-qt%{sover}
Summary:        Qt client library for the Dragomand translation daemon
Recommends:     dragomand

%description -n libdragoman-qt%{sover}
A Qt 6 library for applications that translate text offline through the
Dragomand daemon and Mozilla's Firefox translation models.
%endif

%package devel
Summary:        Development files for libdragoman-qt
%if 0%{?suse_version}
Requires:       libdragoman-qt%{sover} = %{version}
%else
Requires:       %{name}%{?_isa} = %{version}-%{release}
%endif
Requires:       cmake(Qt6Core) >= 6.8
Requires:       cmake(Qt6DBus) >= 6.8

%description devel
Headers and the CMake package (DragomanQt) for building applications on
libdragoman-qt, plus a fake daemon for their unit tests.

%prep
%setup -q

%build
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DKDE_INSTALL_USE_QT_SYS_PATHS=ON \
    -DBUILD_TESTING=ON
cmake --build build %{?_smp_mflags}

%check
ctest --test-dir build --output-on-failure

%install
DESTDIR=%{buildroot} cmake --install build
%find_lang %{name}

%if 0%{?suse_version}
%ldconfig_scriptlets -n libdragoman-qt%{sover}

%files -n libdragoman-qt%{sover} -f %{name}.lang
%else
%ldconfig_scriptlets

%files -f %{name}.lang
%endif
%license LICENSES/GPL-3.0-or-later.txt
%doc README.md
%{_libdir}/libdragoman-qt.so.%{sover}
%{_libdir}/libdragoman-qt.so.%{version}
%dir %{_datadir}/qlogging-categories6
%{_datadir}/qlogging-categories6/libdragoman-qt.categories

%files devel
%{_includedir}/DragomanQt
%{_libdir}/libdragoman-qt.so
%{_libdir}/cmake/DragomanQt

%changelog
* @RPM_DATE@ Blagovest Petrov <blagovest@petrovs.info> - @VERSION@
- Release @VERSION@

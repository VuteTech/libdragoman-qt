# libdragoman-qt

The Qt client library for [Dragomand](https://github.com/VuteTech/dragomand)
(website: [dragomand.l10n-bg.dev](https://dragomand.l10n-bg.dev)), the
per-user daemon that runs Mozilla's Firefox translation models locally
and serves them over D-Bus. It is shared by
[Krakoman](https://github.com/VuteTech/krakoman) and the other Qt front
ends.

The daemon follows the xdg-desktop-portal request pattern: slow methods
return a request object at once, and the outcome arrives later as a
`Response` signal on it. `Dragoman::Client` hides that behind jobs:

```cpp
#include <dragomanclient.h>

auto *client = new Dragoman::Client(this);
Dragoman::Job *job = client->translate(u"bg"_s, u"en"_s, {u"Добро утро"_s});
connect(job, &Dragoman::Job::finished, this, [](const Dragoman::Reply &reply) {
    if (reply.ok()) {
        qDebug() << reply.translations();
    }
});
```

- Each job finishes exactly once (with success, cancellation or an
  error), never from inside the call that created it, and can be
  cancelled. A missing language pair is installed on demand, with
  progress, unless the caller opts out.
- The client subscribes to a request's signals before it calls the
  method, so no signal is ever missed.
- `Dragoman::languageName()` names languages in the user's language from
  the iso-codes catalogs.

## Build and use

Needs CMake 3.24, extra-cmake-modules, Qt 6.8 (Core, DBus) and KF6 I18n
6.13 or newer:

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
cmake --install build --prefix ~/.local
```

Consumers use `find_package(DragomanQt REQUIRED)` and link
`DragomanQt::DragomanQt`. To build against an uninstalled tree, pass
`-DDragomanQt_DIR=<libdragoman-qt build>/buildtree`.

The API is young: until 1.0 the SO version follows the minor version, and
any minor release may change it.

### Testing against a fake daemon

`testing/fakedaemon.h` (installed as `DragomanQt/testing/fakedaemon.h`,
directory in `DragomanQt_TESTING_INCLUDE_DIR`) starts a private
`dbus-daemon` with a scriptable fake of the Translator1 service, so
autotests never touch the session bus. The library's own tests use it,
and so can every front end's.

## Translations

`scripts/update-translations.sh` extracts the strings into
`po/libdragoman-qt.pot` and merges them into every
`po/<language>/libdragoman-qt.po`.

## License

GPL-3.0-or-later. The project follows the [REUSE](https://reuse.software)
specification.

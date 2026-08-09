#pragma once

#include <QString>

class MainWindow;

namespace uitest {

/// Scripted end-to-end test driver. Enabled via the CACHYOS_PI_UI_TEST
/// environment variable (points at the report directory). Exercises install,
/// uninstall, upgrade, orphan, flatpak and theme flows against the
/// tests/fakebin command fixtures, writes PASS/FAIL lines and screenshots
/// into the report directory, and returns a process exit code.
int run(::MainWindow& window, const QString& reportDir);

}  // namespace uitest

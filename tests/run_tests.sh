#!/bin/bash
# End-to-end UI test runner.
# Uses tests/fakebin (fake pkexec/pacman/flatpak/socat/logname) so every
# privileged flow is exercised without touching the host system.
set -u

APP="${1:-build/cachyos-pi}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="$(cd "$ROOT" && realpath "$APP")"

REPORT_DIR="${CACHYOS_PI_REPORT:-$ROOT/build/test-report}"
FAKE_LOG="$REPORT_DIR/fakebin.log"
FIXTURE="$ROOT/tests/fixtures"
FAKEBIN="$ROOT/tests/fakebin"

rm -rf "$REPORT_DIR"
mkdir -p "$REPORT_DIR"

export PATH="$FAKEBIN:$PATH"
export FAKE_LOG
export FAKE_FIXTURE="$FIXTURE"
export QT_QPA_PLATFORM=offscreen

# isolate settings/cache so tests never touch real user state
export XDG_CONFIG_HOME="$REPORT_DIR/config"
export XDG_CACHE_HOME="$REPORT_DIR/cache"

echo "== running UI tests =="
timeout 240 env CACHYOS_PI_UI_TEST="$REPORT_DIR" "$APP" >"$REPORT_DIR/app-stdout.log" 2>&1
rc=$?
echo "exit=$rc"

if [ -f "$REPORT_DIR/report.txt" ]; then
    cat "$REPORT_DIR/report.txt"
else
    echo "NO REPORT PRODUCED"
    cat "$REPORT_DIR/app-stdout.log" | tail -20
    rc=1
fi

# --- single-instance guard ---
echo "== single instance guard =="
LOG_FILE="$XDG_CACHE_HOME/cachyos/cachyos-pi/cachyospi.log"
rm -f "$LOG_FILE"
"$APP" >/dev/null 2>&1 &
first=$!
sleep 4
"$APP" >"$REPORT_DIR/second-run.log" 2>&1 &
second=$!
sleep 3
sessions=$(grep -c "SESSION START" "$LOG_FILE" 2>/dev/null || true)
if kill -0 "$second" 2>/dev/null && [ "$sessions" = "1" ]; then
    echo "PASS  second instance blocked by the single-instance guard (1 session, 2nd alive at lock dialog)"
else
    echo "FAIL  single-instance guard (sessions=$sessions, second_alive=$(kill -0 "$second" 2>/dev/null && echo yes || echo no))"
    rc=1
fi
kill "$first" "$second" 2>/dev/null
wait 2>/dev/null

exit $rc

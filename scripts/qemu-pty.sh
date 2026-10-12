#!/bin/bash
# Run QEMU and, when inside a tmux session, automatically open a minicom
# pane for each PTY serial port QEMU creates.
#
# When a -serial pty argument is present, QEMU is started paused (-S) so
# minicom can attach before any guest output is produced. Once every new
# PTY has a minicom attached, the CPU is released via the QEMU monitor.
# When QEMU exits, all minicom panes are killed automatically.
#
# Usage: qemu-pty.sh <qemu-binary> [qemu-args...]

if [ -z "$TMUX" ]; then
    exec "$@"
fi

# Check whether any -serial pty argument is present.
has_pty_serial=0
prev=""
for arg in "$@"; do
    [[ "$prev" == "-serial" && "$arg" == "pty" ]] && { has_pty_serial=1; break; }
    prev="$arg"
done

if (( ! has_pty_serial )); then
    "$@"
    rc=$?
    if [ $rc -ne 0 ]; then
        printf '\n[QEMU exited with rc=%d - press Enter to close]\n' "$rc"
        read -r _ </dev/tty 2>/dev/null || sleep 5
    fi
    exit $rc
fi

mon_sock="/tmp/qemu-mon-$$.sock"

# Send one line to the QEMU monitor UNIX socket.
qemu_monitor_cmd() {
    if command -v socat >/dev/null 2>&1; then
        printf '%s\n' "$1" | socat -t 2 - "UNIX-CONNECT:$mon_sock" >/dev/null 2>&1 && return 0
    fi
    if command -v nc >/dev/null 2>&1; then
        printf '%s\n' "$1" | nc -U -q 1 "$mon_sock" >/dev/null 2>&1 && return 0
    fi
    python3 - "$mon_sock" "$1" <<'PY' 2>/dev/null && return 0
import socket, sys
s = socket.socket(socket.AF_UNIX); s.connect(sys.argv[1])
s.sendall((sys.argv[2] + "\n").encode()); s.close()
PY
    return 1
}

before=$(ls /dev/pts/ 2>/dev/null | grep -E '^[0-9]+$' | sort)

# Background watcher: detect new PTYs, open a minicom pane for each,
# wait until minicom has the slave end open, then resume the CPU.
(
    deadline=$((SECONDS + 15))
    while (( SECONDS < deadline )); do
        sleep 0.2
        after=$(ls /dev/pts/ 2>/dev/null | grep -E '^[0-9]+$' | sort)
        new_pts=$(diff <(echo "$before") <(echo "$after") \
                  | grep '^>' | sed 's/^> //')
        if [ -n "$new_pts" ]; then
            while IFS= read -r n; do
                # After minicom exits (for any reason), kill QEMU so the
                # main pane closes too. Use -f (full cmdline) because the
                # 24-char binary name is truncated to 15 in /proc and -x
                # would never match.
                tmux split-window -h \
                    "minicom -b 115200 -c on -o -D /dev/pts/$n; pkill -f qemu-system-riscv32 2>/dev/null || true"

                # Poll until minicom has opened the slave PTY (up to 5 s).
                for i in $(seq 1 50); do
                    find /proc/[0-9]*/fd -lname "/dev/pts/$n" 2>/dev/null \
                        | grep -q . && break
                    sleep 0.1
                done
                sleep 0.2   # cushion for minicom curses init
            done <<< "$new_pts"

            # All minicom instances attached - release the CPU.
            qemu_monitor_cmd c || true
            break
        fi
    done
) &
_watcher=$!

# Run QEMU started paused (-S). The extra monitor socket lets the watcher
# send the resume command without disturbing the existing mon:stdio channel.
"$@" -S -monitor "unix:$mon_sock,server,nowait"
rc=$?

kill "$_watcher" 2>/dev/null
wait "$_watcher" 2>/dev/null

pkill -x minicom 2>/dev/null || true

if [ $rc -ne 0 ]; then
    printf '\n[QEMU exited with rc=%d - press Enter to close]\n' "$rc"
    read -r _ </dev/tty 2>/dev/null || sleep 5
fi

exit $rc

#!/usr/bin/env python3
"""QEMU regression checks for the currently built milestone-0 image.

Inside the workshop container, run:
  make build CONFIG=milestone0
  python3 scripts/test-task-isolation.py exercise
  make build CONFIG=milestone0 SOLUTIONS=1
  python3 scripts/test-task-isolation.py solution

The exercise checks expect the unmodified starter; use solution checks for
a completed exercise or reference build. Each check starts a fresh QEMU
process. Logs go to build/validation/.
"""
import argparse
import os
from pathlib import Path
import select
import shlex
import subprocess
import time

ROOT = Path(__file__).resolve().parent.parent


def check_scenario(command, mode, name, steps):
    output = bytearray()
    process = subprocess.Popen(
        command, cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    os.set_blocking(process.stdout.fileno(), False)

    def wait_for(marker, after=0):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            ready, _, _ = select.select([process.stdout], [], [], 0.1)
            if ready:
                chunk = os.read(process.stdout.fileno(), 65536)
                if not chunk:
                    break
                output.extend(chunk)
            if marker in output[after:]:
                return
        raise AssertionError(
            f"{name}: missing {marker!r}\n{output.decode(errors='replace')}"
        )

    def send(data):
        process.stdin.write(data)
        process.stdin.flush()

    try:
        wait_for(b"[task_a] iter=1")
        wait_for(b"[task_b] iter=1")
        mark = len(output)
        send(b"h")
        wait_for(b"Milestone 0: Zephyr task isolation", mark)
        for key, expected in steps:
            mark = len(output)
            send(key)
            wait_for(expected, mark)
            if expected.startswith(b"[SPMP]"):
                # A denied access must abort only the offending task.
                mark = len(output)
                wait_for(b"[task_b] iter=", mark)
                send(b"s")
                wait_for(b"Restart QEMU before another test.", mark)
        assert b"[FAIL]" not in output, "Private-stack protection was bypassed"
        assert b"Unexpected fatal error" not in output
        assert b"Bao Hypervisor" not in output
        print(f"PASS {mode}: {name}")
    finally:
        try:
            send(b"\x01x")
            process.wait(timeout=3)
        except (BrokenPipeError, subprocess.TimeoutExpired):
            process.kill()
            process.wait()
        process.stdin.close()
        process.stdout.close()
        log_dir = ROOT / "build/validation"
        log_dir.mkdir(parents=True, exist_ok=True)
        (log_dir / f"{mode}-{name}.log").write_bytes(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("exercise", "solution"))
    mode = parser.parse_args().mode
    command = shlex.split(subprocess.check_output(
        ["make", "-s", "_print-qemu-cmd", "CONFIG=milestone0", "NO_DOCKER=1"],
        cwd=ROOT, text=True,
    ))
    private_fault = (b"f", b"[SPMP] Blocked private-stack access by task A.")
    check_scenario(command, mode, "private-stack-fault", [private_fault])
    if mode == "exercise":
        steps = [(b"s", b"[SPMP] Blocked unshared-buffer access by task A.")]
        name = "unshared-buffer-fault"
    else:
        steps = [
            (b"s", f"[task_b] received #{i}: hello from task A".encode())
            for i in range(1, 4)
        ] + [private_fault]
        name = "shared-message"
    check_scenario(command, mode, name, steps)


if __name__ == "__main__":
    main()

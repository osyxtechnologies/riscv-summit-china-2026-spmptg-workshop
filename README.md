# Zephyr, SPMP and Bao on QEMU

**RISC-V Summit China 2026 — SPMP workshop**
*Hosted by [OSYX Technologies](https://osyx.tech)*

Start by testing and configuring task isolation in Zephyr on SPMP-aware QEMU,
with OpenSBI providing M-mode firmware. Then move to Bao: boot a baremetal VM,
replace it with Zephyr, and add a second VM with shared-memory communication.
All stages run on your laptop using the workshop container.

## What you will build

```text
Milestone 0                     Milestone 3
+----------------------+        +----------------------+---------------------+
| Zephyr: two U tasks   |        | Zephyr: UART0 shell  | Baremetal: UART1    |
| SPMP task protection  |        |     shared memory + notification IRQ       |
+----------------------+        +----------------------+---------------------+
| OpenSBI              |        | Bao: isolated VMs, guest vSPMP             |
|                      |        +--------------------------------------------+
|                      |        | OpenSBI                                    |
+----------------------+        +--------------------------------------------+
| QEMU virt: RV32, SPMP, AIA (APLIC + IMSIC)                                  |
+----------------------------------------------------------------------------+
```

Zephyr uses SPMP for userspace protection. Under Bao, Zephyr uses virtual SPMP
(vSPMP), while Bao partitions VM memory and devices. In the final milestone,
Zephyr sends LED commands to baremetal; QEMU logs the LED actions on UART1.

## Workshop sequence

| Stage | Goal |
|-------|------|
| Milestone 0 | Trigger a task-isolation fault, then configure intentional sharing in Zephyr |
| Milestone 1 | Inspect a working Bao-hosted baremetal baseline |
| Milestone 2 | Configure a single Zephyr VM |
| Milestone 3 | Add baremetal as a second VM and connect the guests with IPC |
| Wrap-up | Review task protection, VM isolation, and communication |

## Before you arrive

Use a Linux laptop with Docker and a terminal large enough for two console
panes (at least 100 columns by 24 rows). Ubuntu 22.04 / 24.04 is recommended;
macOS and Windows have not been tested.

### 1. Clone this repository

```bash
git clone https://github.com/osyxtechnologies/riscv-summit-china-2026-spmptg-workshop.git
cd riscv-summit-china-2026-spmptg-workshop
```

### 2. Install Docker (Ubuntu / Debian)

If Docker is not already installed, run:

```bash
sudo apt-get update
sudo apt-get install -y ca-certificates curl
sudo install -m 0755 -d /etc/apt/keyrings
sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg \
    -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc
echo "deb [arch=$(dpkg --print-architecture) \
    signed-by=/etc/apt/keyrings/docker.asc] \
    https://download.docker.com/linux/ubuntu \
    $(. /etc/os-release && echo "$VERSION_CODENAME") stable" \
    | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt-get update
sudo apt-get install -y docker-ce docker-ce-cli containerd.io
```

Add your user to the `docker` group so you can run `docker` without `sudo`:

```bash
sudo usermod -aG docker $USER
newgrp docker
```

Verify the installation:

```bash
docker run --rm hello-world
```

You should see a `Hello from Docker!` message. If you do, Docker is ready.

> For Debian, replace `ubuntu` with `debian` in the repository URL above.
> Full instructions: https://docs.docker.com/engine/install/ubuntu/

### 3. Fetch the workshop image

Fetch the pre-built workshop Docker image from Docker Hub (fastest):

```bash
make pull-image
```

Alternatively, build it from source (≈ 15–20 min):

```bash
make build-image
```

---

## QEMU platform reference

The container supplies the pinned SPMP-aware `qemu-system-riscv32-spmp`.
Use this binary: a stock QEMU installation does not provide the workshop's
experimental SPMP extensions. The Makefile selects one RV32 hart, 4 GiB of
RAM, Sstc, SPMP and AIA with two guest interrupt files.

| Resource | Address / value |
|----------|-----------------|
| UART0 (Zephyr or milestone-1 baremetal) | `0x10000000`, APLIC source 10 |
| UART1 (milestone-3 baremetal) | `0x06000000`, APLIC source 12 |
| S-mode APLIC | `0x0d000000` |
| Physical S-mode IMSIC (milestone 0) | `0x28000000` |
| Zephyr guest IMSIC file | `0x28001000` |
| Timer frequency | 10 MHz |

| Memory region | Base | Use |
|---------------|------|-----|
| OpenSBI | `0x80000000` | All stages |
| Milestone 0 payload | `0x80200000` | Zephyr (explicit OpenSBI payload offset) |
| Bao payload | `0x80400000` | Bao (OpenSBI RV32 default offset) |
| Guest #0 | `0x84200000` | Baremetal (m1), Zephyr (m2/m3), 16 MiB |
| Guest #1 | `0x88200000` | Baremetal (m3), 16 MiB |
| Shared memory | `0x8a000000` | IPC (m3), 16 KiB |

## Step-by-step walkthrough

`PLATFORM=qemu` is the default and remains accepted explicitly. Host commands
automatically use the Docker image. Host `make run` builds first and opens
tmux; inside `make shell` or with `NO_DOCKER=1`, run `make build` before
`make run`. Exit QEMU with **Ctrl-A x** in the main console.

### 0. Milestone 0 — task isolation and intentional sharing in Zephyr

> *Goal: see SPMP reject a private-stack access, then configure a memory
> domain so two user tasks can exchange a message without sharing their stacks.*

1. Build and run the exercise:
   ```bash
   make build CONFIG=milestone0
   make run CONFIG=milestone0
   ```
   Zephyr runs directly above OpenSBI. Its kernel runs in S-mode and both
   tasks run in U-mode, with `CONFIG_USERSPACE=y` and `CONFIG_RISCV_SPMP=y`.
   Observe both `[task_a]` and `[task_b]` counters, then press `h` for help.
   Commands are single keys; no Enter is needed.
2. Press **`f`**. Task A tries to write to task B's private stack. Expect a
   Zephyr access-fault dump followed by:
   ```text
   [SPMP] Blocked private-stack access by task A. Stopping this task only.
   ```
   Task B's counter continues. The application handles this intentional fault
   by allowing Zephyr to abort the offending task; unexpected faults still
   halt the system. Exit QEMU with **Ctrl-A x** and start it again before
   the next test.
3. Press **`s`** to try sharing a message. The unfinished exercise has not
   granted access to the message partition, so task A faults:
   ```text
   [SPMP] Blocked unshared-buffer access by task A. Stopping this task only.
   ```
   This is the expected starting state, not a build failure. Exit QEMU.
4. Complete `exercises/qemu/milestone0/config.c`. The application already
   declares the message partition with `K_APPMEM_PARTITION_DEFINE` and places
   its buffer there with `K_APP_BMEM`. The domain is initialized but empty,
   and both tasks are created but not started. Add the partition with
   `k_mem_domain_add_partition()`, then add **both** tasks with
   `k_mem_domain_add_thread()`. Return any setup error.
   Keep the tasks in user mode and keep their private stacks out of the
   shared partition. Inspect `guests/zephyr/apps/dual-task/src/isolation.c`
   to follow the UART command, user-mode access, and fault handling.
5. Rebuild and run the exercise. Press **`s`** several times:
   ```text
   [task_a] Writing the shared message
   [task_b] received #1: hello from task A
   ```
   The sequence number increases with each message. The kernel queues and
   acknowledgement semaphore coordinate the exchange; the data itself lives
   in the participant-configured memory partition.
6. Press **`f`** again. The private-stack write must still fault while task B
   continues. Intentional sharing must not disable stack isolation.

Checkpoint: `s` delivers messages, `f` is blocked, and the unaffected task
continues. If stuck, use the reference configuration:

```bash
make build CONFIG=milestone0 SOLUTIONS=1
make run CONFIG=milestone0 SOLUTIONS=1
```

Next, move from protecting tasks inside one OS to isolating whole guests with Bao.

### 1. Milestone 1 — Bao with a baremetal guest

1. Build and run the baseline reference solution:
   ```bash
   make build CONFIG=milestone1 SOLUTIONS=1
   make run CONFIG=milestone1 SOLUTIONS=1
   ```
2. Observe Bao booting the baremetal VM on UART0 and its repeating
   `[heartbeat]` output.
3. Inspect `solutions/qemu/milestone1/config.c` and
   `guests/baremetal/src/platform/qemu-riscv32-virt-spmp/virt.c`.

Checkpoint: OpenSBI now boots Bao, which starts the single baremetal guest.

### 2. Milestone 2 — replace baremetal with Zephyr

1. Complete `exercises/qemu/milestone2/config.c`, using the address reference
   in the file. Describe one VM with its image, entry point, memory region,
   UART0 passthrough and AIA addresses.
2. Build and run:
   ```bash
   make build CONFIG=milestone2
   make run CONFIG=milestone2
   ```
3. Observe the periodic `dual-task` baseline inside a Bao VM:
   ```text
   Zephyr dual-task demo (board: baovm_qemu-virt-spmp/cv32a6)
   [task_a] iter=0 (user mode)
   [task_b] iter=0 (user mode)
   ```
   Zephyr runs in VS-mode and its tasks in VU-mode. Zephyr uses vSPMP for
   task protection; Bao controls the VM's memory and device access.
   Type a character to check UART input. The milestone-0 fault/sharing commands
   are specific to its exercise; this baseline prints received characters.

Checkpoint: one Zephyr guest runs on UART0. If stuck, append `SOLUTIONS=1`
to both commands to build the reference config.

### 3. Milestone 3 — add baremetal and inter-VM communication

1. Complete `exercises/qemu/milestone3/config.c`. Add:
   - A second VM for baremetal at `0x88200000`, with UART1 and AIA resources.
   - A shared-memory region at `0x8a000000`, size `0x4000`.
   - An IPC mapping in each VM, with notification IRQ source `3`.
2. The Zephyr `console` app and baremetal IPC handler are already provided.
   Build and run:
   ```bash
   make build CONFIG=milestone3
   make run CONFIG=milestone3
   ```
3. Observe the Zephyr shell on UART0 and baremetal on UART1. The QEMU PTY
   helper opens a second minicom pane automatically under tmux; click a pane
   to select it. Identify consoles by their output rather than pane position.
4. At the Zephyr `uart:~$` prompt, send:
   ```text
   baoipc write_notify 0 "led on"
   baoipc write_notify 0 "led off"
   baoipc write_notify 0 "led blink"
   baoipc write_notify 0 "hello from zephyr"
   ```
   `write_notify` copies the message into the channel's write half and
   notifies the peer. Baremetal logs LED actions on UART1; unknown messages
   appear as `[shmem rx] from zephyr: "..."`.

Checkpoint: both guests run, the Zephyr shell accepts commands, and baremetal
receives notifications and logs the requested actions. Use `SOLUTIONS=1` for
reference configurations. Exit with **Ctrl-A x** in the main QEMU console.

The former `CONFIG=standalone` stage is now `CONFIG=milestone0`. The former
Bao milestones 0–2 are now 1–3. If you have builds from that workflow, run
`make clean-all` once before starting; the numbering now selects different
applications and boards.

## Useful make targets

```text
make help                              Show targets and configuration values.
make pull-image                        Fetch the workshop image.
make build-image                       Build the image locally.
make push-image                        Publish the image (maintainers only).
make shell                             Open tmux inside the container.
make build CONFIG=<c>                  Compile the selected scenario.
make run CONFIG=<c>                    Boot QEMU; host dispatch builds first.
make clean CONFIG=<c>                  Remove this scenario's artifacts.
make clean-all                         Remove all build artifacts.
make build-zephyr CONFIG=<c>           Build Zephyr only.
make build-baremetal CONFIG=<c>         Build baremetal for milestone 1 or 3.
make build-bao CONFIG=<c>               Build Bao for milestone 1, 2 or 3.
make build-opensbi CONFIG=<c>           Build the payload and OpenSBI firmware.
```

Configurations: `milestone0`, `milestone1`, `milestone2`, `milestone3`.
`SOLUTIONS=1` selects the reference configuration for any milestone.
`NO_DOCKER=1` uses locally installed tools. Build outputs live under
`build/qemu/<config>/exercise/` or `build/qemu/<config>/solution/`, keeping
each milestone's and each variant's artifacts separate. `make clean CONFIG=<c>`
removes both variants.

## Troubleshooting

- **Unknown SPMP CPU property:** use the workshop image and its patched QEMU.
- **tmux pane does not open:** enlarge the terminal to at least 100×24 and
  use `make run` on the host or `make shell`. The PTY helper is
  `scripts/qemu-pty.sh`; `.tmux.conf` enables mouse selection.
- **Permission denied in `build/`:** container builds can create root-owned
  files. Remove the affected configuration with `make clean CONFIG=<c>`
  through Docker, or restore ownership to your host user.
- **Exercise fails to compile:** fill every TODO in the configuration, or
  build with `SOLUTIONS=1` to compare with the working reference.

## Background and further reading

- [Bao Hypervisor](https://github.com/bao-project/bao-hypervisor)
- RISC-V SPMP / SPMP-for-Hypervisor: experimental physical memory protection
  for supervisor software and virtualized guests on platforms without an MMU.
- RISC-V AIA: APLIC and IMSIC deliver interrupts to the kernel and guests.
- [Zephyr RTOS](https://www.zephyrproject.org)

## Credits

- **Sandro Pinto and Jose Martins** - OSYX Technologies/Bao project (workshop speaker)
- **Bruno Sá and Manuel Rodriguez** - OSYX/Bao project (workshop materials)

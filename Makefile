SHELL := /bin/bash

workshop_root := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

CROSS_COMPILE   ?= riscv32-unknown-elf-
WORKSHOP_IMAGE  ?= osyxtechnologies/riscv-summit-workshop:latest

PLATFORM       ?= qemu

ifneq ($(PLATFORM),qemu)
$(error PLATFORM=$(PLATFORM) is not valid. Supported value: qemu)
endif

.DEFAULT_GOAL := help

# ---------------------------------------------------------------------------
# Container auto-dispatch.
#
# When the make command is invoked on the host, re-run the requested goal
# inside the workshop Docker image so users do not need to `make shell`
# first. Set NO_DOCKER=1 to skip the dispatch (e.g. when the host already
# has the toolchain installed, or for Makefile debugging).
#
# Detection: /.dockerenv exists in every Docker container's root filesystem.
# ---------------------------------------------------------------------------
_host_only_goals  := help build-image pull-image push-image shell
_dispatched_goals := $(filter-out $(_host_only_goals),$(or $(MAKECMDGOALS),help))

ifeq ($(wildcard /.dockerenv),)
ifndef NO_DOCKER
ifneq ($(_dispatched_goals),)
_in_dispatch := 1

# Forward command-line overrides into the in-container make invocation.
_fwd_vars := PLATFORM CONFIG SOLUTIONS CROSS_COMPILE WORKSHOP_IMAGE
_fwd_args := $(foreach v,$(_fwd_vars),$(if $($(v)),$(v)=$($(v))))

.PHONY: $(_dispatched_goals) build

_image_check = \
	docker image inspect $(WORKSHOP_IMAGE) >/dev/null 2>&1 || { \
		echo "workshop image '$(WORKSHOP_IMAGE)' not found."; \
		echo "Run 'make pull-image' to fetch it from Docker Hub, or 'make build-image' to build locally."; \
		exit 1; \
	}

_docker_run = docker run --rm -it \
	-v $(workshop_root):$(workshop_root):rw \
	-w $(workshop_root) \
	$(WORKSHOP_IMAGE)

# Non-run goals: build, clean, build-* -- plain in-container make.
_non_run_goals := $(sort $(filter-out run,$(_dispatched_goals)) $(if $(filter run,$(_dispatched_goals)),build))

ifneq ($(_non_run_goals),)
$(_non_run_goals):
	@$(_image_check)
	$(_docker_run) bash -c '\
		git config --global --add safe.directory "*" && \
		make $@ NO_DOCKER=1 $(_fwd_args)'
endif

_launch_error := $(workshop_root)/.launch-error

# Minimum terminal size for the side-by-side console layout. tmux refuses
# to split (and may fail to start) in a window smaller than this, which
# otherwise produces an instant, silent exit.
_run_min_cols  := 100
_run_min_lines := 24

ifneq ($(filter run,$(MAKECMDGOALS)),)
run: | build
	@$(_image_check)
	@rm -f $(_launch_error)
	$(_docker_run) bash -c '\
		git config --global --add safe.directory "*" && \
		size="$$(stty size 2>/dev/null)"; \
		if [ -n "$$size" ]; then \
			set -- $$size; rows=$$1; cols=$$2; \
			if [ "$$cols" -lt $(_run_min_cols) ] || [ "$$rows" -lt $(_run_min_lines) ]; then \
				printf "\nTerminal window is too small (%sx%s).\nThe workshop opens side-by-side console panes and needs at least %sx%s.\nMaximize or enlarge this terminal window and run the command again.\n\n" \
					"$$cols" "$$rows" "$(_run_min_cols)" "$(_run_min_lines)" >&2; \
				exit 1; \
			fi; \
		fi; \
		tmux -f .tmux.conf new-session "make run NO_DOCKER=1 $(_fwd_args)" ; \
		if [ -s "$(_launch_error)" ]; then \
			echo ""; cat "$(_launch_error)"; \
			rm -f "$(_launch_error)"; \
		fi'
endif

endif # _dispatched_goals non-empty
endif # NO_DOCKER
endif # not in container

ifndef _in_dispatch
# ===========================================================================
# Inside-container (or NO_DOCKER=1) path: the real recipes.
# ===========================================================================

# Targets that work without PLATFORM / CONFIG.
_standalone_goals := help build-image pull-image push-image shell clean-all

# Enable full build-mode when at least one non-standalone goal is requested.
ifneq ($(filter-out $(_standalone_goals), $(or $(MAKECMDGOALS),help)),)
_build_mode := 1
endif

# ---------------------------------------------------------------------------
# Everything below here requires CONFIG.
# ---------------------------------------------------------------------------
ifdef _build_mode

ifndef CONFIG
$(error CONFIG is not set. Use CONFIG=milestone0, milestone1, milestone2 or milestone3)
endif

ifeq ($(filter milestone0 milestone1 milestone2 milestone3,$(CONFIG)),)
$(error CONFIG=$(CONFIG) is not valid. Supported values: milestone0, milestone1, milestone2, milestone3)
endif

config_dir := $(workshop_root)/$(if $(SOLUTIONS),solutions,exercises)/$(PLATFORM)
config := $(config_dir)/$(CONFIG)

ifeq ($(wildcard $(config)),)
$(error CONFIG=$(CONFIG): no such config directory $(config))
endif

# Source roots
bao_dir     := $(workshop_root)/bao-hypervisor
opensbi_dir := $(workshop_root)/opensbi
zephyr_dir  := $(workshop_root)/guests/zephyr
zephyr_src  := $(zephyr_dir)/zephyr

# Per-config build paths
build_dir     := $(workshop_root)/build/$(PLATFORM)/$(CONFIG)
zephyr_build  := $(build_dir)/zephyr
bao_bin       := $(build_dir)/bao/bao.bin
opensbi_build := $(build_dir)/opensbi

# ---------------------------------------------------------------------------
# Scenario settings -- standalone Zephyr or Bao milestones.
# ---------------------------------------------------------------------------

ifeq ($(CONFIG),milestone0)

ZEPHYR_APP     ?= $(workshop_root)/guests/zephyr/apps/dual-task
ZEPHYR_BOARD   ?= qemu-virt-spmp
WORKSHOP_CONFIG := $(config)/config.c
ZEPHYR_OVERLAY ?=

else ifeq ($(CONFIG),milestone1)

BAREMETAL_APP  := $(workshop_root)/guests/baremetal
baremetal_bin  := $(build_dir)/baremetal/baremetal.bin
# Place the baremetal guest where the Zephyr image normally lives so the same
# single-VM physical layout works across milestones. UART0 is set below.
PLAT_MEM_BASE  := 0x84200000
_qemu_extra_serials :=
_qemu_guest_loaders  = \
    -device loader,file=$(baremetal_bin),addr=0x84200000

else ifeq ($(CONFIG),milestone2)

ZEPHYR_APP     ?= $(workshop_root)/guests/zephyr/apps/dual-task
ZEPHYR_OVERLAY ?=
_qemu_extra_serials :=
_qemu_guest_loaders  = \
    -device loader,file=$(zephyr_build)/zephyr/zephyr.bin,addr=0x84200000

else ifeq ($(CONFIG),milestone3)

ZEPHYR_APP     ?= $(workshop_root)/guests/zephyr/apps/console
ZEPHYR_OVERLAY ?= $(ZEPHYR_APP)/app.overlay
BAREMETAL_APP  := $(workshop_root)/guests/baremetal
baremetal_bin  := $(build_dir)/baremetal/baremetal.bin
SHMEM_BASE     := 0x8A000000
SHMEM_SIZE     := 0x00004000
_qemu_extra_serials := -serial pty
_qemu_guest_loaders  = \
    -device loader,file=$(zephyr_build)/zephyr/zephyr.bin,addr=0x84200000 \
    -device loader,file=$(baremetal_bin),addr=0x88200000

endif

# ---------------------------------------------------------------------------
# Platform settings -- toolchain, board IDs, QEMU command.
# ---------------------------------------------------------------------------

BAO_PLATFORM  ?= qemu-riscv32-virt-spmp
ZEPHYR_BOARD  ?= baovm_qemu-virt-spmp
OPENSBI_PLAT  ?= generic
QEMU          ?= qemu-system-riscv32-spmp

# Milestone 1 wires the baremetal guest to UART0 (instead of UART1) on QEMU.
ifeq ($(CONFIG),milestone1)
PLAT_UART_ADDR := 0x10000000
UART_IRQ_ID    := 10
endif

QEMU_CPU    := rv32,priv_spec=v1.13.0,sstc=true,spmp=true,sspmpsw=true,sshspmp=true,ssvspmp=true,sshspmpsw=true
# Bare QEMU argv wrapped by scripts/qemu-pty.sh for tmux PTY handling.
QEMU_CMD     = $(QEMU) \
               -M virt,aia=aplic-imsic,aia-guests=2 \
               -cpu $(QEMU_CPU) \
               -smp 1 -m 4G -nographic \
               -bios $(opensbi_payload) \
               -serial mon:stdio $(_qemu_extra_serials) \
               $(_qemu_guest_loaders)
# qemu-pty.sh splits a tmux pane per QEMU-created PTY.
QEMU_INVOKE  = $(workshop_root)/scripts/qemu-pty.sh $(QEMU_CMD)

opensbi_payload := $(opensbi_build)/platform/$(OPENSBI_PLAT)/firmware/fw_payload.bin
ifeq ($(CONFIG),milestone0)
firmware_payload := $(zephyr_build)/zephyr/zephyr.bin
firmware_dependency := build-zephyr
else
firmware_payload := $(bao_bin)
firmware_dependency := build-bao
endif

bao_cfg_repo  := $(config_dir)
zephyr_prefix := $(shell which $(CROSS_COMPILE)gcc | sed 's/.\{3\}$$//')
zephyr_env    := \
	ZEPHYR_TOOLCHAIN_VARIANT=cross-compile \
	CROSS_COMPILE=$(zephyr_prefix)

endif # _build_mode

# ---------------------------------------------------------------------------

NPROC := $(shell nproc)

.PHONY: help build run clean clean-all \
        build-zephyr build-bao build-opensbi build-baremetal \
        build-image pull-image push-image shell

help:
	@echo "Workshop targets (CONFIG= required; PLATFORM=qemu is the default):"
	@echo "  make build CONFIG=<c>   Build the scenario and its OpenSBI firmware."
	@echo "  make run CONFIG=<c>     Boot QEMU (host dispatch builds first)."
	@echo "  make clean CONFIG=<c>   Remove this scenario's build artifacts."
	@echo "  make clean-all          Remove all build artifacts."
	@echo ""
	@echo "Component targets (CONFIG= required):"
	@echo "  make build-zephyr / build-bao / build-opensbi / build-baremetal"
	@echo "  milestone0: Zephyr task isolation; milestones 1-3: Bao + guests."
	@echo ""
	@echo "Utility targets: pull-image, build-image, push-image, shell"
	@echo "CONFIG values: milestone0 milestone1 milestone2 milestone3"
	@echo "SOLUTIONS=1   Use reference configs instead of exercises."
	@echo "NO_DOCKER=1   Use the host toolchain instead of the container."
	@echo ""
	@echo "Examples:"
	@echo "  make pull-image"
	@echo "  make build CONFIG=milestone0"
	@echo "  make run CONFIG=milestone0"
	@echo "  make run CONFIG=milestone3 SOLUTIONS=1"

# Build the selected applications and firmware (Bao only for milestones 1-3).
build: $(if $(ZEPHYR_APP),build-zephyr) $(if $(BAREMETAL_APP),build-baremetal) build-opensbi

# `make run` boots on the selected platform; it does NOT build first --
# build and run are independent steps. Run `make build` separately when
# guest sources change.
ifdef _build_mode
run:
	$(QEMU_INVOKE)
endif

build-zephyr:
	$(zephyr_env) cmake -GNinja \
		-DCMAKE_PREFIX_PATH=$(zephyr_src)/share/zephyr-package \
		-B$(zephyr_build) \
		-DBOARD_ROOT=$(zephyr_dir) \
		-DBOARD=$(ZEPHYR_BOARD) \
		$(if $(strip $(ZEPHYR_OVERLAY)),-DDTC_OVERLAY_FILE=$(ZEPHYR_OVERLAY)) \
		-DWORKSHOP_CONFIG=$(WORKSHOP_CONFIG) \
		-DZEPHYR_MODULES=$(workshop_root) \
		$(ZEPHYR_APP)
	$(zephyr_env) ninja -C $(zephyr_build)

build-bao:
ifeq ($(CONFIG),milestone0)
	$(error build-bao is not applicable to CONFIG=milestone0)
else
	$(MAKE) -C $(bao_dir) \
		PLATFORM=$(BAO_PLATFORM) \
		CONFIG_REPO=$(bao_cfg_repo) \
		CONFIG=$(CONFIG) \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		-j$(NPROC)
	mkdir -p $(dir $(bao_bin))
	cp $(bao_dir)/bin/$(BAO_PLATFORM)/$(CONFIG)/bao.bin $(bao_bin)

endif

build-opensbi: $(firmware_dependency)
	$(MAKE) -C $(opensbi_dir) \
		O=$(opensbi_build) \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		PLATFORM=$(OPENSBI_PLAT) \
		PLATFORM_RISCV_XLEN=32 \
		FW_PAYLOAD=y \
		FW_PAYLOAD_PATH=$(firmware_payload) \
		$(if $(filter milestone0,$(CONFIG)),FW_PAYLOAD_OFFSET=0x200000) \
		-j$(NPROC)

build-baremetal:
ifeq ($(CONFIG),milestone0)
	$(error build-baremetal is not applicable to CONFIG=milestone0)
else
	$(MAKE) -C $(BAREMETAL_APP) \
		PLATFORM=$(BAO_PLATFORM) \
		BUILD_DIR=$(build_dir)/baremetal \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		$(if $(SHMEM_BASE),SHMEM_BASE=$(SHMEM_BASE) SHMEM_SIZE=$(SHMEM_SIZE)) \
		$(if $(PLAT_MEM_BASE),PLAT_MEM_BASE=$(PLAT_MEM_BASE)) \
		$(if $(PLAT_UART_ADDR),PLAT_UART_ADDR=$(PLAT_UART_ADDR)) \
		$(if $(UART_IRQ_ID),UART_IRQ_ID=$(UART_IRQ_ID)) \
		-j$(NPROC)

endif

# Helper used by the host-side terminal dispatch to obtain the fully
# expanded QEMU argv (depends on PLATFORM, CONFIG, and several scenario
# variables that live inside the _build_mode block above).
.PHONY: _print-qemu-cmd
_print-qemu-cmd:
	@printf '%s' "$(QEMU_CMD)"

clean:
ifneq ($(CONFIG),milestone0)
	-$(MAKE) -C $(bao_dir) clean \
		PLATFORM=$(BAO_PLATFORM) \
		CONFIG_REPO=$(bao_cfg_repo) \
		CONFIG=$(CONFIG)
endif
	rm -rf $(build_dir)

clean-all:
	-rm -rf $(workshop_root)/bao-hypervisor/bin
	-rm -rf $(workshop_root)/bao-hypervisor/build
	-rm -rf $(workshop_root)/opensbi/build
	-rm -rf $(workshop_root)/guests/baremetal/build
	rm -rf $(workshop_root)/build

build-image:
	docker build -t $(WORKSHOP_IMAGE) $(workshop_root)

pull-image:
	docker pull $(WORKSHOP_IMAGE)

push-image:
	docker push $(WORKSHOP_IMAGE)

# Interactive shell in the workshop image. The host's workshop tree is
# bind-mounted in at the same path so build artefacts under build/ are
# usable from either the host or another shell. Run `make pull-image` first if
# the image doesn't exist yet.
shell:
	@docker image inspect $(WORKSHOP_IMAGE) >/dev/null 2>&1 || { \
		echo "workshop image '$(WORKSHOP_IMAGE)' not found."; \
		echo "Run 'make pull-image' to fetch it from Docker Hub, or 'make build-image' to build locally."; \
		exit 1; \
	}
	-docker run --rm -it \
		-v $(workshop_root):$(workshop_root):rw \
		-w $(workshop_root) \
		$(WORKSHOP_IMAGE) bash -c '\
			git config --global --add safe.directory "*" && \
			exec tmux -f .tmux.conf new-session -s workshop'

endif # _in_dispatch

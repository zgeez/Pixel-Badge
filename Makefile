# Developer commands for Linux, WSL, and CI.
#
# Requires the nRF Connect SDK v3.4.1 toolchain on PATH and ZEPHYR_BASE set to
# the SDK's zephyr directory. On Windows, use scripts/dev.ps1 instead.

BOARD         ?= nrf5340dk/nrf5340/cpuapp
BUILD_DIR     ?= build/nrf5340dk
TEST_PLATFORM ?= native_sim

export BOARD BUILD_DIR TEST_PLATFORM

.PHONY: help build test format check clean

help:
	@echo "make build   - build firmware for $(BOARD)"
	@echo "make test    - run Twister host tests on $(TEST_PLATFORM)"
	@echo "make format  - format C/C++ sources in place"
	@echo "make check   - check formatting, lint YAML, check repository hygiene"
	@echo "make clean   - remove build/"

build:
	bash scripts/ci/build-firmware.sh

test:
	bash scripts/ci/run-tests.sh

format:
	bash scripts/ci/check-format.sh --fix

check:
	bash scripts/ci/check-format.sh
	yamllint --strict .
	python3 scripts/ci/check_hygiene.py

clean:
	rm -rf build

CC ?= gcc
PYTHON ?= python3

.PHONY: help test test-c test-python check-shell check clean

help:
	@echo "make test   - run native C and Python tests"
	@echo "make check  - check shell syntax, then run all tests"
	@echo "make clean  - remove native test outputs"
	@echo "Cross-target builds: sh scripts/build_all.sh imx6ull|t113|all"

test: test-c test-python

test-c:
	$(MAKE) -C tests test CC="$(CC)"

test-python:
	$(PYTHON) -m py_compile tests/tcp_fault_injector.py
	$(PYTHON) -m py_compile tools/package_stm32_ota.py
	$(PYTHON) -m py_compile tools/package_stm32_ota_ab.py
	$(PYTHON) -m py_compile tools/generate_ota_signing_key.py
	$(PYTHON) -m py_compile tools/make_ota_negative_test.py
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py'

check-shell:
	find . -type f \( -name '*.sh' -o -name 'S90iot-*' \) -print0 | \
		xargs -0 -n1 sh -n

check: check-shell test

clean:
	$(MAKE) -C tests clean
	$(MAKE) -C linux/kernel_drivers/imx6ull_sensors clean-user

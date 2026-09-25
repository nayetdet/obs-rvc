.PHONY: build install install-hooks

build:
	$(MAKE) -C plugin build

install:
	$(MAKE) -C plugin install
	$(MAKE) -C plugin-worker install

install-hooks:
	git config core.hooksPath .githooks

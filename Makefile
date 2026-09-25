.PHONY: build clean install install-hooks

build:
	$(MAKE) -C plugin build

install:
	$(MAKE) -C plugin install

clean:
	$(MAKE) -C plugin clean

install-hooks:
	git config core.hooksPath .githooks

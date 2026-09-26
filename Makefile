.PHONY: build clean install check install-hooks

build:
	$(MAKE) -C plugin build

install:
	$(MAKE) -C plugin install

clean:
	$(MAKE) -C plugin clean

check:
	$(MAKE) -C plugin check

install-hooks:
	git config core.hooksPath .githooks

BUILD_CONFIG := RelWithDebInfo
RELEASE_DIR  := release/$(BUILD_CONFIG)

ifeq ($(OS),Windows_NT)
  BUILD_PRESET       := windows-x64
  BUILD_DIR          := build_x64
  OBS_INSTALL_PREFIX ?= $(ALLUSERSPROFILE)/obs-studio/plugins
else ifeq ($(shell uname -s),Darwin)
  BUILD_PRESET       := macos
  BUILD_DIR          := build_macos
  OBS_INSTALL_PREFIX ?= $(HOME)/Library/Application Support/obs-studio/plugins
else
  BUILD_PRESET       := ubuntu-x86_64
  BUILD_DIR          := build_x86_64
  OBS_INSTALL_PREFIX ?= /usr
endif

.PHONY: build install clean

build:
	cmake --preset $(BUILD_PRESET)
	cmake --build --preset $(BUILD_PRESET)
	cmake --install $(BUILD_DIR) \
		--config $(BUILD_CONFIG) \
		--prefix $(RELEASE_DIR)

ifeq ($(OS),Windows_NT)
install: clean
	$(MAKE) build
	cmake --install $(BUILD_DIR) --config $(BUILD_CONFIG)
else ifeq ($(shell uname -s),Darwin)
install: clean
	$(MAKE) build
	sudo cmake --install $(BUILD_DIR) --config $(BUILD_CONFIG)
else
install: clean
	$(MAKE) build
	sudo cmake --install $(BUILD_DIR) --config $(BUILD_CONFIG) --prefix $(OBS_INSTALL_PREFIX)
endif

ifeq ($(OS),Windows_NT)
clean:
	powershell -NoProfile -Command 'if (Test-Path "$(BUILD_DIR)\install_manifest.txt") { Get-Content -LiteralPath "$(BUILD_DIR)\install_manifest.txt" | ForEach-Object { Remove-Item -LiteralPath $$_ -Force -ErrorAction SilentlyContinue } }'
else ifeq ($(shell uname -s),Darwin)
clean:
	@if [ -f "$(BUILD_DIR)/install_manifest.txt" ]; then \
		sudo sh -c 'while IFS= read -r path; do rm -f -- "$$path"; done < "$$1"' sh "$(BUILD_DIR)/install_manifest.txt"; \
	fi
else
clean:
	@if [ -f "$(BUILD_DIR)/install_manifest.txt" ]; then \
		sudo sh -c 'while IFS= read -r path; do rm -f -- "$$path"; done < "$$1"' sh "$(BUILD_DIR)/install_manifest.txt"; \
	fi
endif

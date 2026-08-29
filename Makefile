BUILD_CONFIG := RelWithDebInfo
RELEASE_DIR  := release/$(BUILD_CONFIG)

ifeq ($(OS),Windows_NT)
  BUILD_PRESET := windows-x64
  BUILD_DIR    := build_x64
else ifeq ($(shell uname -s),Darwin)
  BUILD_PRESET := macos
  BUILD_DIR    := build_macos
else
  BUILD_PRESET := ubuntu-x86_64
  BUILD_DIR    := build_x86_64
endif

.PHONY: build

build:
	cmake --preset $(BUILD_PRESET)
	cmake --build --preset $(BUILD_PRESET)
	cmake --install $(BUILD_DIR) \
		--config $(BUILD_CONFIG) \
		--prefix $(RELEASE_DIR)

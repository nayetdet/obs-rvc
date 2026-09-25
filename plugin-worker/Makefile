.PHONY: install upgrade run

install:
	uv sync

upgrade:
	uv sync --upgrade

run:
	uv run python -m plugin_worker

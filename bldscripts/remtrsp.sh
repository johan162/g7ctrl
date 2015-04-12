#!/bin/sh
find . -type f -name '*.c' -exec sed --in-place 's/[[:space:]]\+$//' {} \+
find . -type f -name '*.h' -exec sed --in-place 's/[[:space:]]\+$//' {} \+

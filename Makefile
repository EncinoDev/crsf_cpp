BUILD_DIR = build/debug
CLANG_FORMAT_STYLE = --style=file:.clang-format
CMAKE_FORMAT_CONFIG = -c .cmake-format.json

.PHONY: build test format format-check lint quality

build:
	cmake --preset debug
	cmake --build --preset debug

test: build
	-ctest --preset debug

format:
	@find include tests \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -print | xargs -r clang-format -i $(CLANG_FORMAT_STYLE)
	@find . -name "CMakeLists.txt" -not -path "./build/*" -print | xargs -r cmake-format -i $(CMAKE_FORMAT_CONFIG)

# What CI runs: reports drift instead of rewriting the tree.
format-check:
	@find include tests \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -print | xargs -r clang-format --dry-run -Werror $(CLANG_FORMAT_STYLE)

lint: build
	@find tests -name "*.cpp" -print | xargs -r clang-tidy -p $(BUILD_DIR)

quality: format build test lint

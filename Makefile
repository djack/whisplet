BUILD_DIR := build
MODEL     ?= small.en

.PHONY: all build model run check clean

all: build

build:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cmake --build $(BUILD_DIR) -j$(shell nproc 2>/dev/null || sysctl -n hw.logicalcpu)

model:
	./scripts/get-model.sh $(MODEL)

run: build
	./$(BUILD_DIR)/whisplet models/ggml-$(MODEL).bin

check: build
	clang-format -i main.cpp
	clang-tidy main.cpp -p $(BUILD_DIR)/compile_commands.json \
		--extra-arg=-std=gnu++17 \
		--extra-arg=-isystem/usr/include/c++/11 \
		--extra-arg=-isystem/usr/include/x86_64-linux-gnu/c++/11
	cppcheck --enable=all --error-exitcode=1 \
		--suppress=missingIncludeSystem \
		--suppress=missingInclude \
		--suppress=unmatchedSuppression \
		main.cpp

clean:
	rm -rf $(BUILD_DIR)

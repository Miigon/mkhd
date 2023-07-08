FRAMEWORKS     = -framework Cocoa -framework Carbon -framework CoreServices
BUILD_PATH     = ./bin
BUILD_FLAGS    = -std=c99 -Wall -g -O0
MKHD_SRC       = ./src/mkhd.c
BINS           = $(BUILD_PATH)/mkhd

.PHONY: all clean install

all: clean $(BINS)

install: BUILD_FLAGS=-std=c99 -Wall -O2
install: clean $(BINS)

clean:
	rm -rf $(BUILD_PATH)

$(BUILD_PATH)/mkhd: $(MKHD_SRC)
	mkdir -p $(BUILD_PATH)
	clang $^ $(BUILD_FLAGS) $(FRAMEWORKS) -o $@

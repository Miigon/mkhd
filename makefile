SRC_PATH       ?= ./src
BUILD_PATH     ?= ./build
OBJ_PATH       = $(BUILD_PATH)/objs

SRC            = $(wildcard $(SRC_PATH)/*.c)
DEPS           = $(wildcard $(SRC_PATH)/*.h)
OBJS           = $(patsubst %.c,$(OBJ_PATH)/%.o,$(SRC))
BINS           = $(BUILD_PATH)/mkhd

DEBUG_FLAGS ?= -g -O0
CFLAGS = -std=c99 -Wall $(DEBUG_FLAGS)
LDFLAGS = -framework Cocoa -framework Carbon -framework CoreServices

.PHONY: all clean release

all: $(BINS)

release: DEBUG_FLAGS=-O2
release: clean $(BINS)

clean:
	rm -f $(BINS) $(OBJS)

$(BINS): $(OBJS) $(DEPS)
	mkdir -p $(BUILD_PATH)
	clang $(OBJS) $(CFLAGS) $(LDFLAGS) -o $@

$(OBJ_PATH)/%.o: %.c
	@mkdir -p $(@D)
	clang -c $^ $(CFLAGS) -o $@

%.o: %.c
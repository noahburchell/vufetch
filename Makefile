# vufetch — Linux system fetch, C23.

TARGET   := vufetch
SRCDIR   := src
BUILDDIR := build
BIN      := $(BUILDDIR)/$(TARGET)

PREFIX  ?= /usr/local
DESTDIR ?=

CC ?= cc

SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
DEPS := $(OBJS:.o=.d)

WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes \
            -Wmissing-prototypes -Wwrite-strings -Wformat=2 -Wvla \
            -Wcast-qual -Wundef -Wdouble-promotion
SANFLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer

# _GNU_SOURCE is required: -std=c23 is strict ISO, which would otherwise hide
# gethostname(), strtok_r(), getline(), CLOCK_BOOTTIME and major()/minor().
CPPFLAGS += -std=c23 -D_GNU_SOURCE -MMD -MP
CFLAGS   ?= -O2 -g
CFLAGS   += $(WARNINGS)

.PHONY: all
all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	@mkdir -p $@

# Cleans first because sanitised and unsanitised objects cannot be mixed.
#
# Needs the sanitizer runtimes (libasan/libubsan for gcc, compiler-rt for
# clang), which are NOT installed here: linking fails with "cannot find -lasan".
# On Gentoo that means rebuilding gcc with USE=sanitize; until then
# `make debug SANFLAGS=` gives a plain -O0 -g3 unsanitised build.
.PHONY: debug
debug:
	@$(MAKE) clean
	@$(MAKE) CFLAGS="-O0 -g3 $(WARNINGS) $(SANFLAGS)" LDFLAGS="$(SANFLAGS)" all

.PHONY: release
release:
	@$(MAKE) clean
	@$(MAKE) CFLAGS="-O2 -DNDEBUG $(WARNINGS)" LDFLAGS="-s" all

.PHONY: run
run: $(BIN)
	@./$(BIN)

.PHONY: install
install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

.PHONY: uninstall
uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

.PHONY: clean
clean:
	$(RM) -r $(BUILDDIR)

# Leading - because no .d files exist on the first build; -MP (above) emits
# phony header targets so deleting a header is not a hard error either.
-include $(DEPS)

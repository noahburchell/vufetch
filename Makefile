# vufetch — very useful fetch, C23.

TARGET   := vufetch
SRCDIR   := src
BUILDDIR := build
BIN      := $(BUILDDIR)/$(TARGET)

DESTDIR ?=
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin

INSTALL ?= install

SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
DEPS := $(OBJS:.o=.d)

# Applied in the compile recipe rather than folded into CFLAGS, so a package
# manager replacing CFLAGS wholesale does not drop the warning set.
WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes \
            -Wmissing-prototypes -Wwrite-strings -Wformat=2 -Wvla \
            -Wcast-qual -Wundef -Wdouble-promotion

CPPFLAGS += -std=c23 -D_GNU_SOURCE -MMD -MP
CFLAGS   ?= -O2 -g

.DELETE_ON_ERROR:

.PHONY: all
all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -c $< -o $@

$(BUILDDIR):
	@mkdir -p $@

.PHONY: debug
debug:
	@$(MAKE) clean
	@$(MAKE) CFLAGS="-O0 -g3" all

.PHONY: release
release:
	@$(MAKE) clean
	@$(MAKE) CFLAGS="-O2 -DNDEBUG" all

.PHONY: run
run: $(BIN)
	@./$(BIN)

.PHONY: install
install: $(BIN)
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 $(BIN) $(DESTDIR)$(BINDIR)/$(TARGET)

.PHONY: uninstall
uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/$(TARGET)

.PHONY: clean
clean:
	$(RM) -r $(BUILDDIR)

-include $(DEPS)

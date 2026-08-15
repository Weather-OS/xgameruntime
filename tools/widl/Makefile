PROGRAM := widl
BINDIR  ?= bin
TARGET  := $(BINDIR)/$(PROGRAM)

CC       ?= cc
CFLAGS   ?= -O2 -Wall
CPPFLAGS ?=
LDFLAGS  ?=
LDLIBS   ?=

srcdir   ?= .
builddir ?= build

prefix          ?= /usr/local
exec_prefix     ?= $(prefix)
bindir          ?= $(exec_prefix)/bin
includedir      ?= $(prefix)/include
libdir          ?= $(exec_prefix)/lib
datadir         ?= $(prefix)/share
system_dllpath  ?= $(libdir)

PACKAGE_VERSION ?= 1.0.0

CPPFLAGS += -I$(srcdir) -I$(srcdir)/wrc
CPPFLAGS += -I$(builddir) -I$(builddir)/wrc
CPPFLAGS += \
	-DINCLUDEDIR=\"$(includedir)\" \
	-DBINDIR=\"$(bindir)\" \
	-DLIBDIR=\"$(libdir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DSYSTEMDLLPATH=\"$(system_dllpath)\" \
	-DPACKAGE_VERSION=\"$(PACKAGE_VERSION)\"

FLEX  ?= flex
BISON ?= bison


WIDL_SOURCES := \
	attribute.c \
	client.c \
	expr.c \
	hash.c \
	header.c \
	metadata.c \
	proxy.c \
	register.c \
	server.c \
	typegen.c \
	typelib.c \
	typetree.c \
	utils.c \
	widl.c \
	write_msft.c \
	write_sltg.c

WRC_SOURCES := \
	wrc/genres.c \
	wrc/newstruc.c \
	wrc/po.c \
	wrc/utils.c \
	wrc/wpp.c \
	wrc/wrc.c

WIDL_OBJECTS := \
	$(WIDL_SOURCES:.c=.o) \
	parser.tab.o \
	parser.yy.o

WRC_OBJECTS := \
	$(WRC_SOURCES:.c=.o) \
	wrc/ppy.tab.o \
	wrc/ppl.yy.o

# All objects are built under BINDIR.
OBJECTS := $(addprefix $(BINDIR)/,$(WIDL_OBJECTS) $(WRC_OBJECTS))


# ----------------------------------------------------------------------
# Parser generation
# ----------------------------------------------------------------------

parser.tab.c parser.tab.h: parser.y
	$(BISON) -d -o parser.tab.c $<

parser.yy.c: parser.l parser.tab.h
	$(FLEX) -o $@ $<

wrc/parser.tab.c wrc/parser.tab.h: wrc/parser.y
	$(BISON) -d -o wrc/parser.tab.c $<

wrc/ppy.tab.c wrc/ppy.tab.h: wrc/ppy.y
	$(BISON) -d -o wrc/ppy.tab.c $<

wrc/parser.yy.c: wrc/parser.l wrc/parser.tab.h
	$(FLEX) -o $@ $<

wrc/ppl.yy.c: wrc/ppl.l wrc/ppy.tab.h
	$(FLEX) -o $@ $<

# WIDL sources use the WIDL parser header.
$(addprefix $(BINDIR)/,$(WIDL_SOURCES:.c=.o)): parser.tab.h

# WRC sources use the WRC parser header.
$(addprefix $(BINDIR)/wrc/,$(notdir $(WRC_SOURCES:.c=.o))): wrc/parser.tab.h

$(BINDIR)/wrc/wpp.o \
$(BINDIR)/wrc/ppl.yy.o: wrc/ppy.tab.h

$(BINDIR)/parser.tab.o: parser.tab.c parser.tab.h
$(BINDIR)/parser.yy.o: parser.yy.c parser.tab.h

$(BINDIR)/wrc/parser.tab.o: wrc/parser.tab.c wrc/parser.tab.h
$(BINDIR)/wrc/parser.yy.o: wrc/parser.yy.c wrc/parser.tab.h

$(BINDIR)/wrc/ppy.tab.o: wrc/ppy.tab.c wrc/ppy.tab.h
$(BINDIR)/wrc/ppl.yy.o: wrc/ppl.yy.c wrc/ppy.tab.h

$(BINDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<


# ----------------------------------------------------------------------
# Commands
# ----------------------------------------------------------------------

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	rm -f \
    	parser.tab.c \
    	parser.tab.h \
    	parser.yy.c \
    	wrc/parser.tab.c \
    	wrc/parser.tab.h \
    	wrc/ppy.tab.c \
    	wrc/ppy.tab.h \
    	wrc/parser.yy.c \
    	wrc/ppl.yy.c \
    	wrc/y.output

install: $(TARGET)
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(TARGET) $(DESTDIR)$(bindir)/$(PROGRAM)

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(PROGRAM)

clean:
	rm -f $(TARGET) $(OBJECTS)
	rm -f \
		parser.tab.c \
		parser.tab.h \
		parser.yy.c \
		wrc/parser.tab.c \
		wrc/parser.tab.h \
		wrc/ppy.tab.c \
		wrc/ppy.tab.h \
		wrc/parser.yy.c \
		wrc/ppl.yy.c \
		wrc/y.output
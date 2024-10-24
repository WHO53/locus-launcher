NAME=locus-launcher
BIN=${NAME}
SRC=.

PKGS = locus

LOCUS_LAUNCHER_SOURCES += $(wildcard $(SRC)/*.c) $(wildcard $(SRC)/**/*.c)
LOCUS_LAUNCHER_HEADERS += $(wildcard $(SRC)/*.h) $(wildcard $(SRC)/**/*.h)

CFLAGS += -std=gnu99 -Wall -g -Wno-format-truncation
CFLAGS += $(shell pkg-config --cflags $(PKGS))
LDFLAGS += $(shell pkg-config --libs $(PKGS)) -lm -lutil -lrt

SOURCES = $(LOCUS_LAUNCHER_SOURCES)

OBJECTS = $(SOURCES:.c=.o)

all: ${BIN}

$(OBJECTS): $(LOCUS_LAUNCHER_HEADERS)

$(BIN):$(OBJECTS)
	$(CC) -o $(BIN) $(OBJECTS) $(LDFLAGS)

clean:
	rm -f $(OBJECTS) ${BIN}

format:
	clang-format -i $(LOCUS_LAUNCHER_SOURCES) $(LOCUS_LAUNCHER_HEADERS)

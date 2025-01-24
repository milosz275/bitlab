CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address -O2 -Wno-unused-result
INCLUDES = -Iinclude
CLIBS = -lpthread -lreadline -lcrypto -lssl
CSRCS = $(wildcard src/*.c)
COBJS = $(CSRCS:.c=.o)
COBJS := $(addprefix build/, $(COBJS))
MAIN = main

.PHONY: default all debug clean depend

default: all

all: $(MAIN)
	@echo "Bitlab has been compiled"

debug: CFLAGS += -g -DDEBUG -D_DEBUG
debug: $(MAIN)
	@echo "Bitlab has been compiled in debug mode"

$(MAIN): $(COBJS)
	@mkdir -p build/bin
	$(CC) $(CFLAGS) $(INCLUDES) -o build/bin/$(MAIN) $(COBJS) $(CLIBS)

build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

depend: $(CSRCS)
	makedepend $(INCLUDES) $^

clean:
	$(RM) build/src/*.o *~ $(MAIN)
	$(RM) build/bin/$(MAIN)
	$(RM) build/lib/*.a

-include $(SRCS:.c=.d)

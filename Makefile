override CFLAGS= -std=c17 -DPERF_METRICS -Wall -Wextra -Wshadow -Wno-unused-parameter -Wno-unused-const-variable -g -O0

NAME=backup-manager

.PHONY: clean all

all: ${NAME}

SOURCES=$(shell find src -type f -iname '*.c')

OBJECTS=$(foreach x, $(basename $(SOURCES)), $(x).o)

$(NAME): $(OBJECTS)
		$(CC) $^ ${CFLAGS} -o $@
clean:
	rm -f $(NAME) $(OBJECTS)
CC      = gcc
CFLAGS  = -Wall -g -I../common
SRCS    = $(wildcard *.c)
TARGETS = $(SRCS:.c=)

# common を使う必要があるかどうかを自動判定（ソース内に utmplib.h があるか探す）
USE_COMMON = $(shell grep -l "utmplib.h" $(SRCS) 2>/dev/null)
COMMON_SRC = ../common/utmplib.c

all: $(TARGETS)

# 各ターゲットのビルド
%: %.c
	@if [ -n "$(USE_COMMON)" ]; then \
		echo "Building $@ with common library..."; \
		$(CC) $(CFLAGS) $< $(COMMON_SRC) -o $@; \
	else \
		echo "Building $@ without common library..."; \
		$(CC) $(CFLAGS) $< -o $@; \
	fi

clean:
	rm -f $(TARGETS) *.o

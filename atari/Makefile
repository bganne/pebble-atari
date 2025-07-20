CC = m68k-atari-mint-gcc
LD = m68k-atari-mint-ld

CFLAGS = -Os -Wall -Wextra -m68000 -fomit-frame-pointer
LDFLAGS = -lgem

TARGET = CLOCK.PRG
SRCS = CLOCK.C
OBJS = $(SRCS:.C=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.C
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean

CC = mpicc
CFLAGS = -Ofast -march=native -mtune=native -funroll-loops -fomit-frame-pointer
LDFLAGS = -lm

SRCS = lib.c logic.c dispatcher.c worker.c main.c
OBJS = $(SRCS:.c=.o)
TARGET = program

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

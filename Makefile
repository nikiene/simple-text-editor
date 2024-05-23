CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99
OBJS = main.o

main: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJS) main

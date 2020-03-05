CC = gcc
CPPFLAGS = -Iinclude
CFLAGS = -g -Wall -Wextra -Werror -std=gnu99
LDLIBS = -pthread
LDFLAGS =
TEST_CFLAGS = -g -Wall -Wextra -std=gnu99
TEST_LDLIBS = -pthread -lcheck

all: server client showip

server: obj/main.o obj/server.o obj/handler.o obj/queue.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

check: obj-tests/main.o obj-tests/test-queue.o obj/queue.o
	$(CC) $(LDFLAGS) $^ $(TEST_LDLIBS) -o $@

client: obj/client.o
	$(CC) $^ -o $@

showip: obj/showip.o
	$(CC) $^ -o $@

obj/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

obj-tests/%.o: tests/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) -c $< -o $@

clean:
	$(RM) client server showip check obj/*.o obj-tests/*.o

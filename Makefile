CC = gcc
CPPFLAGS = -Iinclude
CFLAGS = -g -Wall  # -std=c99  # TODO
LDLIBS =
LDFLAGS =

all: server client showip

server: obj/server.o obj/handler.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

client: obj/client.o
	$(CC) $^ -o $@

showip: obj/showip.o
	$(CC) $^ -o $@

obj/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	$(RM) client server showip obj/*.o

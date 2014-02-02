CC=clang++
CFLAGS=-g -c -Wall -std=c++11
LDFLAGS=
SRCS=build_tree.cc main.cc
OBJS=$(SRCS:.cc=.o)
EXEC=decode_tree

all: $(SRCS) $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

.cc.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o $(EXEC) *~




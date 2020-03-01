INCLUDE_PATH=-Iinclude
LIBS=-lpthread
CC=gcc
FLAGS=-fPIC -shared

all:src/event_loop.o src/future.o
	$(CC) $(FLAGS) -o asynclib.so $(LIBS) src/event_loop.o src/future.o

src/event_loop.o: src/event_loop.c include/event_loop.h
	$(CC) $(FLAGS) -o src/event_loop.o -c src/event_loop.c $(INCLUDE_PATH)

src/future.o: src/future.c include/future.h
	$(CC) $(FLAGS) -o src/future.o -c src/future.c $(INCLUDE_PATH)


clean:
	find -name "*.o" -exec rm {} \;
	find -name "*.so" -exec rm {} \;

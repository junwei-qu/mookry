INCLUDE_PATH=-Iinclude
CC=gcc
FLAGS=-fPIC

all:src/event_loop.o src/future.o common/balance_binary_heap.o
	$(CC) -shared -o asynclib.so $(LIBS) src/event_loop.o src/future.o common/balance_binary_heap.o

src/event_loop.o: src/event_loop.c include/event_loop.h
	$(CC) $(FLAGS) -o src/event_loop.o -c src/event_loop.c $(INCLUDE_PATH)

src/future.o: src/future.c include/future.h
	$(CC) $(FLAGS) -o src/future.o -c src/future.c $(INCLUDE_PATH)

common/balance_binary_heap.o: common/balance_binary_heap.c include/balance_binary_heap.h 
	$(CC) $(FLAGS) -o common/balance_binary_heap.o -c common/balance_binary_heap.c $(INCLUDE_PATH)

clean:
	find -name "*.o" -exec rm {} \;
	find -name "*.so" -exec rm {} \;

INCLUDE_PATH=-Iinclude
CC=gcc
FLAGS=-fPIC

all: src/core/event_loop.o src/core/balance_binary_heap.o src/core/channel.o src/core/coroutine.o src/boost/make_fcontext.o src/boost/jump_fcontext.o
	$(CC) -shared $(FLAGS) -Wl,-soname,libmookry.so -o mookry.so src/core/channel.o src/core/event_loop.o src/core/balance_binary_heap.o src/core/coroutine.o src/boost/make_fcontext.o src/boost/jump_fcontext.o

src/core/channel.o: src/core/channel.c include/channel.h
	$(CC) $(FLAGS) -o src/core/channel.o -c src/core/channel.c $(INCLUDE_PATH)

src/core/coroutine.o: src/core/coroutine.c include/coroutine.h
	$(CC) $(FLAGS) -o src/core/coroutine.o -c src/core/coroutine.c $(INCLUDE_PATH)

src/boost/make_fcontext.o: src/boost/make_x86_64_sysv_elf_gas.S
	$(CC) $(FLAGS) -o src/boost/make_fcontext.o -c src/boost/make_x86_64_sysv_elf_gas.S

src/boost/jump_fcontext.o: src/boost/jump_x86_64_sysv_elf_gas.S
	$(CC) $(FLAGS) -o src/boost/jump_fcontext.o -c src/boost/jump_x86_64_sysv_elf_gas.S

src/core/event_loop.o: src/core/event_loop.c include/event_loop.h
	$(CC) $(FLAGS) -o src/core/event_loop.o -c src/core/event_loop.c $(INCLUDE_PATH)

src/core/balance_binary_heap.o: src/core/balance_binary_heap.c include/balance_binary_heap.h 
	$(CC) $(FLAGS) -o src/core/balance_binary_heap.o -c src/core/balance_binary_heap.c $(INCLUDE_PATH)
install:
	if [[ ! -e /usr/include/mookry ]];then \
	    mkdir /usr/include/mookry; \
	fi
	cp -f include/coroutine.h include/extend_errno.h /usr/include/mookry
	cp -f mookry.so /usr/lib64/libmookry.so
uninstall:
	rm -f /usr/lib64/libmookry.so
	rm -rf /usr/include/mookry
clean:
	find -name "*.o" -exec rm {} \;
	find -name "*.so" -exec rm {} \;
	rm -f mookry.so 

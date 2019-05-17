CC=gcc
CXX=g++
CFLAGS=-DDEBUG_CHILD
RM=rm
LDFLAGS=-pie -ldl -g

default: factinject exp0 exp1 exp2

clean:
	$(RM) -rf *.o factinject test? exp?

factinject: factinject.o inject.o rbreak.o segment.o
	$(CC) $(LDFLAGS) $^ -o $@

test0: test0.o
	$(CC) -g $^ -o $@

exp%: exp%.o
	$(CC) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -fpie -fexceptions -g -c $^ 

%.obj: %.cpp
	$(CXX) $(CFLAGS) -g -c $^


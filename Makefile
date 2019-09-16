CC=gcc
CXX=g++
CFLAGS=
RM=rm
LDFLAGS=-pie -ldl -g

default: factinject exp0 exp1 exp2

clean:
	$(RM) -rf *.o factinject test? exp?

factinject: factinject.obj inject.o rbreak.o segment.o Inject.obj
	$(CXX) $(LDFLAGS) $^ -o $@

test0: test0.o
	$(CC) -g $^ -o $@

exp%: exp%.o
	$(CC) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -fpie -fexceptions -g -c $^ -o $@

%.obj: %.cpp
	$(CXX) $(CFLAGS) -fpie -fexceptions -g -c $^ -o $@


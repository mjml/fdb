CC=gcc
CXX=g++
RM=rm
LDFLAGS=-pie -ldl -g

default: factinject exp0

clean:
	$(RM) -rf *.o factinject test? exp?

factinject: factinject.o findpid.o inject.o rbreak.o
	$(CC) $(LDFLAGS) $^ -o $@

test0: test0.o
	$(CC) -g $^ -o $@

exp%: exp%.o
	$(CC) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) -fpie -fexceptions -g -c $^ 

%.obj: %.cpp
	$(CXX) -g -c $^


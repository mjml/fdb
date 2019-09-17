CC=gcc
CXX=g++
CFLAGS=-I. -DDEBUG -DLOGLEVEL_FACTINJECT=6 -fpie -fexceptions
CXXFLAGS=-I. -DDEBUG -DLOGLEVEL_FACTINJECT=6 -std=c++17 -fpie -fexceptions
RM=rm
LDFLAGS=-pie -ldl -g

default: factinject exp0 exp1 exp2

clean:
	$(RM) -rf *.o *.obj factinject test? exp?

factinject: factinject.obj segment.o Inject.obj util/log.obj
	$(CXX) $(LDFLAGS) $^ -o $@

test0: test0.o
	$(CC) -g $^ -o $@

exp%: exp%.o Inject.obj segment.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -g -c $^ -o $@

%.obj: %.cpp
	$(CXX) $(CXXFLAGS) -g -c $^ -o $@


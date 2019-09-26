CC=gcc
CXX=g++
CFLAGS=-g -O0 -I. -DDEBUG -DLOGLEVEL_FACTINJECT=8 -fpie -fexceptions
CXXFLAGS=-g -O0 -I. -DDEBUG -DLOGLEVEL_FACTINJECT=8 -std=c++17 -fpie -fexceptions
RM=rm
LDFLAGS=-pie -ldl -g

default: factinject

clean:
	$(RM) -rf *.o *.obj util/*.o util/*.obj factinject test? exp?

factinject: factinject.obj Inject.obj 
	$(CXX) $(LDFLAGS) -rdynamic $^ -o $@

activestub.so: Inject.obj
	$(CXX) $(LDFLAGS) -shared -pie -rdynamic -pthread $^ -o $@

test0: test0.o
	$(CC) -g $^ -o $@

exp%: exp%.o Inject.obj segment.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -g -c $^ -o $@

%.obj: %.cpp
	$(CXX) $(CXXFLAGS) -g -c $^ -o $@


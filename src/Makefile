CC=gcc
CXX=g++
CFLAGS=-g -O0 -I. -DDEBUG -DLOGLEVEL_FACTINJECT=10 -DLOGLEVEL_WAIT=10 -fexceptions
CXXFLAGS=-fpic -g -O0 -I. -DDEBUG -DLOGLEVEL_FACTINJECT=10 -DLOGLEVEL_WAIT=10 -DLOGLEVEL_RB=10 -std=c++17 -fexceptions
RM=rm
LDFLAGS=-g -lrt

default: factinject factstub.so

clean:
	$(RM) -rf *.o *.obj util/*.o util/*.obj factinject test? exp?

factinject: factinject.obj Inject.obj 
	$(CXX) $(LDFLAGS) -pie -ldl -rdynamic $^ -o $@

factstub.so: factstub.obj Inject.obj
	$(CXX) $(LDFLAGS) -shared -rdynamic -pthread $^ -o $@

test0: test0.o
	$(CC) -g $^ -o $@

exp%: exp%.o Inject.obj segment.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -g -c $^ -o $@

%.obj: %.cpp
	$(CXX) $(CXXFLAGS) -g -c $^ -o $@


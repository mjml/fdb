factinject: factinject.o findpid.o
	$(CC) -g $^ -o $@

%.o: %.c
	$(CC) -g -c $^ 

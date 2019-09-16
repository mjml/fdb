MAKE=make

@DEFAULT:
	$(MAKE) -C src

clean:
	$(MAKE) -C src clean

CC_FOR_DEPS ?= $(CC)

%.d: %.c Makefile
	$(CC_FOR_DEPS) $(CFLAGS) -Iinclude -MM $*.c -o $*.d.new
	if ! diff -q $*.d $*.d.new 2>/dev/null; then cp $*.d.new $*.d; else rm $*.d.new; fi

%.o: %.c Makefile
	$(CC) $(CFLAGS) -Iinclude $*.c -o $*.o -c

%.exe: %.o Makefile
	$(CC) $(CFLAGS) $*.o -o $*.exe $(LINKS)

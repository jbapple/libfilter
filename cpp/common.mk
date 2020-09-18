CXX_FOR_DEPS ?= $(CXX)

%.d: %.cpp Makefile
	$(CXX_FOR_DEPS) $(CXXFLAGS) -Iinclude -MM $*.cpp -o $*.d.new
	if ! diff -q $*.d $*.d.new 2>/dev/null; then mv $*.d.new $*.d; else rm $*.d.new; fi

%.o: %.cpp Makefile
	$(CXX) $(CXXFLAGS) $*.cpp -o $*.o -c

%.exe: %.o Makefile
	$(CXX) $(CXXFLAGS) $*.o -o $*.exe $(LINKS)

.PHONY: world clean test

world: libfilter.jar test

libfilter/Block.class: libfilter/Block.java Makefile
	javac libfilter/Block.java

libfilter.jar: libfilter/Block.class Makefile
	jar cf libfilter.jar libfilter/Library.class

libfilter/test/BlockTest.class: libfilter/test/BlockTest.java Makefile
	javac -cp ".:/usr/share/java/junit4.jar:/usr/share/java/hamcrest-core-1.3.jar" libfilter/test/BlockTest.java

test: libfilter/Block.class libfilter/test/BlockTest.class
	$(MAKE) -C libfilter
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:./libfilter/ java -cp ".:/usr/share/java/junit4.jar:/usr/share/java/hamcrest-core-1.3.jar" org.junit.runner.JUnitCore libfilter.test.BlockTest

clean:
	$(MAKE) -C libfilter clean
	rm -f libfilter/Block.class
	rm -f libfilter/test/BlockTest.class
	rm -f libfilter.jar

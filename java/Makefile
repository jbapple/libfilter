.PHONY: world libjni-bridge clean test

world: libfilter.jar test

libjni-bridge: Makefile
	$(MAKE) -C jni-bridge

libfilter/Library.class: libfilter/Library.java Makefile
	javac libfilter/Library.java

libfilter.jar: libfilter/Library.class Makefile
	jar cf libfilter.jar libfilter/Library.class

libfilter/LibraryTest.class: libfilter/LibraryTest.java Makefile
	javac -cp ".:/usr/share/java/junit4.jar:/usr/share/java/hamcrest-core-1.3.jar" libfilter/LibraryTest.java

test: libfilter/Library.class libfilter/LibraryTest.class libjni-bridge
	java -Djava.library.path=jni-bridge -cp ".:/usr/share/java/junit4.jar:/usr/share/java/hamcrest-core-1.3.jar" org.junit.runner.JUnitCore libfilter.LibraryTest

clean:
	$(MAKE) -C jni-bridge clean
	rm -f libfilter/Library.class
	rm -f libfilter/LibraryTest.class
	rm -f libfilter.jar

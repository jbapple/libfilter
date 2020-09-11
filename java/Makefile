.PHONY: world libjni-bridge clean test

world: libfilter.jar test

libjni-bridge: Makefile
	$(MAKE) -C jni-bridge

libfilter/Library.class: libfilter/Library.java Makefile
	javac libfilter/Library.java

libfilter.jar: libfilter/Library.class Makefile
	jar cf libfilter.jar libfilter/Library.class

LibraryTest.class: LibraryTest.java Makefile
	javac -cp ".:/usr/share/java/junit4.jar:/usr/share/java/hamcrest-core-1.3.jar" LibraryTest.java

test: libfilter/Library.class LibraryTest.class libjni-bridge
	java -Djava.library.path=jni-bridge -cp ".:/usr/share/java/junit4.jar:/usr/share/java/hamcrest-core-1.3.jar" org.junit.runner.JUnitCore LibraryTest

clean:
	$(MAKE) -C jni-bridge clean
	rm -f libfilter/Library.class
	rm -f LibraryTest.class
	rm -f libfilter.jar

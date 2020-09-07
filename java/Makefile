.PHONY: all libjni-bridge jar clean test

all: jar

libjni-bridge: Makefile
	$(MAKE) -C jni-bridge

Library.class: Library.java Makefile
	javac Library.java

# libfilter.jar: src/main/java/Library.class Makefile
# 	jar cf libfilter.jar src/main/java/Library.class

LibraryTest.class: LibraryTest.java Makefile
	javac -cp ".:/usr/share/java/junit4.jar:/usr/share/java/hamcrest-core-1.3.jar" LibraryTest.java

test: Library.class LibraryTest.class libjni-bridge
	java -Djava.library.path=jni-bridge -cp ".:/usr/share/java/junit4.jar:/usr/share/java/hamcrest-core-1.3.jar" org.junit.runner.JUnitCore LibraryTest

clean:
	$(MAKE) -C jni-bridge clean
	rm -f src/main/java/Library.class
	rm -f libfilter.jar

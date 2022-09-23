.PHONY: default clean install uninstall lib world c-world cpp-world java-world go-world python-world

default: lib

export PROJECT_ROOT := $(CURDIR)

lib:
	$(MAKE) -C c lib

install:
	$(MAKE) -C c install
	$(MAKE) -C cpp install

uninstall:
	$(MAKE) -C cpp uninstall
	$(MAKE) -C c uninstall

clean:
	$(MAKE) -C c clean
	$(MAKE) -C cpp clean
	mvn -f ./java/ clean
	cd go && go clean

# TODO: clean up for java & gradle

world: c-world cpp-world java-world go-world python-world

c-world: lib
	$(MAKE) -C c world

cpp-world: lib
	$(MAKE) -C cpp world

java-world:
	mvn -f ./java/ package -Dmaven.test.skip=true

go-world: lib
	cd go && go build -p 4

python-world: lib
	cd python && python3 build.py

anonymous.zip: $(shell git ls-files)
	git archive-all --prefix=libfilter/ anonymous.zip
	zip -d anonymous.zip libfilter/README.md libfilter/doc/doc.tex libfilter/doc/taffy/taffy.tex libfilter/java/* libfilter/go/go.mod CMakeLists.txt
#TODO: java install

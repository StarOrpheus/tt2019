.PHONY: all, run, clean

COMPILER=g++
OPTIONS=-O3 -flto -D NDEBUG -march=native --std=c++17 -o main
SOURCES=task0.cpp include/lambdas.h

all: $(SOURCES)
	$(COMPILER) $(SOURCES) $(OPTIONS)
run:
	./main
clean:
	rm ./main

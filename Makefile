.PHONY: all run clean

SRC:= time/time.cpp reactor/reactor.cpp main.cpp

all:
	g++ -std=c++17 -Wall -Wextra -g $(SRC) -o a.out -lglog

run:
	make all
	./a.out

clean:
	rm -f output/*
	rm a.out
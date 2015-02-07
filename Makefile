.PHONY: parsim

parsim:
	mkdir -p bin
	g++ -std=c++11 -o bin/frontEnd src/frontEnd.cpp -lpthread -lrt
	g++ -std=c++11 -o bin/middleEnd src/middleEnd.cpp -lpthread -lrt
	g++ -std=c++11 -o bin/backEnd src/backEnd.cpp -lpthread -lrt
	g++ -std=c++11 -o bin/parsim src/parsim.cpp -lpthread -lrt

test:
	bin/parsim -m MEM -p SEM -s 0 10 -s 1 8 -c 9 -b 1

clean:
	rm -rf bin
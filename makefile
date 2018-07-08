all:pub sub

pub:pub.cpp node.hpp
	g++ $< -o $@ -O2 -std=c++11

sub:sub.cpp node.hpp
	g++ $< -o $@ -O2 -std=c++11

clean:
	rm ./pub ./sub

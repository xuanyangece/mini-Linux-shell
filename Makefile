FLAGS=-std=gnu++11 -ggdb3 -pedantic -Wall -Werror

myShell: main.cpp
	g++ $(FLAGS) -o myShell main.cpp

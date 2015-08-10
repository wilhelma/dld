#include <cstdlib>
#include <iostream>
#include <fstream>
#include <time.h>

int foo(int p) {

	std::cout << "foo" << std::endl;

	int x = 2 * p;
	x = 3 + x;
	x = x / p;
	return x;
}

int bar() {

	std::ofstream of;
	of.open("outstream.out", std::ofstream::out | std::ofstream::app);
	int val;


	srand(time(NULL));	
	val = rand() % 10 + 1;
	of << "my file oleee " << val;
	of.close();
	return val;
}


int main(int argc, char** argv) {

	int a, b = 0;

	bar();

	if (argc > 1)
		a = foo(atoi(argv[1]));
	else
		a = foo(4) + bar();

	for (int i=0; i<a; ++i) {

		b = b + a;
		b = b * a;
		if (b > 10000)
			break;
	}


	std::cout << "result is " << b << std::endl;

	return 0;
}
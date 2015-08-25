#include <cstdlib>


int main(int argc, char** argv) {

	int a, b = 0;

	if (argc > 1)
		a = atoi(argv[1]);
	else
		a = 10;

	for (int i=0; i<100; ++i) {

		if (i < 50)
			a = a + 1;
		if (i > 50)
			a = a - 1;
	}

	for (int i=0; i<11; ++i) {

		b = b + a;
		b = b * a;
		if (b > 1000000000)
			break;
	}

	int i=20;
	while (i > 5) {
		b = b - a;
		a = a + b;
		i--;
	}

	i = 0;
	do {
		for (int j=0 ; j<5; j++)
			a = a - b;
		if (b == 1000)
			break;
		i++;
	} while (i < 20);

	return 0;
}

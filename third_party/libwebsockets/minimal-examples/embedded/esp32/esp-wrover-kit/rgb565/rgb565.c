/*
 * gcc /tmp/q.c && convert cat-565.png -depth 8 rgb:- | ./a.out > cat-565.h
 */

#include <stdio.h>

int main()
{
	int r, g, b, w, m = 0;

	while (1) {
		r = getchar();
		g = getchar();
		b = getchar();

		if (r == EOF || g == EOF || b == EOF)
			return  r == EOF;

		w = (b >> 3) | ((g >> 2) << 5) | ((r >> 3) << 11);
		printf("0x%02X, 0x%02X, ", (w >> 8) & 0xFF, w & 0xFF);

		if (((++m) & 3) == 0)
			printf("\n");
	}
}


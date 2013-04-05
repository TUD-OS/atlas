#include <time.h>
#include <stdio.h>

int main(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts.tv_sec += 2;
	printf("%lld\n", (long long)ts.tv_sec * 1000000 + (ts.tv_nsec + 500) / 1000);
	return 0;
}


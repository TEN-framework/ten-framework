#include <clingo.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    int major, minor, revision;

    clingo_version(&major, &minor, &revision);

    printf("Hello, this is clingo version %d.%d.%d.\n", major, minor, revision);

    return 0;
}

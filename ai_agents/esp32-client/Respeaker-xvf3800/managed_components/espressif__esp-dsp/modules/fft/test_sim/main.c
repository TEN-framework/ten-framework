#include <stdlib.h>
#include <stdio.h>

void test_fft2r();

int main(void)
{
    printf("main starts!\n");
//    xt_iss_profile_enable();
    test_fft2r();
//    xt_iss_profile_disable();

    printf("Test done\n");
}

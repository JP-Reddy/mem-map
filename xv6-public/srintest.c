#include "types.h"
#include "stat.h"
#include "user.h"

char *str = "You can't change a character!";

void test_task3()
{
    str[1] = 'O';
    printf(1, "%s\n", str);
}

void test_task4_1()
{

#ifdef COW
    int i = 10;
#else
    int i = 20;
#endif

    int res = fork();

    i = i * 20;

    if (i == 10 * 20)
    {
        printf(1, "PID: %d, COW enabled\n", res);
    }
    else
    {
        printf(1, "PID: %d, COW disabled\n", res);
    }

    if (res > 0)
    {
        wait();
    }
}

int main() {
    test_task4_1();
    exit();
}

/* 
TODO SRINAG
This is a test program that needs to be removed before submission.

Run `make qemu-nox COW=1` to execute with COW.
*/
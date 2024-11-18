#include "types.h"
#include "stat.h"
#include "user.h"

#define PGSIZE          4096    // bytes mapped by a page

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

void test_task4_2()
{
    int big_array[PGSIZE * 4];

#ifdef COW
    int v = 10;
#else
    int v = 20;
#endif

    for (int i = 0; i < sizeof(big_array) / sizeof(int); ++i)
    {
        big_array[i] = i;
    }

    int pid1 = fork();
    int pid2 = fork();
    int res;

    if (pid1 != 0 && pid2 != 0)
    {
        res = 1;
    }
    else if (pid1 == 0 && pid2 == 0)
    {
        res = 4;
    }
    else if (pid1 == 0)
    {
        res = 2;
    }
    else
    {
        res = 3;
    }

    for (int i = 0; i < sizeof(big_array) / sizeof(int); ++i)
    {
        big_array[i] = v;
    }

    if (big_array[0] == 10)
    {
        printf(1, "P: %d, COW enabled\n", res);
    }
    else
    {
        printf(1, "P: %d, COW disabled\n", res);
    }

    if (res == 1 || res == 2)
    {
        wait();
    }
}

int main() {
    test_task4_2();
    exit();
}

/* 
TODO SRINAG
This is a test program that needs to be removed before submission.

Run `make qemu-nox COW=1` to execute with COW.
*/
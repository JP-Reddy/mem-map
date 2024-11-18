#include "types.h"
#include "stat.h"
#include "user.h"

#define PGSIZE          4096    // bytes mapped by a page

char *str = "You can't change a character!";

void test_task3()
{
    printf(1, "%s start\n", __func__);
    printf(1, "Expect segfault\n");
    str[1] = 'O';
    printf(1, "%s\n", str);
    printf(1, "%s end\n", __func__);
}

void test_task4_1()
{
    printf(1, "%s start\n", __func__);

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

    printf(1, "%s end\n", __func__);
}

void test_task4_2()
{
    printf(1, "%s start\n", __func__);

    const int array_size = PGSIZE * 1;
    int big_array[array_size];
    // int big_array[PGSIZE * 1];

#ifdef COW
    int v = 10;
#else
    int v = 20;
#endif

    big_array[1] = v;

    for (int i = 0; i < array_size; ++i)
    {
        printf(1, "%d\n", i);
        big_array[i] = i * v;
    }

    // int pid1 = fork();
    // int pid2 = fork();
    int res = 1;//pid1;

    // if (pid1 != 0 && pid2 != 0)
    // {
    //     res = 1;
    // }
    // else if (pid1 == 0 && pid2 == 0)
    // {
    //     res = 4;
    // }
    // else if (pid1 == 0)
    // {
    //     res = 2;
    // }
    // else
    // {
    //     res = 3;
    // }

    // for (int i = 0; i < array_size; ++i)
    // {
    //     big_array[i] = v;
    // }

    if (big_array[1] == 10)
    {
        printf(1, "P: %d, COW enabled\n", res);
    }
    else
    {
        printf(1, "P: %d, COW disabled\n", res);
    }

    // if (res == 1 || res == 2)
    if (res > 0)
    {
        // wait();
    }

    printf(1, "%s end\n", __func__);
}

int main() {
    void (*test[])() = 
    {
        // test_task4_1,
        test_task4_2,
        // test_task3,
    };

    for (int i = 0; i < sizeof(test) / sizeof(test[0]); ++i)
    {
        (test[i])();
    }

    exit();
}

/* 
TODO SRINAG
This is a test program that needs to be removed before submission.

Run `make qemu-nox COW=1` to execute with COW.
*/
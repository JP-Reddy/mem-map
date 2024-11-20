#include "types.h"
#include "stat.h"
#include "user.h"

#define PGSIZE          4096    // bytes mapped by a page

char *str = "You can't change a character!";

void test_task3()
{
    printf(1, "\n%s start\n", __func__);
    printf(1, "**Expect segfault**\n");
    str[1] = 'O';
    printf(1, "%s\n", str);
    printf(1, "%s end\n", __func__);
}

void test_task4_1()
{
    printf(1, "\n%s start\n", __func__);

#ifndef NO_COW
    int i = 10;
#else
    int i = 20;
#endif

    int p = fork();

    i = i * 20;

    if (i == 10 * 20)
    {
        printf(1, "PID: %d, COW enabled\n", p);
    }
    else
    {
        printf(1, "PID: %d, COW disabled\n", p);
    }

    if (p > 0)
    {
        wait();
        printf(1, "%s end\n", __func__);
    }
}

void test_task4_2()
{
    printf(1, "\n%s start\n", __func__);

    const int array_size = PGSIZE * 1;
    int big_array[array_size];
    // int big_array[PGSIZE * 1];

#ifndef NO_COW
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

void test_task4_3()
{
    printf(1, "\n%s start\n", __func__);

#ifndef NO_COW
    int i = 10;
#else
    int i = 20;
#endif

    int p = 1;
    int pid1 = fork();

    if (pid1 == 0)
    {
        ++p;
        int pid2 = fork();

        if (pid2 == 0)
        {
            ++p;
        }
    }

    if (i == 10)
    {
        printf(1, "P: %d, COW enabled\n", p);
    }
    else
    {
        printf(1, "P: %d, COW disabled\n", p);
    }

    i = p * p;
    i = i / p;

    printf(1, "P: %d, i = %d\n", p, i);

    if (p == 1 || p == 2)
    {
        wait();
    }

    if (p == 1)
    {
        printf(1, "%s end\n", __func__);
    }
}

void test_task4_4()
{
    printf(1, "\n%s start\n", __func__);

    const int array_size = PGSIZE * 1 / 10;
    int array1[array_size];
    int array2[array_size];

#ifndef NO_COW
    int v = 10;
#else
    int v = 20;
#endif

    for (int i = 0; i < array_size; ++i)
    {
        array1[i] = i;
        array2[i] = i;
    }

    int p = fork() == 0 ? 2 : 1;

    for (int i = 0; i < array_size; ++i)
    {
        array1[i] = v * array2[i];
    }

    for (int i = 0; i < array_size; ++i)
    {
        if (array1[i] != v * array2[i])
        {
           printf(1, "P: %d !! Bug !!", p);
           break;
        }
    }

    if (p == 1)
    {
        wait();
        printf(1, "%s end\n", __func__);
    }
}

void test_task4_5()
{
    printf(1, "\n%s start\n", __func__);

#ifndef NO_COW
    int cow = 1;
#else
    int cow = 0;
#endif

    printf(1, "P%d COW enabled: %d\n", 1, cow);

    int *addr = malloc(sizeof(char));
    uint p1_pa = va2pa((uint)addr);

    printf(1, "P%d Virtual address: %d\n", 1, addr);
    printf(1, "P%d Phy address: %d\n", 1, p1_pa);

    int p = fork() ? 1 : 2;

    printf(1, "P%d Virtual address: %d\n", p, addr);
    printf(1, "P%d Phy address: %d\n", p, va2pa((uint)addr));

    if (p == 1)
    {
        wait();
        free(addr);
        printf(1, "%s end\n", __func__);
    }
}

int main() {
    void (*test[])() = 
    {
        // test_task3,
        // test_task4_1,
        // // test_task4_2,
        // test_task4_3,
        // test_task4_4,
        test_task4_5,
    };

    for (int i = 0; i < sizeof(test) / sizeof(test[0]); ++i)
    {
        int pid = fork();

        if (pid == 0)
        {
            (test[i])();
            exit();
        }
        else
        {
            wait();
        }
    }

    exit();
}

/* 
TODO SRINAG
This is a test program that needs to be removed before submission.

Run `make qemu-nox COW=1` to execute with COW.
*/
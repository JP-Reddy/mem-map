#include "types.h"
#include "stat.h"
#include "user.h"

char *str = "You can't change a character!";

int main() {
    str[1] = 'O';
    printf(1, "%s\n", str);
    exit();
}

/* 
TODO SRINAG
This is a test program for Task 3 that needs to be removed before submission.
The program will run in xv6 without the changes for Task 3.
The program will fail with a panic in xv6 with the changes for Task 3.
*/
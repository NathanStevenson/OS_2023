// traverse the process table
// count the number of times the write system call has been called across all processes
// store this count in a shared kernel variable or a global data structure

#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char **argv)
{
    int x = writecount();
    printf(1, "%d\n", x);
    exit();
}

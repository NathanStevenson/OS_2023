//takes an integer to set to numtickets
//we can assume that the maximum number of tickets per process is 100000
#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char **argv)
{
    if(argc < 2){
        printf(2, "invalid command. only one argument\n");
        exit();
    }

    if(atoi(argv[1]) < 0 || atoi(argv[1]) > 100000){
        printf(2, "The number of tickets must be between 0 and 100,000\n");
        exit();
    }

    settickets(atoi(argv[1]));
    exit();
}

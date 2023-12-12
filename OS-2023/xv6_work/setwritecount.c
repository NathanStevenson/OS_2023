//that takes one integer argument
//resets the count returned by the writecount call to new_count
//returns the value “0” when successful
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
    setwritecount(atoi(argv[1]));
    exit();
}

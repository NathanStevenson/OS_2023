#include "types.h"
#include "stat.h"
#include "user.h"

// int getprocessesinfo(struct processes_info *p);

int main(void) {
    struct processes_info pinfo;
    int result = getprocessesinfo(&pinfo);
    
    if (result == 0) {
        // loop through pinfo.pids, pinfo.times_scheduled, pinfo.tickets from processesinfo.h
    } else {
        printf(2, "Error: getprocessesinfo failed\n");
    }
    exit();
}

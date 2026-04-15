#include "kernel/types.h"
#include "user/user.h"

int main() {
    int pid_p = getpid();
    int pid_c = fork();

    if (pid_c == 0) {
        int v = co_yield(pid_p, 100);
        printf("Child received: %d\n", v);
        co_yield(pid_p, 0); // שחרור סופי לאבא
        exit(0);
    } else {
        sleep(5); 
        int v = co_yield(pid_c, 200);
        printf("Parent received: %d\n", v);
        wait(0);
        printf("DONE\n");
        exit(0);
    }
    return 0;
}
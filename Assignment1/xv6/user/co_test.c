#include "kernel/types.h"
#include "user/user.h"

int main() {
    int pid_p = getpid();
    int pid_c = fork();

    if (pid_c == 0) {
        // הילד מחכה לקבל מהאבא (אמור לקבל 2) [cite: 149]
        int v = co_yield(pid_p, 1);
        printf("Child received: %d\n", v);
        
        // איתות אחרון כדי שהאבא יוכל לחזור מה-co_yield שלו ולהדפיס
        co_yield(pid_p, 1);
        exit(0);
    } else {
        // האבא מחכה שהילד יכנס לקרנל (מונע קבלת 0)
        sleep(10); 
        
        // האבא שולח 2 ומחכה לקבל 1 חזרה [cite: 153]
        int v = co_yield(pid_c, 2);
        printf("Parent received: %d\n", v);
        
        wait(0);
        printf("Test complete.\n");
        exit(0);
    }
    return 0;
}
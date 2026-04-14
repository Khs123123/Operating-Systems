#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main() {
    int pid1 = getpid(); // אבא
    int pid2 = fork();   // ילד

    if (pid2 < 0) {
        printf("fork failed\n");
        exit(1);
    }

    if (pid2 == 0) { 
        // --- קוד הילד ---
        int val = co_yield(pid1, 1); // מחכה לאבא
        printf("Child received: %d\n", val);
        
        co_yield(pid1, 1); // מחזיר שליטה לאבא
        exit(0);
    } else { 
        // --- קוד האבא ---
        // מחכים שנייה כדי לוודא שהילד כבר "ישן" בתוך ה-co_yield שלו
        sleep(2); 
        
        int val = co_yield(pid2, 2); // שולח לילד ומחכה
        printf("Parent received: %d\n", val);
        
        wait(0); // מחכה לסיום הילד
        printf("Test finished successfully!\n");
        exit(0);
    }
    return 0;
}
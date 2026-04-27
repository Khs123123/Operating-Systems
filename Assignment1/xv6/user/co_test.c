#include "kernel/types.h"
#include "user/user.h"

int main() {
    int pid_p = getpid();
    int pid_c = fork();

    if (pid_c < 0) exit(1);

    if (pid_c == 0) { // תהליך הילד
        for (int i = 0; i < 5; i++) {
            int val = co_yield(pid_p, 1);
            printf("Child received: %d\n", val);
        }
        // הילד נרדם ומחכה למסירה השישית, אבל האבא יסגור את המשחק במקום
        co_yield(pid_p, 1); 
        exit(0);
    } else { // תהליך האב
        sleep(5); // נותן לילד להתמקם ולהיכנס להמתנה הראשונה
        
        for (int i = 0; i < 5; i++) {
            int val = co_yield(pid_c, 2);
            printf("parent received: %d\n", val);
        }
        
        // סיום המשחק באלגנטיות - מונע את הקיפאון
        kill(pid_c); // מעיר את הילד מההמתנה וסוגר אותו
        wait(0);     // מחכה שהילד יסיים להתנקות מהזיכרון
        printf("DONE\n");
        exit(0);
    }
}
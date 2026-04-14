#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    // 1. נדפיס את גודל הזיכרון הנוכחי
    printf("Memory size before allocation: %d bytes\n", memsize());

    // 2. נקצה 20K בתים (20 כפול 1024)
    void *ptr = malloc(20 * 1024);

    // 3. נדפיס שוב כדי לראות שהגודל השתנה
    printf("Memory size after allocating 20K: %d bytes\n", memsize());

    // 4. נשחרר את הזיכרון
    free(ptr);

    // 5. נדפיס את הגודל הסופי (שים לב להבדלים, xv6 לא תמיד מקטין את שטח הזיכרון מיד בחזרה!)
    printf("Memory size after free: %d bytes\n", memsize());

    exit(0);
}
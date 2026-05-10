#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    printf("--- Test 1: Seed 123 ---\n");
    lcg_srand(123);
    printf("1: %d\n", lcg_rand());
    printf("2: %d\n", lcg_rand());
    printf("3: %d\n", lcg_rand());

    printf("\n--- Test 2: Seed 123 (Should MATCH Test 1) ---\n");
    lcg_srand(123);
    printf("1: %d\n", lcg_rand());
    printf("2: %d\n", lcg_rand());
    printf("3: %d\n", lcg_rand());

    printf("\n--- Test 3: Seed 999 (Should be DIFFERENT) ---\n");
    lcg_srand(999);
    printf("1: %d\n", lcg_rand());
    printf("2: %d\n", lcg_rand());
    printf("3: %d\n", lcg_rand());

    exit(0);
}
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NUM_TEAMS 3
#define RUNNERS_PER_TEAM 5
#define TARGET_SCORE 30

int main(int argc, char *argv[]) {
    // Make sure the user provided a favoritism level
    if(argc < 2){
        printf("Usage: relay_race <favoritism_percentage>\n");
        exit(1);
    }

    int favoritism = atoi(argv[1]);
    int lock_id = israeli_create(favoritism);
    
    // Reset the kernel scoreboard to 0
    init_scores(); 
    
    printf("\n--- Starting Relay Race (Favoritism: %d%%) ---\n\n", favoritism);

    // Create the runners
    for(int t = 0; t < NUM_TEAMS; t++) {
        for(int r = 0; r < RUNNERS_PER_TEAM; r++) {
            int pid = fork();
            if(pid == 0) {
                // We are in the child process (the runner)
                setgid(t); // Join the team
                
                while(1) {
                    israeli_acquire(lock_id);
                    
                    // Advance the team score
                    int current_score = inc_score(t);
                    
                    if(current_score <= TARGET_SCORE) {
                        printf("Runner %d (Team %d) acquired the baton\nTeam %d score = %d\n", getpid(), t, t, current_score);
                        
                        if (current_score == TARGET_SCORE) {
                            printf("\n*** TEAM %d WINS THE RACE! ***\n\n", t);
                        }
                    }
                    
                    israeli_release(lock_id);
                    
                    // If our team hit the target, this runner can retire
                    if (current_score >= TARGET_SCORE) {
                        break; 
                    }
                    
                    pause(1); // Brief pause to force context switching
                }
                exit(0);
            }
        }
    }

    // The parent process waits for all runners to finish their laps
    for(int i = 0; i < (NUM_TEAMS * RUNNERS_PER_TEAM); i++) {
        wait(0);
    }

    israeli_destroy(lock_id);
    printf("Race Finished.\n");
    exit(0);
}
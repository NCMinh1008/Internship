#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_USERS 12
#define USERS_PER_TTI 4
#define MAX_TTIS 10000
#define MAX_MCS_INDEX 28
#define MAX_RB 100
#define TTI_DURATION 0.001
#define SCHEDULING_INTERVAL 40

int TBSArray[MAX_MCS_INDEX + 1][MAX_RB + 1];
typedef struct {
    int user_id;
    int mcs_index;
    int total_resource_blocks;  
    int current_resource_blocks; 
    int times_scheduled;
    long long total_data_transmitted;
    int last_scheduled_tti;
} User;

void assign_resource_blocks(User *user, int num_blocks, int current_tti);
void reset_resource_blocks(User *user);
int compare_users(const void *a, const void *b);
void proportional_scheduler(User users[], int num_users, int total_resource_blocks, int current_tti);
void generate_TBSArray(int TBSArray[MAX_MCS_INDEX + 1][MAX_RB + 1]);
void shuffle(int array[], int n);
void generate_unique_random_mcs(User users[], int num_users);

int main() {
    User users[MAX_USERS];
    int total_resource_blocks;
    int total_ttis = MAX_TTIS;
    int num_users = MAX_USERS;

    generate_TBSArray(TBSArray);

    // Initialize users
    for (int i = 0; i < MAX_USERS; i++) {
        users[i].user_id = i;
        users[i].total_resource_blocks = 0;
        users[i].current_resource_blocks = 0;
        users[i].times_scheduled = 0;
        users[i].total_data_transmitted = 0;
        users[i].last_scheduled_tti = -1;
    }

    srand(time(NULL));
    printf("THIS IS PROPORTIONAL_FAIR WITH MCS CHANGED EVERY TTI ALGORITHM\n");
    printf("Enter total number of resource blocks per TTI: ");
    scanf("%d", &total_resource_blocks);

    // Perform maximum CI scheduling over multiple TTIs
    for (int tti = 0; tti < total_ttis; tti++) {
        generate_unique_random_mcs(users, num_users);
        proportional_scheduler(users, MAX_USERS, total_resource_blocks, tti);
    }

    // Print resource blocks assigned to each user
    for (int i = 0; i < MAX_USERS; i++) {
        printf("User %d: Total RBs over %d TTIs = %d, Average per TTI = %.2f, Times scheduled = %d, Average throughput = %.3f Mbps\n",
               users[i].user_id, total_ttis, users[i].total_resource_blocks,
               (float)users[i].total_resource_blocks / total_ttis, users[i].times_scheduled, (users[i].total_data_transmitted * 8 / 1000000)/(total_ttis*TTI_DURATION));
    }

    // Calculate total bytes transmitted by all users
    long long total_bytes_all_users = 0;
    for (int i = 0; i < MAX_USERS; i++) {
        total_bytes_all_users += users[i].total_data_transmitted;
    }

    printf("\nAverage throughput over the entire cell = %.3f Mbps\n", (total_bytes_all_users * 8 / 1000000)/(total_ttis*TTI_DURATION));

    return 0;
}

void assign_resource_blocks(User *user, int num_blocks, int current_tti) {
    user->current_resource_blocks += num_blocks;
    user->total_resource_blocks += num_blocks;
    user->times_scheduled += 1;
    user->total_data_transmitted += TBSArray[user->mcs_index][num_blocks];
}


void reset_resource_blocks(User *user) {
    user->current_resource_blocks = 0;
}

int compare_users(const void *a, const void *b) {
    User *userA = (User *)a;
    User *userB = (User *)b;
    return userB->mcs_index - userA->mcs_index;  // Sort in descending order of mcs_index
}

void proportional_scheduler(User users[], int num_users, int total_resource_blocks, int current_tti) {
    User delay_users[MAX_USERS];
    User non_delay_users[MAX_USERS];
    int delay_count = 0;
    int non_delay_count = 0;

    // Separate delay and non-delay users
    for (int i = 0; i < num_users; i++) {
        if (current_tti - users[i].last_scheduled_tti >= SCHEDULING_INTERVAL) {
            delay_users[delay_count++] = users[i];
        } else {
            non_delay_users[non_delay_count++] = users[i];
        }
    }

    // Sort delay users by MCS index in descending order
    qsort(delay_users, delay_count, sizeof(User), compare_users);

    // Sort non_delay_users by MCS index in descending order
    qsort(non_delay_users, non_delay_count, sizeof(User), compare_users);

    int users_scheduled = 0;
    int blocks_per_user = total_resource_blocks / USERS_PER_TTI;
    int remaining_blocks = total_resource_blocks % USERS_PER_TTI;

    for (int i = 0; i < USERS_PER_TTI; i++) {
        if (i < delay_count) {
            reset_resource_blocks(&delay_users[i]);
        }
        if (i < non_delay_count) {
            reset_resource_blocks(&non_delay_users[i]);
        }
    }

    // Schedule delay users first
    for (int i = 0; i < delay_count && users_scheduled < USERS_PER_TTI; i++) {
        assign_resource_blocks(&delay_users[i], blocks_per_user, current_tti);
        users_scheduled++;
    }

    // Schedule non-delay users to fill remaining slots
    for (int i = 0; i < non_delay_count && users_scheduled < USERS_PER_TTI; i++) {
        assign_resource_blocks(&non_delay_users[i], blocks_per_user, current_tti);
        users_scheduled++;
    }

    // Distribute remaining blocks if any
    int index = 0;
    while (remaining_blocks > 0) {
        if (index < delay_count) {
            assign_resource_blocks(&delay_users[index], 1, current_tti);
        } else if (index - delay_count < non_delay_count) {
            assign_resource_blocks(&non_delay_users[index - delay_count], 1, current_tti);
        }
        remaining_blocks--;
        index++;
    }

    // Update the original users array with the scheduled information
    for (int i = 0; i < delay_count; i++) {
        users[delay_users[i].user_id] = delay_users[i];
    }
    for (int i = 0; i < non_delay_count; i++) {
        users[non_delay_users[i].user_id] = non_delay_users[i];
    }
}



void generate_TBSArray(int TBSArray[MAX_MCS_INDEX + 1][MAX_RB + 1]) {
    int base_tbs = 100; 
    int increment_mcs = 50; 
    int increment_rb = 10; // 

    for (int mcs = 0; mcs <= MAX_MCS_INDEX; mcs++) {
        for (int rb = 0; rb <= MAX_RB; rb++) {
            TBSArray[mcs][rb] = base_tbs + mcs * increment_mcs + rb * increment_rb;
        }
    }
}

void shuffle(int array[], int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

void generate_unique_random_mcs(User users[], int num_users) {
    int mcs_indices[MAX_MCS_INDEX + 1];
    for (int i = 0; i <= MAX_MCS_INDEX; i++) {
        mcs_indices[i] = i;
    }
    shuffle(mcs_indices, MAX_MCS_INDEX + 1);
    for (int i = 0; i < num_users; i++) {
        users[i].mcs_index = mcs_indices[i];
    }
}

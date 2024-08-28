#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MAX_USERS 12
#define USERS_PER_TTI 4
#define MAX_TTIS 10000
#define MAX_MCS_INDEX 28
#define MAX_RB 100
#define TTI_DURATION 0.001
#define MCS_COUNT (MAX_MCS_INDEX + 1)

int TBSArray[MAX_MCS_INDEX + 1][MAX_RB + 1];
typedef struct {
    int user_id;
    int mcs_index;
    int total_resource_blocks;  
    int current_resource_blocks; 
    int times_scheduled;
    long long total_data_transmitted;
} User;

void assign_resource_blocks(User *user, int num_blocks);
void reset_resource_blocks(User *user);
void round_robin_scheduler(User users[], int num_users, int total_resource_blocks, int start_index);
void generate_TBSArray(int TBSArray[MAX_MCS_INDEX + 1][MAX_RB + 1]);
void shuffle(int array[], int n);
void generate_and_save_mcs_indices();
void load_mcs_indices(User users[], int num_users);

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
    }

    if (access("mcs_indices.dat", F_OK) == -1) {
        generate_and_save_mcs_indices();
    }

    load_mcs_indices(users, num_users);

    // for (int i = 0; i < num_users; i++) {
    //     printf("User %d: MCS Index = %d\n", users[i].user_id, users[i].mcs_index);
    // }

    printf("THIS IS ROUND ROBIN ALGORITHM\n");

    printf("Enter total number of resource blocks per TTI: ");
    scanf("%d", &total_resource_blocks);

    // Perform round robin scheduling over multiple TTIs
    for (int tti = 0; tti < total_ttis; tti++) {
        int start_index = (tti * USERS_PER_TTI) % MAX_USERS;
        round_robin_scheduler(users, MAX_USERS, total_resource_blocks, start_index);
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

void assign_resource_blocks(User *user, int num_blocks) {
    user->current_resource_blocks += num_blocks;
    user->total_resource_blocks += num_blocks;
    user->times_scheduled += 1;
    user->total_data_transmitted += TBSArray[user->mcs_index][num_blocks];
}

void reset_resource_blocks(User *user) {
    user->current_resource_blocks = 0;
}

void round_robin_scheduler(User users[], int num_users, int total_resource_blocks, int start_index) {
    int blocks_per_user = total_resource_blocks / USERS_PER_TTI;
    int remaining_blocks = total_resource_blocks % USERS_PER_TTI;

    // Reset resource blocks for selected users
    for (int i = 0; i < USERS_PER_TTI; i++) {
        reset_resource_blocks(&users[(start_index + i) % num_users]);
    }

    // Distribute blocks evenly
    for (int i = 0; i < USERS_PER_TTI; i++) {
        assign_resource_blocks(&users[(start_index + i) % num_users], blocks_per_user);
    }

    // Distribute remaining blocks
    for (int i = 0; i < remaining_blocks; i++) {
        assign_resource_blocks(&users[(start_index + i) % num_users], 1);
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

void generate_and_save_mcs_indices() {
    int mcs_indices[MCS_COUNT];
    FILE *file = fopen("mcs_indices.dat", "wb");
    if (file == NULL) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    // Initialize MCS indices
    for (int i = 0; i < MCS_COUNT; i++) {
        mcs_indices[i] = i;
    }
    shuffle(mcs_indices, MCS_COUNT); // Shuffle indices

    fwrite(mcs_indices, sizeof(int), MCS_COUNT, file);
    fclose(file);
}

void load_mcs_indices(User users[], int num_users) {
    int mcs_indices[MCS_COUNT];
    FILE *file = fopen("mcs_indices.dat", "rb");
    if (file == NULL) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    fread(mcs_indices, sizeof(int), MCS_COUNT, file);
    fclose(file);

    // Assign MCS indices to users
    for (int i = 0; i < num_users; i++) {
        users[i].mcs_index = mcs_indices[i % MCS_COUNT];
    }
}

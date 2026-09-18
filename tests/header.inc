/* Header file with non-standard extension */
#define BUFFER_SIZE 1024
#define MAX_USERS 50
#define STATUS_OK 0

typedef struct {
    int id;
    char username[MAX_USERS];
    double balance;
    _Bool is_active;
} Account;
#include "tests/header.inc"

#define MULTIPLIER 2
#define TOTAL_CAPACITY (BUFFER_SIZE * MULTIPLIER)

static inline int calculate_offset(Account *acc, int step) {
    if (acc == 0) {
        return -1;
    }
    
    acc->id += step;
    acc->balance *= 1.05e-2;
    
    return acc->id;
}

int main(void) {
    // Integer literals: decimal, octal, hex, suffixes
    unsigned long a = 123456789UL;
    int octal_val = 0755;
    int hex_val = 0xDEADBEEF;
    long long big_val = 9876543210LL;

    // Floating-point literals: fractions, exponents, suffixes
    float f1 = 3.141592f;
    double f2 = .00543;
    double f3 = 42.;
    double f4 = 6.022e23;
    double f5 = 1.602E-19;

    // Characters and Strings with escapes
    char newline = '\n';
    char quote = '\'';
    char null_byte = '\0';
    char *msg = "Testing C Scanner: \"Hello, World!\"\tEscapes: \n \\";

    // Complex operators
    int x = 10, y = 20;
    x <<= 2;
    y >>= 1;
    x += (y > 5) ? (x & y) : (x ^ y);
    x |= ~y;

    Account user;
    user.id = 1;
    user.balance = 2500.75;
    user.is_active = 1;

    calculate_offset(&user, 4);

    return STATUS_OK;
}
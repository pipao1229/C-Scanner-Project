#define ARRAY_SIZE 10

void bubble_sort(int arr[], int n) {
    int i, j, temp;
    for (i = 0; i < n - 1; i++) {
        for (j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

int binary_search(int arr[], int l, int r, int x) {
    while (l <= r) {
        int m = l + (r - l) / 2;
        if (arr[m] == x) return m;
        if (arr[m] < x) l = m + 1;
        else r = m - 1;
    }
    return -1;
}

int main() {
    int numbers[ARRAY_SIZE] = {64, 34, 25, 12, 22, 11, 90, 88, 76, 50};
    bubble_sort(numbers, ARRAY_SIZE);
    int target = 22;
    int index = binary_search(numbers, 0, ARRAY_SIZE - 1, target);
    return index;
}
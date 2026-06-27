extern int get_data(int key);
extern int printf(const char *fmt, ...);

int main(void)
{
    int count = 0;
    for (int i = 0; i < 4; i++) {
        if (get_data(i))
            count++;
    }

    if (count >= 2)
        printf("[ALERT-v2] WARNING: %d buttons pressed simultaneously!\n", count);
    else if (count == 1) {
        for (int i = 0; i < 4; i++) {
            if (get_data(i))
                printf("[ALERT-v2] Button %d active\n", i + 1);
        }
    }
    return 0;
}

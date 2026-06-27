extern int get_data(int key);
extern int printf(const char *fmt, ...);

int main(void)
{
    for (int i = 0; i < 4; i++) {
        if (get_data(i))
            printf("[ALERT] Button %d pressed!\n", i + 1);
    }
    return 0;
}

extern int get_data(int key);
extern void set_led(int n, int on);

int main(void)
{
    for (int i = 0; i < 4; i++)
        set_led(i, get_data(i));
    return 0;
}

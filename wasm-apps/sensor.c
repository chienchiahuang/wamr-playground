extern int read_button(int n);
extern void set_data(int key, int value);

int main(void)
{
    for (int i = 0; i < 4; i++)
        set_data(i, read_button(i));
    return 0;
}

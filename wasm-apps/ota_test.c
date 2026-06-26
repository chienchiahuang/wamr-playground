extern int printf(const char *fmt, ...);

int main(void)
{
    printf("*** OTA UPDATE SUCCESS ***\n");
    printf("This wasm was loaded over UART!\n");
    printf("If you see this, hot-swap works!\n");
    return 0;
}

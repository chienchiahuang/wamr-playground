/* Declare printf as an import from the host (WAMR libc-builtin provides it) */
int printf(const char *fmt, ...);

int main(void)
{
    printf("Hello World from WebAssembly!\n");
    return 0;
}

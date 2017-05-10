#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

// returns the number of utf8 code points in the buffer at s
size_t utf8len(char *s)
{
    size_t len = 0;
    for (; *s; ++s) if ((*s & 0xC0) != 0x80) ++len;
    return len;
}

// returns a pointer to the beginning of the pos'th utf8 codepoint
// in the buffer at s
char *utf8index(char *s, size_t pos)
{    
    ++pos;
    for (; *s; ++s) {
        if ((*s & 0xC0) != 0x80) --pos;
        if (pos == 0) return s;
    }
    return NULL;
}

// converts codepoint indexes start and end to byte offsets in the buffer at s
void utf8slice(char *s, ssize_t *start, ssize_t *end)
{
    char *p = utf8index(s, *start);
    *start = p ? p - s : -1;
    p = utf8index(s, *end);
    *end = p ? p - s : -1;
}

// appends the utf8 string at src to dest
char *utf8cat(char *dest, char *src)
{
    return strcat(dest, src);
}

// test program
int main(int argc, char **argv)
{
    // slurp all of stdin to p, with length len
    char *p = malloc(0);
    size_t len = 0;
    while (true) {
        p = realloc(p, len + 0x10000);
        ssize_t cnt = read(STDIN_FILENO, p + len, 0x10000);
        if (cnt == -1) {
            perror("read");
            abort();
        } else if (cnt == 0) {
            break;
        } else {
            len += cnt;
        }
    }

    // do some demo operations
    printf("utf8len=%zu\n", utf8len(p));
    ssize_t start = 2, end = 3;
    utf8slice(p, &start, &end);
    printf("utf8slice[2:3]=%.*s\n", end - start, p + start);
    start = 3; end = 4;
    utf8slice(p, &start, &end);
    printf("utf8slice[3:4]=%.*s\n", end - start, p + start);

    printf("Defining \"Åke\" as Wide chars\n");

    wchar_t ws[] = L"Åke", ws2[32];
    char s[] = "Åke";

    size_t nn=mbstowcs(ws2,s,32);

    printf("\"Åke\" wlen=%zu, strlen=%zu\n",wcslen(ws),strlen(s));

    printf("Converted \"Åke\" nn=%zu, wlen=%zu\n",nn,wcslen(ws2));

    wchar_t buff[256];

    return 0;
}

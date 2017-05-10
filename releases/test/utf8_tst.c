#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <locale.h>


size_t 
utf8len(char *s)
{
    size_t len = 0;
    for (; *s; ++s) if ((*s & 0xC0) != 0x80) ++len;
    return len;
}

size_t
xmblen(const char *s) {
    mbstate_t t;
    const char *scopy = s;
    memset(&t, '\0', sizeof (t));
    size_t n = mbsrtowcs(NULL, &scopy, strlen(scopy), &t);

    return n;
}

// gcc -std=gnu99 -O2 
int
main() {

  // Set the local locale
  (void)setlocale(LC_ALL, "");

  char *input = "Åke Ökar"; // 8 character, 10 byters

  printf("%s, strlen=%zu, utf8len=%zu, xmblen=%zu\n",input,strlen(input),utf8len(input),xmblen(input));

  printf("int=%zu, long int=%zu,  size_t=%zu\n",sizeof(int),sizeof(long int), sizeof(size_t));

  exit(EXIT_SUCCESS);

}

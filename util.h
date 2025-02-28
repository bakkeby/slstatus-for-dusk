/* See LICENSE file for copyright and license details. */
#include <stdint.h>

extern char buf[1024];

#define LEN(x) (sizeof(x) / sizeof((x)[0]))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

extern char *argv0;

void warn(const char *, ...);
void die(const char *, ...);

int esnprintf(char *str, size_t size, const char *fmt, ...);
const char *bprintf(const char *fmt, ...);
const char *fmt_human(uintmax_t num, int base);
int pscanf(const char *path, const char *fmt, ...);
size_t strlcpy(char * __restrict dst, const char * __restrict src, size_t dsize);
size_t strlcat(char *dst, const char *src, size_t siz);

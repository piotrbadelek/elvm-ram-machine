#ifndef ELVM_LIBC_STDIO_H_
#define ELVM_LIBC_STDIO_H_

#include <stddef.h>
#include <stdarg.h>
#include "_raw_print.h"
#include <string.h>

#define EOF -1
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int getchar(void);
int putchar(int c);
int puts(const char* p);
int printf(const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);
int snprintf(char* buf, size_t size, const char* fmt, ...);

int puts(const char* p) {
  print_str(p);
  putchar('\n');
}

int vsnprintf(char* buf, size_t size, const char* fmt, va_list ap) {
  const char* inp;
  size_t off = 0;
  int is_overlow = 0;
  for (inp = fmt; *inp; inp++) {
    if (*inp != '%') {
      if (!is_overlow) {
        if (off + 1 >= size) {
          is_overlow = 1;
          buf[off] = 0;
        } else {
          buf[off] = *inp;
        }
      }
      off++;
      continue;
    }

    int zero_pad = 0;
    int left_align = 0;
    int width = 0;
    int prec = -1;
    int in_prec = 0;

    for (;;) {
      char c = *++inp;
      if (c == '-') {
        left_align = 1;
      } else if (c == '0' && !in_prec && width == 0) {
        zero_pad = 1;
      } else if (c == '.') {
        in_prec = 1;
        prec = 0;
      } else if (c >= '0' && c <= '9') {
        if (in_prec) {
          prec = prec * 10 + (c - '0');
        } else {
          width = width * 10 + (c - '0');
        }
      } else if (c == 'l' || c == 'h' || c == ' ' || c == '+') {
        // ignore modifier
      } else {
        break;
      }
    }

    char cur_buf[64];
    char* cur_p;
    switch (*inp) {
      case 'd':
      case 'i':
      case 'u': {
        cur_p = stringify_int(va_arg(ap, long), cur_buf + sizeof(cur_buf) - 1);
        int is_neg = 0;
        if (*cur_p == '-') {
          is_neg = 1;
          cur_p++;
        }
        int digits = strlen(cur_p);
        int target = digits;
        if (prec > target) target = prec;
        if (zero_pad && !left_align) {
          int z_target = width - (is_neg ? 1 : 0);
          if (z_target > target) target = z_target;
        }
        while (digits < target && cur_p > cur_buf + 1) {
          *--cur_p = '0';
          digits++;
        }
        if (is_neg) {
          *--cur_p = '-';
        }
        break;
      }
      case 'p':
      case 'x':
      case 'X': {
        cur_p = stringify_hex(va_arg(ap, long), cur_buf + sizeof(cur_buf) - 1);
        int hex_digits = strlen(cur_p);
        int target = hex_digits;
        if (prec > target) target = prec;
        if (zero_pad && !left_align && width > target) target = width;
        while (hex_digits < target && cur_p > cur_buf + 1) {
          *--cur_p = '0';
          hex_digits++;
        }
        break;
      }
      case 's':
        cur_p = va_arg(ap, char*);
        if (!cur_p) cur_p = "(null)";
        break;
      case 'c':
        cur_buf[0] = va_arg(ap, int);
        cur_buf[1] = 0;
        cur_p = cur_buf;
        break;
      case '%':
        cur_buf[0] = '%';
        cur_buf[1] = 0;
        cur_p = cur_buf;
        break;
      case 'f':
        (void)va_arg(ap, double);
        cur_buf[0] = '0';
        cur_buf[1] = 0;
        cur_p = cur_buf;
        break;
      default:
        cur_buf[0] = *inp;
        cur_buf[1] = 0;
        cur_p = cur_buf;
        break;
    }

    size_t len = strlen(cur_p);
    if (prec >= 0 && (*inp == 's')) {
      if (len > (size_t)prec) len = prec;
    }
    int pad = (int)width - (int)len;
    if (pad < 0) pad = 0;

    if (!left_align && pad > 0) {
      for (int k = 0; k < pad; k++) {
        if (!is_overlow) {
          if (off + 1 >= size) { is_overlow = 1; buf[off] = 0; }
          else { buf[off] = ' '; }
        }
        off++;
      }
    }

    for (size_t k = 0; k < len; k++) {
      if (!is_overlow) {
        if (off + 1 >= size) { is_overlow = 1; buf[off] = 0; }
        else { buf[off] = cur_p[k]; }
      }
      off++;
    }

    if (left_align && pad > 0) {
      for (int k = 0; k < pad; k++) {
        if (!is_overlow) {
          if (off + 1 >= size) { is_overlow = 1; buf[off] = 0; }
          else { buf[off] = ' '; }
        }
        off++;
      }
    }
  }
  if (size > 0) {
    if (off < size)
      buf[off] = 0;
    else
      buf[size - 1] = 0;
  }
  return off;
}

int vsprintf(char* buf, const char* fmt, va_list ap) {
  return vsnprintf(buf, 256, fmt, ap);
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vsnprintf(buf, size, fmt , ap);
  va_end(ap);
  return r;
}

int sprintf(char* buf, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vsprintf(buf, fmt, ap);
  va_end(ap);
  return r;
}

int vprintf(const char* fmt, va_list ap) {
  char buf[256];
  int r = vsnprintf(buf, 256, fmt, ap);
  buf[r] = 0;
  print_str(buf);
  return r;
}

int printf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vprintf(fmt, ap);
  va_end(ap);
  return r;
}

typedef char FILE;
FILE* stdin = (FILE*)1;
FILE* stdout = (FILE*)1;
FILE* stderr = (FILE*)1;

int fprintf(FILE* fp, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
}

int vfprintf(FILE* fp, const char* fmt, va_list ap) {
  return vprintf(fmt, ap);
}

int fileno(FILE* fp) {
  return 0;
}

FILE* fopen(const char* filename, const char* mode) {
  return stdin;
}

int fclose(FILE* fp) {
  return 0;
}

size_t fwrite(void* ptr, size_t s, size_t n, FILE* fp) {
  char* str = ptr;
  size_t l = (int)s * (int)n;
  for (size_t i = 0; i < l; i++)
    putchar(str[i]);
  return l;
}

int fputs(const char* s, FILE* fp) {
  print_str(s);
}

static int g_ungot = EOF;
static int eof_seen;

int fgetc(FILE* fp) {
  int r;
  if (g_ungot == EOF) {
    // A hack for whitespace, in which getchar after EOF is not
    // defined well.
    if (eof_seen)
      return EOF;
    r = getchar();
    eof_seen = r == EOF;
    return r;
  }
  r = g_ungot;
  g_ungot = EOF;
  return r;
}

int getc(FILE* fp) {
  return fgetc(fp);
}

int ungetc(int c, FILE* fp) {
  if (g_ungot == EOF)
    return g_ungot = c;
  return EOF;
}

int fputc(int c, FILE* fp) {
  return putchar(c);
}

int putc(int c, FILE* fp) {
  return putchar(c);
}

char* fgets(char* s, int size, FILE* fp) {
  if (size <= 0)
    return NULL;

  int i = 0;
  while (i < size - 1) {
    int c = fgetc(fp);
    if (c == EOF) {
      if (i) break;
      return NULL;
    }
    s[i++] = c;
    if (c == '\n')
      break;
  }
  s[i] = 0;
  return s;
}

#endif  // ELVM_LIBC_STDIO_H_

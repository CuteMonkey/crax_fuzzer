#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <execinfo.h>
#include <assert.h>
#include <signal.h>
#include <string.h>

static void* (*real_malloc)(size_t) = NULL;
static void* (*real_realloc)(void *, size_t) = NULL;
static char* (*real_strcpy)(char *, const char *) = NULL;
static char* (*real_strncpy)(char *, const char *, size_t) = NULL;
static int (*real_fprintf)(FILE *stream, const char *format, ...) = NULL;
static int (*real_vsnprintf)(char *str, size_t size, const char *format,
 va_list ap) = NULL;
static void *(*real_memcpy)(void *dest, const void *src, size_t n) = NULL;
static void *(*real_memset)(void *s, int c, size_t n) = NULL;
static size_t (*real_fread)(void *ptr, size_t size, size_t nmemb, FILE *stream)
 = NULL;
static int (*real_vfprintf)(FILE *stream, const char *format, va_list ap)
 = NULL;
static int (*real_system)(const char *command) = NULL;
static int (*real_execvp)(const char *file, char *const argv[]) = NULL;

#define MAX 1024
char buff[MAX];
static char had_fail_file = 0;
static char malloc_set = 0;

void printStack()
{
   if(real_strcpy==NULL)
      real_strcpy = dlsym(RTLD_NEXT, "strcpy");
   int j, nptrs;
   void *buffer[MAX];
   char **strings;
   nptrs = backtrace(buffer, MAX);
   strings = backtrace_symbols(buffer, nptrs);
   printf("-----printStack() start -----\n");
   for (j = 1; j < nptrs; j++) {
      printf("%s", strings[j]);
   }
   printf("-----printStack() end-----\n");
   free(strings);
}

void generateSuccess()
{
    FILE *f = fopen("/home/chengte/SCraxF/crax-fuzzer/local_verify/success.txt", "w");
    if(f == NULL) {
        perror("E: fail to opening file!\n");
        exit(1);
    }

    fclose(f);
}

void generateFail()
{
    had_fail_file = 1;

    FILE *f = fopen("/home/chengte/SCraxF/crax-fuzzer/local_verify/fail.txt", "w");
    if(f == NULL) {
        perror("E: fail to opening file!\n");
        exit(1);
    }

    fclose(f);
}


size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
   if(real_fread == NULL)
      real_fread = dlsym(RTLD_NEXT, "fread");
   printf("Testing fread (nmemb)\n");
   //if(s2e_is_symbolic(&nmemb, sizeof(nmemb))) {
   //}
   return real_fread(ptr, size, nmemb, stream); 
}

int vfprintf(FILE *stream, const char *format, va_list ap)
{
   if(real_vfprintf == NULL)
      real_vfprintf = dlsym(RTLD_NEXT, "vfprintf");
   int i, len = strlen(format);
   char result = 1, nonzero_len = 0;

   printf("Testing vfprintf (format - %s)\n", format);
   for(i = 0; i < len; i++)
   {
        nonzero_len = 1;
        if(format[i] != 'a')
        {
            result = 0;
        }
   }

   if(!had_fail_file) {
        generateFail();
   }
   if(result && nonzero_len)
   {
        generateSuccess();
   }

   return real_vfprintf(stream, format, ap);
}

void syslog(int priority, const char *format, ...)
{
   int i, len = strlen(format);
   char result = 1, nonzero_len = 0;

   printf("Testing syslog (format - %s)\n", format);
   for(i = 0; i < len; i++)
   {
        nonzero_len = 1;
        if(format[i] != 'a')
        {
            result = 0;
        }
   }

   if(!had_fail_file) {
        generateFail();
   }
   if(result && nonzero_len)
   {
        generateSuccess();
   }

   va_list args;
   va_start(args, format);
   vsyslog(priority, format, args);
   va_end(args);
}

void *memcpy(void *dest, const void *src, size_t n)
{
   if(real_memcpy == NULL)
      real_memcpy = dlsym(RTLD_NEXT, "memcpy");
   int i;
   printf("Testing memcpy (src)\n");
   //if(s2e_is_symbolic((void *)src, n)) {
   //}
   printf("Testing memcpy (n)\n");
   //if(s2e_is_symbolic(&n, sizeof(n))) {
   //}
   return real_memcpy(dest, src, n);
}

void *memset(void *s, int c, size_t n)
{
   if(real_memset == NULL)
      real_memset = dlsym(RTLD_NEXT, "memset");
   printf("Testing memset (n)\n");
   //if(s2e_is_symbolic(&n, sizeof(n))) {
   //}
   return real_memset(s, c, n);
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
   if(real_vsnprintf == NULL)
      real_vsnprintf = dlsym(RTLD_NEXT, "vsnprintf");
   int i;
   printf("Testing vsnprintf (format)\n");
   //if(s2e_is_symbolic((void *)format, strlen(format))) {
   //}
   return real_vsnprintf(str, size, format, ap);
}

int sprintf(char *str, const char *format, ...)
{
   int i;
   printf("Testing sprintf (format)\n");
   //if(s2e_is_symbolic((void *)format, strlen(format))) {
   //}
   printf("Testing sprintf (\\0)\n");
   //if(s2e_is_symbolic((void *)format+strlen(format), 1)) {
   //}
   va_list args;
   va_start(args, format);
   int r = vsprintf(str, format, args);
   va_end(args);
   return r;
}

int fprintf(FILE *stream, const char *format, ...)
{
   va_list args;
   va_start(args, format);
   int r = vfprintf(stream, format, args);
   va_end(args);
   return r;
}

char *strcpy(char *dest, const char *src)
{
   if(real_strcpy == NULL)
      real_strcpy = dlsym(RTLD_NEXT, "strcpy");
   int i, len = strlen(src);
   char result = 1, nonzero_len = 0;

   printf("Testing strcpy (src - %s)\n", src);
   for(i = 0; i < len; i++)
   {
        nonzero_len = 1;
        if(src[i] != 'a') {
            result = 0;
        }
   }

   if(!had_fail_file) {
        generateFail();
   }
   if(result && nonzero_len)
   {
        generateSuccess();
   }

   printf("Testing strcpy (\\0)\n");
   //if(s2e_is_symbolic((void *)src+len, 1)) {
   //}

   return real_strcpy(dest, src);
}

char *strncpy(char *dest, const char *src, size_t n)
{
   if(real_strncpy == NULL)
      real_strncpy = dlsym(RTLD_NEXT, "strncpy");
   int i, len = strlen(src);
   char result = 1, nonzero_len = 0;

   printf("Testing strncpy (src - %s)\n", src);
   for(i = 0; i < len; i++)
   {
        nonzero_len = 1;
        if(src[i] != 'a') {
            result = 0;
        }
   }

   if(!had_fail_file) {
        generateFail();
   }
   if(result && nonzero_len)
   {
        generateSuccess();
   }

   printf("Testing strncpy (n)\n");
   //if(s2e_is_symbolic(&n, sizeof(n))) {
   //}

   return real_strncpy(dest, src, n);
}

void *realloc(void *ptr, size_t size)
{
    if(real_realloc == NULL)
       real_realloc = dlsym(RTLD_NEXT, "realloc");
    printf("Testing realloc (size)\n");
    //if(s2e_is_symbolic(&size, sizeof(size))) {
    //}
    return real_realloc(ptr, size);
}

void *malloc(size_t size)
{
    if(real_malloc == NULL) {
        real_malloc = dlsym(RTLD_NEXT, "malloc");
        if(real_malloc != NULL) malloc_set = 1;
    }
    if(!malloc_set) printf("Warning: malloc() is still NULL.\n");

    printf("Testing malloc (size - %u)\n", size);

    if(!had_fail_file) {
        generateFail();
    }
    if(size == 0) {
        generateSuccess();
    }

    return real_malloc(size);
}

int system(const char *command)
{
   if(real_system==NULL)
      real_system = dlsym(RTLD_NEXT, "system");
   int i, len=strlen(command);
   printf("Testing system (cmd)\n");
   //if(s2e_is_symbolic((void *)command, len)) {
   //}
   return real_system(command);
}

int execvp(const char *file, char *const argv[])
{
   if(real_execvp == NULL)
      real_execvp = dlsym(RTLD_NEXT, "execvp");
   int i, len=strlen(file);
   printf("Testing execvp (file)\n");
   //if(s2e_is_symbolic((void *)file, len)) {
   //}
   return real_execvp(file, argv);
}

int execl(const char *path, const char *arg, ...)
{
   printf("Testing execl\n");
}

int execlp(const char *file, const char *arg, ...)
{
   printf("Testing execvlp\n");
}

/*int execle(const char *path, const char *arg, ..., char * const envp[])
{
   printf("Testing execle\n");
}*/

int execv(const char *path, char *const argv[])
{
   printf("Testing execv\n");
}

int execvpe(const char *file, char *const argv[], char *const envp[])
{
   printf("Testing execvpe\n");
}

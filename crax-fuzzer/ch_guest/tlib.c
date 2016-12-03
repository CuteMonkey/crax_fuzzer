#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>
#include <assert.h>
#include <signal.h>
#include "s2e.h"

static void* (*real_malloc)(size_t)=NULL;
static void* (*real_realloc)(void *, size_t)=NULL;
static char* (*real_strcpy)(char *, const char *)=NULL;
static char* (*real_strncpy)(char *, const char *, size_t)=NULL;
static int (*real_fprintf)(FILE *stream, const char *format, ...)=NULL;
static int (*real_vsnprintf)(char *str, size_t size, const char *format, va_list ap)=NULL;
static void *(*real_memcpy)(void *dest, const void *src, size_t n)=NULL;
static void *(*real_memset)(void *s, int c, size_t n)=NULL;
static size_t (*real_fread)(void *ptr, size_t size, size_t nmemb, FILE *stream)=NULL;
static int (*real_vfprintf)(FILE *stream, const char *format, va_list ap)=NULL;
static int (*real_system)(const char *command)=NULL;
static int (*real_execvp)(const char *file, char *const argv[])=NULL;

#define MAX 1024
char buff[MAX];

void printStack()
{
   if(real_strcpy==NULL)
      real_strcpy = dlsym(RTLD_NEXT, "strcpy");
   int j, nptrs;
   void *buffer[MAX];
   char **strings;
   nptrs = backtrace(buffer, MAX);
   strings = backtrace_symbols(buffer, nptrs);
   s2e_message("-----printStack() start -----");
   for (j = 1; j < nptrs; j++) {
      //puts(strings[j]);
      //real_strcpy(buff, strings[j]);
      //s2e_concretize(buff, MAX);
      s2e_message(strings[j]);
   }
   s2e_message("-----printStack() end-----");
   free(strings);
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
   if(real_fread==NULL)
      real_fread = dlsym(RTLD_NEXT, "fread");
   s2e_message("Testing fread (nmemb)");
   if(s2e_is_symbolic(&nmemb, sizeof(nmemb))) {
      printStack();
      s2e_print_path_constraint();
      s2e_print_expression("fread_n", nmemb);
      s2e_collect_args_constraint("fread_n");
   }
   return real_fread(ptr, size, nmemb, stream); 
}

int vfprintf(FILE *stream, const char *format, va_list ap)
{
   if(real_vfprintf==NULL)
      real_vfprintf = dlsym(RTLD_NEXT, "vfprintf");
   int i;
   s2e_message("Testing vfprintf (format)");
   if(s2e_is_symbolic((void *)format, strlen(format))) {
      printStack();
      s2e_print_path_constraint();
      for(i=0;i<strlen(format);i++)
        s2e_print_expression("vfprintf_format", format[i]);
      s2e_collect_args_constraint("vfprintf_format");
   }
   return real_vfprintf(stream, format, ap);
}

void syslog(int priority, const char *format, ...)
{
   int i;
   s2e_message("Testing syslog (format)");
   if(s2e_is_symbolic((void *)format, strlen(format))) {
      printStack();
      s2e_print_path_constraint();
      for(i=0;i<strlen(format);i++)
        s2e_print_expression("syslog_format", format[i]);
      s2e_collect_args_constraint("syslog_format");
   }
   va_list args;
   va_start(args, format);
   vsyslog(priority, format, args);
   va_end(args);
}

void *memcpy(void *dest, const void *src, size_t n)
{
   if(real_memcpy==NULL)
      real_memcpy = dlsym(RTLD_NEXT, "memcpy");
   int i;
   s2e_message("Testing memcpy (src)");
   if(s2e_is_symbolic((void *)src, n)) {
      printStack();
      s2e_print_path_constraint();
      for(i=0;i<n;i++)
        s2e_print_expression("memcpy_src", ((char *)src)[i]);
      s2e_collect_args_constraint("memcpy_src");
   }
   s2e_message("Testing memcpy (n)");
   if(s2e_is_symbolic(&n, sizeof(n))) {
      printStack();
      s2e_print_path_constraint();
      s2e_print_expression("memcpy_n", n);
      s2e_collect_args_constraint("memcpy_n");
   }
   return real_memcpy(dest, src, n);
}

void *memset(void *s, int c, size_t n)
{
   if(real_memset==NULL)
      real_memset = dlsym(RTLD_NEXT, "memset");
   s2e_message("Testing memset (n)");
   if(s2e_is_symbolic(&n, sizeof(n))) {
      printStack();
      s2e_print_path_constraint();
      s2e_print_expression("memset_n", n);
      s2e_collect_args_constraint("memset_n");
   }
   return real_memset(s, c, n);
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
   if(real_vsnprintf==NULL)
      real_vsnprintf = dlsym(RTLD_NEXT, "vsnprintf");
   int i;
   s2e_message("Testing vsnprintf (format)");
   if(s2e_is_symbolic((void *)format, strlen(format))) {
      s2e_print_path_constraint();
      for(i=0;i<strlen(format);i++)
        s2e_print_expression("vsnprintf_format", format[i]);
      s2e_collect_args_constraint("vsnprintf_format");
   }
   return real_vsnprintf(str, size, format, ap);
}

int sprintf(char *str, const char *format, ...)
{
   int i;
   s2e_message("Testing sprintf (format)");
   if(s2e_is_symbolic((void *)format, strlen(format))) {
      s2e_print_path_constraint();
      for(i=0;i<strlen(format);i++)
        s2e_print_expression("sprintf_format", format[i]);
      s2e_collect_args_constraint("sprintf_format");
   }
   s2e_message("Testing sprintf (\\0)");
   if(s2e_is_symbolic((void *)format+strlen(format), 1)) {
        s2e_print_path_constraint();
        s2e_print_expression("sprintf_0", format[strlen(format)]);
        s2e_collect_args_constraint("sprintf_0");
   }
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
   if(real_strcpy==NULL)
      real_strcpy = dlsym(RTLD_NEXT, "strcpy");
   int i, len=strlen(src);
   s2e_message("Testing strcpy (src)");
   if(s2e_is_symbolic((void *)src, len)) {
      printStack();
      s2e_print_path_constraint();
      for(i=0;i<len;i++)
        s2e_print_expression("strcpy_src", src[i]);
      s2e_collect_args_constraint("strcpy_src");
   }
   s2e_message("Testing strcpy (\\0)");
   if(s2e_is_symbolic((void *)src+len, 1)) {
      printStack();
      s2e_print_path_constraint();
      s2e_print_expression("strcpy_0", src[len]);
      s2e_collect_args_constraint("strcpy_0");
   }
   return real_strcpy(dest, src);
}

char *strncpy(char *dest, const char *src, size_t n)
{
   if(real_strncpy==NULL)
      real_strncpy = dlsym(RTLD_NEXT, "strncpy");
   int i, len=strlen(src);
   s2e_message("Testing strncpy (src)");
   if(s2e_is_symbolic((void *)src, len)) {
      printStack();
      s2e_print_path_constraint();
      for(i=0;i<len;i++)
        s2e_print_expression("strncpy_src", src[i]);
      s2e_collect_args_constraint("strncpy_src");
   }
   s2e_message("Testing strncpy (n)");
   if(s2e_is_symbolic(&n, sizeof(n))) {
      printStack();
      s2e_print_path_constraint();
      s2e_print_expression("strncpy_n", n);
      s2e_collect_args_constraint("strncpy_n");
   }
   return real_strncpy(dest, src, n);
}

void* realloc(void *ptr, size_t size)
{
    if(real_realloc==NULL)
       real_realloc = dlsym(RTLD_NEXT, "realloc");
    s2e_message("Testing realloc (size)");
    if(s2e_is_symbolic(&size, sizeof(size))) {
       printStack();
       s2e_print_path_constraint();
       s2e_print_expression("realloc_size", size);
       s2e_collect_args_constraint("realloc_size");
    }
    return real_realloc(ptr, size);
}

void *malloc(size_t size)
{
    if(real_malloc==NULL)
        real_malloc = dlsym(RTLD_NEXT, "malloc");
    s2e_message("Testing malloc (size)");
    if(s2e_is_symbolic(&size, sizeof(size))) {
       printStack();
       s2e_print_path_constraint();
       s2e_print_expression("malloc_size", size);
       s2e_collect_args_constraint("malloc_size");
    }
    return real_malloc(size);
}

int system(const char *command)
{
   if(real_system==NULL)
      real_system = dlsym(RTLD_NEXT, "system");
   int i, len=strlen(command);
   s2e_message("Testing system (cmd)");
   if(s2e_is_symbolic((void *)command, len)) {
      printStack();
      s2e_print_path_constraint();
      for(i=0;i<len;i++)
         s2e_print_expression("system_cmd", command[i]);
      s2e_collect_args_constraint("system_cmd");
   }
   return real_system(command);
}

int execvp(const char *file, char *const argv[])
{
   if(real_execvp==NULL)
      real_execvp = dlsym(RTLD_NEXT, "execvp");
   int i, len=strlen(file);
   s2e_message("Testing execvp (file)");
   if(s2e_is_symbolic((void *)file, len)) {
      printStack();
      s2e_print_path_constraint();
      for(i=0;i<len;i++)
         s2e_print_expression("execvp_file", file[i]);
      s2e_collect_args_constraint("execvp_file");
   }
   return real_execvp(file, argv);
}

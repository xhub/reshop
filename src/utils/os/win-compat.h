#ifndef WIN_COMPAT_H
#define WIN_COMPAT_H

// IWYU pragma: always_keep

#if defined(_WIN32) && !defined(__CYGWIN__)

#define CLEANUP_FNS_HAVE_DECL

void cleanup_vrepr(void);
void cleanup_opcode_diff(void);
void cleanup_snans_funcs(void);
void cleanup_path(void);
void cleanup_pathvi(void);
void cleanup_gams(void);

char *strndup(const char *str, size_t chars);
char *win_gettmpdir(void);
unsigned win_strerror(unsigned sz, char buf[VMT(static sz)], const char **msg);

#endif

#endif

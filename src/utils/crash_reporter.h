#ifndef RHP_CRASH_REPORTER
#define RHP_CRASH_REPORTER

#ifdef _WIN32
#define CODE_TYPE unsigned long
#define CODE_TYPE_FMT "0x%lX"
#else
#define CODE_TYPE int
#define CODE_TYPE_FMT "%d"
#endif

void crash_handler(CODE_TYPE sigcode);

#endif

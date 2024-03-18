#ifndef TEST_GAMS_UTILS_H
#define TEST_GAMS_UTILS_H

#include <stdbool.h>
void setup_gams(void);
bool gams_skip_solver(const char *slv, const char *testname);

#endif

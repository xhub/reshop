#ifndef RHP_BENCH_UTILS
#define RHP_BENCH_UTILS


struct rhp_mdl* bench_getgamssolver(const char *solver_name);
struct rhp_mdl* bench_init(double tol);
void setup_gams(void);

#endif

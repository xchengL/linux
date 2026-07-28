/* xcdbg.h - XC debug library header file */

#ifndef __INCxcdgb
#define __INCxcdgb

#ifdef __cplusplus
extern "C" {
#endif

void xc_bench_data_init(char *task_name);
bool xc_bench_data_is_inited(void);
uint64_t xc_timer_cnt_get(bool is_new);
void xc_bench_data_record(uint64_t old, uint64_t new);
void xc_bench_data_output(void);

#ifdef __cplusplus
}
#endif

#endif

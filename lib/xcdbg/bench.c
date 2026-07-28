#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/printk.h>

struct xc_bench_data {
	uint64_t max;
	uint64_t min;
	uint64_t sum;
	uint64_t cnt;
	uint64_t switch_cnt;
	struct task_struct *tid;
	atomic_t switch_ref;
};

static struct xc_bench_data g_data;

static uint64_t xc_arm_cntvct_get(void)
{
	uint64_t cnt;
	__asm__ __volatile__("ISB");
	__asm__ __volatile__("MRS %0, CNTVCT_EL0" : "=r"(cnt) :);
	return cnt;
}

static uint64_t xc_arm_cntvct_freq(void)
{
	uint64_t cnt;
	__asm__ __volatile__("MRS %0, CNTFRQ_EL0" : "=r"(cnt) :);
	return cnt;
}

static uint64_t xc_timer_freq(void)
{
	return xc_arm_cntvct_freq();
}

uint64_t xc_timer_cnt_get(bool is_new)
{
	uint64_t cnt;
	if (!is_new) {
		atomic_set(&g_data.switch_ref, 0);
	}
	cnt = xc_arm_cntvct_get();
	if (is_new) {
		if (atomic_read(&g_data.switch_ref) >
		    0) { /* task is switched */
			g_data.switch_cnt += 1;
		}
	}
	return cnt;
}

EXPORT_SYMBOL(xc_timer_cnt_get);

static struct task_struct *xc_self_is_spec(char *p_name)
{
	char name[TASK_COMM_LEN] = { 0 };
	struct task_struct *tid = current;

	get_task_comm(name, tid);
	if (strncmp(name, p_name, TASK_COMM_LEN) == 0) {
		return tid;
	}

	return NULL;
}

void xc_bench_data_init(char *task_name)
{
	if (g_data.tid == NULL) {
		g_data.max = 0;
		g_data.min = 0;
		g_data.sum = 0;
		g_data.cnt = 0;
		g_data.switch_cnt = 0;
		atomic_set(&g_data.switch_ref, 0);
		g_data.tid = xc_self_is_spec(task_name);
		if (g_data.tid != NULL) {
			printk("%s: tid %p, %s, timer freq %lld, \n", __func__,
			       g_data.tid, task_name, xc_timer_freq());
		}
	}
}

EXPORT_SYMBOL(xc_bench_data_init);

bool xc_bench_data_is_inited(void)
{
	if (g_data.tid == current) {
		return true;
	} else {
		return false;
	}
}

EXPORT_SYMBOL(xc_bench_data_is_inited);

void xc_bench_data_record(uint64_t old, uint64_t new)
{
	struct xc_bench_data *p_data = &g_data;
	uint64_t delta;

	if (new < old) {
		return; /* invalid data */
	}

	delta = new - old;
	if ((delta < p_data->min) || (p_data->min == 0)) {
		p_data->min = delta;
	}

	if (delta > p_data->max) {
		p_data->max = delta;
	}

	if ((U64_MAX - delta) > p_data->sum) {
		p_data->sum += delta;
		p_data->cnt += 1;
	} else { /* avoid overflow */
		p_data->sum = p_data->sum / p_data->cnt + delta;
		p_data->cnt = 1 + 1;
	}
}

EXPORT_SYMBOL(xc_bench_data_record);

static void xc_data_get_ms(uint64_t cycles, uint64_t freq, uint64_t *p_ms,
			   uint64_t *p_us_frac)
{
	uint64_t secs = cycles / freq;
	uint64_t rem = cycles % freq;
	uint64_t total_us = secs * 1000000ULL + (rem * 1000000ULL) / freq;

	*p_ms = total_us / 1000;
	*p_us_frac = total_us % 1000;
}

void xc_bench_data_output(void)
{
	uint64_t freq;
	struct xc_bench_data *p_data = &g_data;
	uint64_t max_ms, max_us, min_ms, min_us, avg_ms, avg_us;

	if (p_data->cnt < 10000) {
		return;
	}

	freq = xc_timer_freq();

	xc_data_get_ms(p_data->max, freq, &max_ms, &max_us);
	xc_data_get_ms(p_data->min, freq, &min_ms, &min_us);
	xc_data_get_ms(p_data->sum / p_data->cnt, freq, &avg_ms, &avg_us);

	printk("Switch(%lld), Max(%lld, %llu.%03llums), Min(%lld, %llu.%03llums), Avg(%lld, "
	       "%llu.%03llums)\n",
	       p_data->switch_cnt, p_data->max, max_ms, max_us, p_data->min,
	       min_ms, min_us, p_data->sum / p_data->cnt, avg_ms, avg_us);

	p_data->cnt = 0;
	p_data->sum = 0;
	p_data->min = 0;
	p_data->max = 0;
	p_data->switch_cnt = 0;
}

EXPORT_SYMBOL(xc_bench_data_output);

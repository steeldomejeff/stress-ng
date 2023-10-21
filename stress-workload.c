/*
 * Copyright (C)      2023 Colin Ian King.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "stress-ng.h"
#include "core-builtin.h"
#include "core-put.h"

#define NUM_BUCKETS	(20)

#define STRESS_WORKLOAD_DIST_RANDOM1	(1)
#define STRESS_WORKLOAD_DIST_RANDOM2	(2)
#define STRESS_WORKLOAD_DIST_RANDOM3	(3)
#define STRESS_WORKLOAD_DIST_CLUSTER	(4)

typedef struct {
	uint32_t when_us;
} stress_workload_t;

typedef struct {
	const char *name;
	const int type;
} stress_workload_dist_t;

typedef struct {
	double width;
	uint64_t bucket[NUM_BUCKETS];
	uint64_t overflow;
} stress_workload_bucket_t;

static const stress_help_t help[] = {
	{ NULL,	"workload N",		"start N workers that exercise a mix of scheduling loads" },
	{ NULL,	"workload-ops N",	"stop after N workload bogo operations" },
	{ NULL, "workload-load P",	"percentage load P per workload time slice" },
	{ NULL,	"workload-quanta-us N",	"max duration of each quanta work item in microseconds" },
	{ NULL, "workload-slice-us N",	"duration of workload time load in microseconds" },
	{ NULL,	"workload-dist type",	"workload distribution type [random1, random2, random3, cluster]" },
	{ NULL,	NULL,			NULL }
};

static const stress_workload_dist_t workload_dist[] = {
	{ "random1",	STRESS_WORKLOAD_DIST_RANDOM1 },
	{ "random2",	STRESS_WORKLOAD_DIST_RANDOM2 },
	{ "random3",	STRESS_WORKLOAD_DIST_RANDOM3 },
	{ "cluster",	STRESS_WORKLOAD_DIST_CLUSTER },
};

static int stress_set_workload_dist(const char *opt)
{
	size_t i;

	for (i = 0; i < SIZEOF_ARRAY(workload_dist); i++) {
		if (strcmp(opt, workload_dist[i].name) == 0)
			return stress_set_setting("workload-dist", TYPE_ID_INT, &workload_dist[i].type);
	}

	(void)fprintf(stderr, "workload-dist must be one of:");
	for (i = 0; i < SIZEOF_ARRAY(workload_dist); i++) {
		(void)fprintf(stderr, " %s", workload_dist[i].name);
	}
	(void)fprintf(stderr, "\n");
	return -1;
}

/*
 *  stress_set_workload_load()
 *	set workload load (%)
 */
static int stress_set_workload_load(const char *opt)
{
	uint32_t workload_load;

	workload_load = stress_get_uint32(opt);
	stress_check_range("workload-load", (uint64_t)workload_load, 1, 100);
	return stress_set_setting("workload-load", TYPE_ID_UINT32, &workload_load);
}

/*
 *  stress_set_workload_quanta_us()
 *	set duration of each work quanta in microseconds
 */
static int stress_set_workload_quanta_us(const char *opt)
{
	uint32_t workload_quanta_us;

	workload_quanta_us = stress_get_uint32(opt);
	stress_check_range("workload-quanta-us", (uint64_t)workload_quanta_us,
		1, 10000000);
	return stress_set_setting("workload-quanta-us", TYPE_ID_UINT32, &workload_quanta_us);
}

/*
 *  stress_set_workload_slice_us()
 *	set duration of each work slice in microseconds
 */
static int stress_set_workload_slice_us(const char *opt)
{
	uint32_t workload_slice_us;

	workload_slice_us = stress_get_uint32(opt);
	stress_check_range("workload-slice-us", (uint64_t)workload_slice_us,
		1, 10000000);
	return stress_set_setting("workload-slice-us", TYPE_ID_UINT32, &workload_slice_us);
}

static const stress_opt_set_func_t opt_set_funcs[] = {
	{ OPT_workload_load,		stress_set_workload_load },
	{ OPT_workload_quanta_us,	stress_set_workload_quanta_us },
	{ OPT_workload_slice_us,	stress_set_workload_slice_us },
	{ OPT_workload_dist,		stress_set_workload_dist },
	{ 0,				NULL }
};

static void stress_workload_nop(void)
{
	register int i;

	for (i = 0; i < 16; i++) {
		stress_asm_nop();
		stress_asm_nop();
		stress_asm_nop();
		stress_asm_nop();
		stress_asm_nop();
		stress_asm_nop();
		stress_asm_nop();
		stress_asm_nop();
		stress_asm_nop();
		stress_asm_nop();
		stress_asm_nop();
		stress_asm_nop();
		stress_asm_nop();
		stress_asm_nop();
		stress_asm_nop();
		stress_asm_nop();
	}
}

static void stress_workload_math(const double v1, const double v2)
{
	double r;

	r = sqrt(v1) + hypot(v1, v1 + v2);
	r += sqrt(v2) + hypot(v2, v1 + v2);
	r += sqrt(v1 + v2);

	stress_double_put(r);
}

static inline void stress_workload_waste_time(
	const double run_duration_sec,
	void *buffer,
	const size_t buffer_len)
{
	const double t_end = stress_time_now() + run_duration_sec;
	double t;
	static volatile uint64_t val = 0;

	switch (stress_mwc8modn(8)) {
	case 0:
		while (stress_time_now() < t_end)
			;
		break;
	case 1:
		while (stress_time_now() < t_end)
			stress_workload_nop();
		break;
	case 2:
		while (stress_time_now() < t_end)
			shim_memset(buffer, stress_mwc8(), buffer_len);
		break;
	case 3:
		while (stress_time_now() < t_end)
			shim_memmove(buffer, buffer + 1, buffer_len - 1);
		break;
	case 4:
		while ((t = stress_time_now()) < t_end)
			stress_workload_math(t, t_end);
	case 5:
		while ((t = stress_time_now()) < t_end)
			val++;
		break;
	case 6:
		while (stress_time_now() < t_end)
			(void)stress_mwc64();
		break;
	case 7:
	default:
		while ((t = stress_time_now()) < t_end) {
			switch (stress_mwc8modn(7)) {
			case 0:
				break;
			case 1:
				stress_workload_nop();
				break;
			case 2:
				shim_memset(buffer, stress_mwc8(), buffer_len);
				break;
			case 3:
				shim_memmove(buffer, buffer + 1, buffer_len - 1);
				break;
			case 4:
				while ((t = stress_time_now()) < t_end)
					val++;
				break;
			case 5:
				(void)stress_mwc64();
				break;
			case 6:
			default:
				stress_workload_math(t, t_end);
				break;
			}
		}
		break;
	}
}

static void stress_workload_bucket_init(stress_workload_bucket_t *bucket, const double width)
{
	size_t i;

	for (i = 0; i < SIZEOF_ARRAY(bucket->bucket); i++)
		bucket->bucket[i] = 0;
	bucket->width = width / SIZEOF_ARRAY(bucket->bucket);
	bucket->overflow = 0;
}

static void stress_workload_bucket_account(stress_workload_bucket_t *bucket, const double value)
{
	ssize_t i;

	i = (ssize_t)(value / bucket->width);
	if (i < 0)
		i = 0;
	if (i < (ssize_t)SIZEOF_ARRAY(bucket->bucket))
		bucket->bucket[i]++;
	else
		bucket->overflow++;
}

static void stress_workload_bucket_report(stress_workload_bucket_t *bucket)
{
	size_t i;
	int width1, width2;
	char buf[64];
	uint64_t total;

	(void)snprintf(buf, sizeof(buf), "%" PRIu64,
		(uint64_t)((SIZEOF_ARRAY(bucket->bucket) + 1) * bucket->width));
	width1 = (int)strlen(buf);
	if (width1 < 7)
		width1 = 7;

	total = bucket->overflow;
	for (i = 0; i < SIZEOF_ARRAY(bucket->bucket); i++)
		total += bucket->bucket[i];
	(void)snprintf(buf, sizeof(buf), "%" PRIu64, total);
	width2 = (int)strlen(buf);
	if (width2 < 7)
		width2 = 7;

	pr_block_begin();
	pr_dbg("distribution of workload start time in workload slice:\n");
	pr_dbg("%-*s %*s %4s\n",
		(width1 * 2) + 4, "start time (us)",
		width2, "count", "%");
	for (i = 0; i < SIZEOF_ARRAY(bucket->bucket); i++) {
		pr_dbg("%*" PRIu64 " .. %*" PRIu64 " %*" PRIu64 " %4.1f\n",
			width1, (uint64_t)(i * bucket->width),
			width1, (uint64_t)((i + 1) * bucket->width) - 1,
			width2, bucket->bucket[i],
			(double)100.0 * (double)bucket->bucket[i] / (double)total);
	}
	pr_dbg("%*" PRIu64 " .. %*s %*" PRIu64 " %4.1f\n",
		width1, (uint64_t)(i * bucket->width),
		width1, "",
		width2, bucket->overflow,
		(double)100.0 * (double)bucket->overflow / (double)total);
	pr_block_end();
}

static int stress_workload_cmp(const void *p1, const void *p2)
{
	stress_workload_t *w1 = (stress_workload_t *)p1;
	stress_workload_t *w2 = (stress_workload_t *)p2;

	register uint32_t when1 = w1->when_us;
	register uint32_t when2 = w2->when_us;

	if (when1 < when2)
		return -1;
	else if (when1 > when2)
		return 1;
	else
		return 0;
}

static int stress_workload_exercise(
	const stress_args_t *args,
	const uint32_t workload_load,
	const uint32_t workload_slice_us,
	const uint32_t workload_quanta_us,
	const uint32_t max_quanta,
	const int workload_dist,
	stress_workload_t *workload,
	stress_workload_bucket_t *slice_offset_bucket,
	void *buffer,
	const size_t buffer_len)
{
	size_t i;
	const double scale_us_to_sec = 1.0 / STRESS_DBL_MICROSECOND;
	double t_begin = stress_time_now();
	double t_end = t_begin + ((double)workload_slice_us * scale_us_to_sec);
	double sleep_duration_ns, run_duration_sec;
	uint32_t offset;

	run_duration_sec = (double)workload_quanta_us * scale_us_to_sec * ((double)workload_load / 100.0);

	switch (workload_dist) {
	case STRESS_WORKLOAD_DIST_RANDOM1:
		for (i = 0; i < max_quanta; i++)
			workload[i].when_us = stress_mwc32modn(workload_slice_us - workload_quanta_us);
		break;
	case STRESS_WORKLOAD_DIST_RANDOM2:
		for (i = 0; i < max_quanta; i++)
			workload[i].when_us = (stress_mwc32modn(workload_slice_us - workload_quanta_us) +
					       stress_mwc32modn(workload_slice_us - workload_quanta_us)) / 2;
		break;
	case STRESS_WORKLOAD_DIST_RANDOM3:
		for (i = 0; i < max_quanta; i++) {
			workload[i].when_us = (stress_mwc32modn(workload_slice_us - workload_quanta_us) +
					       stress_mwc32modn(workload_slice_us - workload_quanta_us) +
					       stress_mwc32modn(workload_slice_us - workload_quanta_us)) / 3;
		}
		break;
	case STRESS_WORKLOAD_DIST_CLUSTER:
		offset = stress_mwc32modn(workload_slice_us / 2);
		for (i = 0; i < (max_quanta * 2) / 3; i++)
			workload[i].when_us = stress_mwc32modn(workload_quanta_us) + offset;
		for (; i < max_quanta; i++)
			workload[i].when_us = stress_mwc32modn(workload_slice_us - workload_quanta_us);
		break;
	}

	qsort(workload, max_quanta, sizeof(*workload), stress_workload_cmp);

	t_begin = stress_time_now();
	t_end = t_begin + ((double)workload_slice_us * scale_us_to_sec);

	for (i = 0; i < max_quanta; i++) {
		const double run_when = t_begin + (workload[i].when_us * scale_us_to_sec);

		sleep_duration_ns = (run_when - stress_time_now()) * STRESS_DBL_NANOSECOND;
		if (sleep_duration_ns > 10000.0) {
			shim_nanosleep_uint64((uint64_t)sleep_duration_ns);
		}
		stress_workload_bucket_account(slice_offset_bucket, STRESS_DBL_MICROSECOND * (stress_time_now() - t_begin));
		if (run_duration_sec > 0.0)
			stress_workload_waste_time(run_duration_sec, buffer, buffer_len);
		stress_bogo_inc(args);
	}
	sleep_duration_ns = (t_end - stress_time_now()) * STRESS_DBL_NANOSECOND;
	if (sleep_duration_ns > 100.0)
		shim_nanosleep_uint64((uint64_t)sleep_duration_ns);

	return EXIT_SUCCESS;
}

static int stress_workload(const stress_args_t *args)
{
	uint32_t workload_load = 30;
	uint32_t workload_slice_us = 100000;	/* 1/10th second */
	uint32_t workload_quanta_us = 1000;	/* 1/1000th second */
	uint32_t max_quanta;
	int workload_dist = STRESS_WORKLOAD_DIST_CLUSTER;
	stress_workload_t *workload;
	void *buffer;
	const size_t buffer_len = MB;
	stress_workload_bucket_t slice_offset_bucket;

	(void)stress_get_setting("workload-load", &workload_load);
	(void)stress_get_setting("workload-slice-us", &workload_slice_us);
	(void)stress_get_setting("workload-quanta-us", &workload_quanta_us);
	(void)stress_get_setting("workload-dist", &workload_dist);

	if (workload_quanta_us > workload_slice_us) {
		pr_err("%s: workload-quanta-us %" PRIu32 " must be less "
			"than workload-slice-us %" PRIu32 "\n",
			args->name, workload_quanta_us, workload_slice_us);
		return EXIT_FAILURE;
	}

	max_quanta = workload_slice_us / workload_quanta_us;
	if (max_quanta < 1)
		max_quanta = 1;

	workload = calloc(max_quanta, sizeof(*workload));
	if (!workload) {
		pr_inf_skip("%s: cannot allocate %" PRIu32 " scheduler workload timings, "
			"skipping stressor\n", args->name, max_quanta);
		return EXIT_NO_RESOURCE;
	}
	buffer = mmap(NULL, buffer_len, PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (buffer == MAP_FAILED) {
		pr_inf_skip("%s: cannot mmap %zd sized buffer, "
			"skipping stressor\n", args->name, buffer_len);
		free(workload);
		return EXIT_NO_RESOURCE;
	}

	stress_workload_bucket_init(&slice_offset_bucket, (double)workload_slice_us);

	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	do {
		stress_workload_exercise(args, workload_load,
					workload_slice_us,
					workload_quanta_us,
					max_quanta, workload_dist,
					workload,
					&slice_offset_bucket,
					buffer, buffer_len);
	} while (stress_continue(args));

	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	if (args->instance == 0)
		stress_workload_bucket_report(&slice_offset_bucket);

	(void)munmap(buffer, buffer_len);
	free(workload);

	return EXIT_SUCCESS;
}

stressor_info_t stress_workload_info = {
	.stressor = stress_workload,
	.class = CLASS_INTERRUPT | CLASS_SCHEDULER | CLASS_OS,
	.opt_set_funcs = opt_set_funcs,
	.verify = VERIFY_ALWAYS,
	.help = help
};

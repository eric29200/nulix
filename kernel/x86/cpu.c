#include <x86/cpu.h>
#include <string.h>
#include <stdio.h>

#define NR_CPUS			32

#define X86_VENDOR_INTEL	0
#define X86_VENDOR_AMD		2
#define X86_VENDOR_UNKNOWN	0xFF

/*
 * X86 CPU informations.
 */
struct cpuinfo_x86 {
	uint8_t			x86;
	uint8_t			x86_vendor;
	uint8_t			x86_model;
	uint8_t			x86_mask;
	uint32_t		x86_capability;
	char			x86_vendor_id[16];
	char			x86_model_id[64];
	int			x86_cache_size;
};

/* CPUs */
static struct cpuinfo_x86 boot_cpu_data = { 0 };
static struct cpuinfo_x86 cpu_data[NR_CPUS];
static uint32_t cpu_online_map = 0;

/* CPU frequency (defined in drivers/char/pit.c)*/
extern uint32_t cpu_khz;

/*
 * Retrieve informations about a cpu.
 */
static void cpuid(int op, int *eax, int *ebx, int *ecx, int *edx)
{
	__asm__("cpuid" : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx) : "0" (op));
}

/*
 * Retrieve informations about a cpu.
 */
static uint32_t cpuid_eax(uint32_t op)
{
	uint32_t eax;
	__asm__("cpuid" : "=a" (eax) : "0" (op) : "bx", "cx", "dx");
	return eax;
}

/*
 * Retrieve informations about a cpu.
 */
static uint32_t cpuid_ecx(uint32_t op)
{
	uint32_t eax, ecx;
	__asm__("cpuid" : "=a" (eax), "=c" (ecx) : "0" (op) : "bx", "dx" );
	return ecx;
}

/*
 * Get CPU vendor.
 */
static void get_cpu_vendor(struct cpuinfo_x86 *c)
{
	if (!strcmp(c->x86_vendor_id, "GenuineIntel"))
		c->x86_vendor = X86_VENDOR_INTEL;
	else if (!strcmp(c->x86_vendor_id, "AuthenticAMD"))
		c->x86_vendor = X86_VENDOR_AMD;
	else
		c->x86_vendor = X86_VENDOR_UNKNOWN;
}

/*
 * Get model name.
 */
static void get_model_name(struct cpuinfo_x86 *c)
{
	int *v = (int *) c->x86_model_id;

	cpuid(0x80000002, &v[0], &v[1], &v[2], &v[3]);
	cpuid(0x80000003, &v[4], &v[5], &v[6], &v[7]);
	cpuid(0x80000004, &v[8], &v[9], &v[10], &v[11]);
	c->x86_model_id[48] = 0;
}

/*
 * Init cache informations.
 */
static void init_cache_info_amd(struct cpuinfo_x86 *c)
{
	int dummy, ecx, edx;
	uint32_t n, l2size;

	n = cpuid_eax(0x80000000);

	/* L1 cache */
	if (n >= 0x80000005){
		cpuid(0x80000005, &dummy, &dummy, &ecx, &edx);
		c->x86_cache_size = (ecx >> 24) + (edx >> 24);
	}

	if (n < 0x80000006)
		return;

	/* L2 cache */
	ecx = cpuid_ecx(0x80000006);
	l2size = ecx >> 16;

	if (l2size == 0)
		return;

	c->x86_cache_size = l2size;
}

/*
 * Init Intel cpu.
 */
static void init_intel(struct cpuinfo_x86 *c)
{
	/* get model name */
	get_model_name(c);
}

/*
 * Init AMD cpu.
 */
static void init_amd(struct cpuinfo_x86 *c)
{
	/* get model name */
	get_model_name(c);

	/* init cache info */
	init_cache_info_amd(c);
}

/*
 * Get CPU informations.
 */
size_t get_cpuinfo(char *page)
{
	static char *x86_cap_flags[] = {
		"fpu", "vme", "de", "pse", "tsc", "msr", "pae", "mce",
		"cx8", "apic", "10", "sep", "mtrr", "pge", "mca", "cmov",
		"pat", "pse36", "pn", "clflush", "20", "dts", "acpi", "mmx",
		"fxsr", "sse", "sse2", "ss", "28", "tm", "ia64", "31"
	};
	struct cpuinfo_x86 *c;
	char *p = page;
	int n, i;

	/* for each cpu */
	for (n = 0, c = cpu_data; n < NR_CPUS; n++, c++) {
		/* skip offline CPU */
		if (!(cpu_online_map & ( 1 <<n)))
			continue;

		/* print processor informations */
		p += sprintf(p,
			"processor\t: %d\n"
			"vendor_id\t: %s\n"
			"cpu family\t: %d\n"
			"model\t\t: %d\n"
			"model name\t: %s\n",
			n,
			c->x86_vendor_id[0] ? c->x86_vendor_id : "unknown",
			c->x86,
			c->x86_model,
			c->x86_model_id[0] ? c->x86_model_id : "unknown");

		/* print stepping informations */
		if (c->x86_mask)
			p += sprintf(p, "stepping\t: %d\n", c->x86_mask);
		else
			p += sprintf(p, "stepping\t: unknown\n");

		/* print CPU speed */
		if (c->x86_capability & X86_FEATURE_TSC)
			p += sprintf(p, "cpu MHz\t\t: %lu.%03lu\n", cpu_khz / 1000, cpu_khz % 1000);

		/* cache size */
		if (c->x86_cache_size >= 0)
			p += sprintf(p, "cache size\t: %d KB\n", c->x86_cache_size);

		/* print flags */
		p += sprintf(p, "flags\t\t:");
		for (i = 0 ; i < 32; i++)
			if (c->x86_capability & (1 << i))
				p += sprintf(p, " %s", x86_cap_flags[i]);
		p += sprintf(p, "\n");
	}

	return p - page;
}

/*
 * Init boot CPU.
 */
static void init_boot_cpu()
{
	struct cpuinfo_x86 *c = &boot_cpu_data;
	int eax, ebx, ecx, edx;

	/* get vendor */
	cpuid(0, &eax, &ebx, &ecx, &edx);
	memcpy(c->x86_vendor_id, &ebx, 4);
	memcpy(c->x86_vendor_id + 4, &edx, 4);
	memcpy(c->x86_vendor_id + 8, &ecx, 4);

	/* get model and family */
	cpuid(1, &eax, &ebx, &ecx, &edx);
	c->x86_model = (eax >> 4) & 0x0F;
	c->x86 = (eax >> 8) & 0x0F;
	c->x86_mask = eax & 0x0F;
	c->x86_capability = edx;
}

/*
 * Init CPU.
 */
void init_cpu()
{
	struct cpuinfo_x86 *c;
	size_t n;

	/* init boot cpu */
	init_boot_cpu();

	/* only one CPU */
	cpu_online_map |= (1 << 0);

	/* for each CPU */
	for (n = 0, c = cpu_data; n < NR_CPUS; n++, c++) {
		/* skip offline CPU */
		if (!(cpu_online_map & ( 1 <<n)))
			continue;

		/* set CPU */
		*c = boot_cpu_data;

		/* get vendor */
		get_cpu_vendor(c);

		/* init specific vendor */
		switch (c->x86_vendor) {
			case X86_VENDOR_INTEL:
				init_intel(c);
				break;
			case X86_VENDOR_AMD:
				init_amd(c);
				break;
			case X86_VENDOR_UNKNOWN:
				break;
		}
	}
}
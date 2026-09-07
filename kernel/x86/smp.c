#include <x86/smp.h>
#include <mm/paging.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>

/* global variable */
static int smp_found_config = 0;
static struct intel_mp_floating *mpf_found = NULL;
static uint32_t mp_lapic_addr = 0;

/*
 * Compute checksum.
 */
static int mpf_checksum(uint8_t *mp, int len)
{
	int sum = 0;

	while (len--)
		sum += *mp++;

	return sum & 0xFF;
}

/*
 * Scan SMP configuation.
 */
static int smp_scan_config(uint32_t base, size_t len)
{
	uint32_t *bp = (uint32_t *) __va(base);
	struct intel_mp_floating *mpf;

	if (sizeof(struct intel_mp_floating) != 16)
		panic("smp_scan_config: MPF size != 16\n");

	while (len > 0) {
		mpf = (struct intel_mp_floating *) bp;
		if (*bp == SMP_MAGIC_IDENT
			&& mpf->mpf_length == 1
			&& mpf_checksum((uint8_t *) bp, 16) == 0
			&& (mpf->mpf_specification == 1 || mpf->mpf_specification == 4)) {
			smp_found_config = 1;
			printf("found SMP MP-table at %08lx\n", __pa(mpf));
			mpf_found = mpf;
			return 1;
		}

		bp += 4;
		len -= 16;
	}

	return 0;
}

/*
 * Read SMP configuration.
 */
static int smp_read_mpc(struct mp_config_table *mpc)
{
	char oem[16], prod[14];

	/* check signature */
	if (memcmp(mpc->mpc_signature, MPC_SIGNATURE, 4) != 0) {
		panic("smp_read_mpc: bad signature [%c%c%c%c]\n",
			mpc->mpc_signature[0],
			mpc->mpc_signature[1],
			mpc->mpc_signature[2],
			mpc->mpc_signature[3]);
		return 0;
	}

	/* compute checksum */
	if (mpf_checksum((uint8_t *) mpc, mpc->mpc_length) != 0) {
		panic("smp_read_mpc: checksum error\n");
		return 0;
	}

	/* check specifications */
	if (mpc->mpc_spec != 1 && mpc->mpc_spec != 4) {
		panic("smp_read_mpc: bad table version (%d)\n", mpc->mpc_spec);
		return 0;
	}

	/* no local apic ! */
	if (!mpc->mpc_lapic)
		return 0;

	/* get OEM */
	memcpy(oem, mpc->mpc_oem, 8);
	oem[8] = 0;
	printf("OEM ID: %s ", oem);

	/* get product */
	memcpy(prod, mpc->mpc_productid, 12);
	prod[12] = 0;
	printf("Product ID: %s ", prod);

	/* get apic address */
	printf("APIC at: 0x%lX\n", mpc->mpc_lapic);

	/* save the local APIC address */
	mp_lapic_addr = mpc->mpc_lapic;

	return 1;
}

/*
 * Get SMP configuration.
 */
static void get_smp_config()
{
	struct intel_mp_floating *mpf = mpf_found;

	/* print specifications */
	printf("Intel MultiProcessor Specification v1.%d\n", mpf->mpf_specification);
	if (mpf->mpf_feature2 & (1 << 7))
		printf("    IMCR and PIC compatibility mode.\n");
	else
		printf("    Virtual Wire compatibility mode.\n");

	/* read configuration */
	if (mpf->mpf_physptr)
		smp_read_mpc((void *) mpf->mpf_physptr);
}

/*
 * Init smp.
 */
void init_smp()
{
	/*
	 * 1) Scan the bottom 1K for a signature
	 * 2) Scan the top 1K of base RAM
	 * 3) Scan the 64K of bios
	 */
	if (!smp_scan_config(0x0, 0x400)
		&& !smp_scan_config(639 * 0x400, 0x400)
		&& !smp_scan_config(0xF0000, 0x10000))
		return;

	/* get smp configuration */
	get_smp_config();
}
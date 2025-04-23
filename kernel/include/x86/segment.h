#ifndef _SEGMENT_H_
#define _SEGMENT_H_

/* Flags for _DESC_S (non-system) descriptors */
#define _DESC_ACCESSED			0x0001
#define _DESC_DATA_WRITABLE		0x0002
#define _DESC_CODE_READABLE		0x0002
#define _DESC_DATA_EXPAND_DOWN		0x0004
#define _DESC_CODE_CONFORMING		0x0004
#define _DESC_CODE_EXECUTABLE		0x0008

/* Common flags */
#define _DESC_S				0x0010
#define _DESC_DPL(dpl)			((dpl) << 5)
#define _DESC_PRESENT			0x0080

#define _DESC_LONG_CODE			0x2000
#define _DESC_DB			0x4000
#define _DESC_GRANULARITY_4K		0x8000

/* System descriptors have a numeric "type" field instead of flags */
#define _DESC_SYSTEM(code)		(code)

#define _DESC_DATA			(_DESC_S | _DESC_PRESENT | _DESC_ACCESSED | _DESC_DATA_WRITABLE)
#define _DESC_CODE			(_DESC_S | _DESC_PRESENT | _DESC_ACCESSED | _DESC_CODE_READABLE | _DESC_CODE_EXECUTABLE)
#define DESC_DATA32			(_DESC_DATA | _DESC_GRANULARITY_4K | _DESC_DB)
#define DESC_CODE32			(_DESC_CODE | _DESC_GRANULARITY_4K | _DESC_DB)
#define DESC_TSS32			(_DESC_SYSTEM(9) | _DESC_PRESENT)
#define DESC_USER			(_DESC_DPL(3))

#define GDT_ENTRY_TLS_ENTRIES		3
#define TLS_SIZE			(GDT_ENTRY_TLS_ENTRIES * 8)
#define GDT_ENTRIES			32
#define GDT_SIZE			(GDT_ENTRIES * 8)

/* GDT entries */
#define GDT_ENTRY_KERNEL_CS		1
#define GDT_ENTRY_KERNEL_DS		2
#define GDT_ENTRY_USER_CS		3
#define GDT_ENTRY_USER_DS		4
#define GDT_ENTRY_TSS			5
#define GDT_ENTRY_TLS_MIN		6
#define GDT_ENTRY_TLS_MAX		(GDT_ENTRY_TLS_MIN + GDT_ENTRY_TLS_ENTRIES - 1)
#define GDT_ENTRY_LDT			17

#endif
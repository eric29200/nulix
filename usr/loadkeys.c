#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <linux/keyboard.h>
#include <linux/kd.h>

#include "libutils/utils.h"

#define CONSOLE_DEV		"/dev/console"

#define E(x)			{ x, sizeof(x) / sizeof(x[0]) }
/*
 * Symbols entry.
 */
struct syms_entry {
	const char *const *	table;
	const uint16_t		size;
};

/*
 * Keysyms whose KTYP is KT_LATIN or KT_LETTER and whose KVAL is 0..127.
 */
static const char *const iso646_syms[] = {
	"nul", /* 0x00, 0000 */
	"Control_a",
	"Control_b",
	"Control_c",
	"Control_d",
	"Control_e",
	"Control_f",
	"Control_g",
	"BackSpace",
	"Tab",
	"Linefeed",
	"Control_k",
	"Control_l",
	"Control_m",
	"Control_n",
	"Control_o",
	"Control_p", /* 0x10, 0020 */
	"Control_q",
	"Control_r",
	"Control_s",
	"Control_t",
	"Control_u",
	"Control_v",
	"Control_w",
	"Control_x",
	"Control_y",
	"Control_z",
	"Escape",
	"Control_backslash",
	"Control_bracketright",
	"Control_asciicircum",
	"Control_underscore",
	"space", /* 0x20, 0040 */
	"exclam",
	"quotedbl",
	"numbersign",
	"dollar",
	"percent",
	"ampersand",
	"apostrophe",
	"parenleft",
	"parenright",
	"asterisk",
	"plus",
	"comma",
	"minus",
	"period",
	"slash",
	"zero", /* 0x30, 0060 */
	"one",
	"two",
	"three",
	"four",
	"five",
	"six",
	"seven",
	"eight",
	"nine",
	"colon",
	"semicolon",
	"less",
	"equal",
	"greater",
	"question",
	"at", /* 0x40, 0100 */
	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"H",
	"I",
	"J",
	"K",
	"L",
	"M",
	"N",
	"O",
	"P", /* 0x50, 0120 */
	"Q",
	"R",
	"S",
	"T",
	"U",
	"V",
	"W",
	"X",
	"Y",
	"Z",
	"bracketleft",
	"backslash",
	"bracketright",
	"asciicircum",
	"underscore",
	"grave", /* 0x60, 0140 */
	"a",
	"b",
	"c",
	"d",
	"e",
	"f",
	"g",
	"h",
	"i",
	"j",
	"k",
	"l",
	"m",
	"n",
	"o",
	"p", /* 0x70, 0160 */
	"q",
	"r",
	"s",
	"t",
	"u",
	"v",
	"w",
	"x",
	"y",
	"z",
	"braceleft",
	"bar",
	"braceright",
	"asciitilde",
	"Delete",
	"", /* 0x80, 0200 */
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"", /* 0x90, 0220 */
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"nobreakspace", /* 0xa0, 0240 */
	"exclamdown",
	"cent",
	"sterling",
	"currency",
	"yen",
	"brokenbar",
	"section",
	"diaeresis",
	"copyright",
	"ordfeminine",
	"guillemotleft",
	"notsign",
	"hyphen",
	"registered",
	"macron",
	"degree", /* 0xb0, 0260 */
	"plusminus",
	"twosuperior",
	"threesuperior",
	"acute",
	"mu",
	"paragraph",
	"periodcentered",
	"cedilla",
	"onesuperior",
	"masculine",
	"guillemotright",
	"onequarter",
	"onehalf",
	"threequarters",
	"questiondown",
	"Agrave", /* 0xc0, 0300 */
	"Aacute",
	"Acircumflex",
	"Atilde",
	"Adiaeresis",
	"Aring",
	"AE",
	"Ccedilla",
	"Egrave",
	"Eacute",
	"Ecircumflex",
	"Ediaeresis",
	"Igrave",
	"Iacute",
	"Icircumflex",
	"Idiaeresis",
	"ETH", /* 0xd0, 0320 */
	"Ntilde",
	"Ograve",
	"Oacute",
	"Ocircumflex",
	"Otilde",
	"Odiaeresis",
	"multiply",
	"Ooblique",
	"Ugrave",
	"Uacute",
	"Ucircumflex",
	"Udiaeresis",
	"Yacute",
	"THORN",
	"ssharp",
	"agrave", /* 0xe0, 0340 */
	"aacute",
	"acircumflex",
	"atilde",
	"adiaeresis",
	"aring",
	"ae",
	"ccedilla",
	"egrave",
	"eacute",
	"ecircumflex",
	"ediaeresis",
	"igrave",
	"iacute",
	"icircumflex",
	"idiaeresis",
	"eth", /* 0xf0, 0360 */
	"ntilde",
	"ograve",
	"oacute",
	"ocircumflex",
	"otilde",
	"odiaeresis",
	"division",
	"oslash",
	"ugrave",
	"uacute",
	"ucircumflex",
	"udiaeresis",
	"yacute",
	"thorn",
	"ydiaeresis", /* 0xff, 0377 */
};

/*
 * Keysyms whose KTYP is KT_FN.
 */
static const char *const fn_syms[] = {
	"F1", "F2", "F3", "F4", "F5",
	"F6", "F7", "F8", "F9", "F10",
	"F11", "F12", "F13", "F14", "F15",
	"F16", "F17", "F18", "F19", "F20",
	"Find", /* also called: "Home" */
	"Insert",
	"Remove",
	"Select", /* also called: "End" */
	"Prior",  /* also called: "PageUp" */
	"Next",   /* also called: "PageDown" */
	"Macro",
	"Help",
	"Do",
	"Pause",
	"F21", "F22", "F23", "F24", "F25",
	"F26", "F27", "F28", "F29", "F30",
	"F31", "F32", "F33", "F34", "F35",
	"F36", "F37", "F38", "F39", "F40",
	"F41", "F42", "F43", "F44", "F45",
	"F46", "F47", "F48", "F49", "F50",
	"F51", "F52", "F53", "F54", "F55",
	"F56", "F57", "F58", "F59", "F60",
	"F61", "F62", "F63", "F64", "F65",
	"F66", "F67", "F68", "F69", "F70",
	"F71", "F72", "F73", "F74", "F75",
	"F76", "F77", "F78", "F79", "F80",
	"F81", "F82", "F83", "F84", "F85",
	"F86", "F87", "F88", "F89", "F90",
	"F91", "F92", "F93", "F94", "F95",
	"F96", "F97", "F98", "F99", "F100",
	"F101", "F102", "F103", "F104", "F105",
	"F106", "F107", "F108", "F109", "F110",
	"F111", "F112", "F113", "F114", "F115",
	"F116", "F117", "F118", "F119", "F120",
	"F121", "F122", "F123", "F124", "F125",
	"F126", "F127", "F128", "F129", "F130",
	"F131", "F132", "F133", "F134", "F135",
	"F136", "F137", "F138", "F139", "F140",
	"F141", "F142", "F143", "F144", "F145",
	"F146", "F147", "F148", "F149", "F150",
	"F151", "F152", "F153", "F154", "F155",
	"F156", "F157", "F158", "F159", "F160",
	"F161", "F162", "F163", "F164", "F165",
	"F166", "F167", "F168", "F169", "F170",
	"F171", "F172", "F173", "F174", "F175",
	"F176", "F177", "F178", "F179", "F180",
	"F181", "F182", "F183", "F184", "F185",
	"F186", "F187", "F188", "F189", "F190",
	"F191", "F192", "F193", "F194", "F195",
	"F196", "F197", "F198", "F199", "F200",
	"F201", "F202", "F203", "F204", "F205",
	"F206", "F207", "F208", "F209", "F210",
	"F211", "F212", "F213", "F214", "F215",
	"F216", "F217", "F218", "F219", "F220",
	"F221", "F222", "F223", "F224", "F225",
	"F226", "F227", "F228", "F229", "F230",
	"F231", "F232", "F233", "F234", "F235",
	"F236", "F237", "F238", "F239", "F240",
	"F241", "F242", "F243", "F244", "F245",
	"F246" /* there are 10 keys named Insert etc., total 256 */
};

/*
 * Keysyms whose KTYP is KT_SPEC.
 */
static const char *const spec_syms[] = {
	"VoidSymbol",
	"Return",
	"Show_Registers",
	"Show_Memory",
	"Show_State",
	"Break",
	"Last_Console",
	"Caps_Lock",
	"Num_Lock",
	"Scroll_Lock",
	"Scroll_Forward",
	"Scroll_Backward",
	"Boot",
	"Caps_On",
	"Compose",
	"SAK",
	"Decr_Console",
	"Incr_Console",
	"KeyboardSignal",
	"Bare_Num_Lock"
};

/*
 * Keysyms whose KTYP is KT_PAD.
 */
static const char *const pad_syms[] = {
	"KP_0",
	"KP_1",
	"KP_2",
	"KP_3",
	"KP_4",
	"KP_5",
	"KP_6",
	"KP_7",
	"KP_8",
	"KP_9",
	"KP_Add",
	"KP_Subtract",
	"KP_Multiply",
	"KP_Divide",
	"KP_Enter",
	"KP_Comma",
	"KP_Period",
	"KP_MinPlus"
};

/*
 * Keysyms whose KTYP is KT_DEAD.
 */
static const char *const dead_syms[] = {
	"dead_grave",
	"dead_acute",
	"dead_circumflex",
	"dead_tilde",
	"dead_diaeresis",
	"dead_cedilla",
	"dead_macron",
	"dead_kbreve", // dead_breve is an alias for dead_tilde
	"dead_abovedot",
	"dead_abovering",
	"dead_kdoubleacute", // dead_doubleacute is an alias for dead_tilde
	"dead_kcaron", // dead_caron is an alias for dead_circumflex
	"dead_kogonek", // dead_ogonek is an alias for dead_cedilla
	"dead_iota",
	"dead_voiced_sound",
	"dead_semivoiced_sound",
	"dead_belowdot",
	"dead_hook",
	"dead_horn",
	"dead_stroke",
	"dead_abovecomma",
	"dead_abovereversedcomma",
	"dead_doublegrave",
	"dead_invertedbreve",
	"dead_belowcomma",
	"dead_currency",
	"dead_greek"
};

/*
 * Keysyms whose KTYP is KT_CONS.
 */
static const char *const cons_syms[] = {
	"Console_1",
	"Console_2",
	"Console_3",
	"Console_4",
	"Console_5",
	"Console_6",
	"Console_7",
	"Console_8",
	"Console_9",
	"Console_10",
	"Console_11",
	"Console_12",
	"Console_13",
	"Console_14",
	"Console_15",
	"Console_16",
	"Console_17",
	"Console_18",
	"Console_19",
	"Console_20",
	"Console_21",
	"Console_22",
	"Console_23",
	"Console_24",
	"Console_25",
	"Console_26",
	"Console_27",
	"Console_28",
	"Console_29",
	"Console_30",
	"Console_31",
	"Console_32",
	"Console_33",
	"Console_34",
	"Console_35",
	"Console_36",
	"Console_37",
	"Console_38",
	"Console_39",
	"Console_40",
	"Console_41",
	"Console_42",
	"Console_43",
	"Console_44",
	"Console_45",
	"Console_46",
	"Console_47",
	"Console_48",
	"Console_49",
	"Console_50",
	"Console_51",
	"Console_52",
	"Console_53",
	"Console_54",
	"Console_55",
	"Console_56",
	"Console_57",
	"Console_58",
	"Console_59",
	"Console_60",
	"Console_61",
	"Console_62",
	"Console_63"
};

/*
 * Keysyms whose KTYP is KT_CUR.
 */
static const char *const cur_syms[] = {
	"Down",
	"Left",
	"Right",
	"Up"
};

/*
 * Keysyms whose KTYP is KT_SHIFT.
 */
static const char *const shift_syms[] = {
	"Shift",
	"AltGr",
	"Control",
	"Alt",
	"ShiftL",
	"ShiftR",
	"CtrlL",
	"CtrlR",
	"CapsShift"
};

/*
 * Keysyms whose KTYP is KT_ASCII.
 */
static const char *const ascii_syms[] = {
	"Ascii_0",
	"Ascii_1",
	"Ascii_2",
	"Ascii_3",
	"Ascii_4",
	"Ascii_5",
	"Ascii_6",
	"Ascii_7",
	"Ascii_8",
	"Ascii_9",
	"Hex_0",
	"Hex_1",
	"Hex_2",
	"Hex_3",
	"Hex_4",
	"Hex_5",
	"Hex_6",
	"Hex_7",
	"Hex_8",
	"Hex_9",
	"Hex_A",
	"Hex_B",
	"Hex_C",
	"Hex_D",
	"Hex_E",
	"Hex_F"
};

/*
 * Keysyms whose KTYP is KT_LOCK.
 */
static const char *const lock_syms[] = {
	"Shift_Lock",
	"AltGr_Lock",
	"Control_Lock",
	"Alt_Lock",
	"ShiftL_Lock",
	"ShiftR_Lock",
	"CtrlL_Lock",
	"CtrlR_Lock",
	"CapsShift_Lock"
};

/*
 * Keysyms whose KTYP is KT_SLOCK.
 */
static const char *const sticky_syms[] = {
	"SShift",
	"SAltGr",
	"SControl",
	"SAlt",
	"SShiftL",
	"SShiftR",
	"SCtrlL",
	"SCtrlR",
	"SCapsShift"
};

/*
 * Keysyms whose KTYP is KT_BRL.
 */
static const char *const brl_syms[] = {
	"Brl_blank",
	"Brl_dot1",
	"Brl_dot2",
	"Brl_dot3",
	"Brl_dot4",
	"Brl_dot5",
	"Brl_dot6",
	"Brl_dot7",
	"Brl_dot8",
	"Brl_dot9",
	"Brl_dot10"
};

/*
 * Symbols.
 */
const struct syms_entry syms[] = {
	E(iso646_syms), 	/* KT_LATIN */
	E(fn_syms),     	/* KT_FN */
	E(spec_syms),   	/* KT_SPEC */
	E(pad_syms),    	/* KT_PAD */
	E(dead_syms),   	/* KT_DEAD */
	E(cons_syms),   	/* KT_CONS */
	E(cur_syms),    	/* KT_CUR */
	E(shift_syms),  	/* KT_SHIFT */
	{ 0, 0 },       	/* KT_META */
	E(ascii_syms),  	/* KT_ASCII */
	E(lock_syms),   	/* KT_LOCK */
	{ 0, 0 },      	 	/* KT_LETTER */
	E(sticky_syms), 	/* KT_SLOCK */
	{ 0, 0 },       	/*  */
	E(brl_syms)     	/* KT_BRL */
};

/* symbols size */
static size_t syms_size = sizeof(syms) / sizeof(syms[0]);

/*
 * Load a keymap.
 */
static int load_keymap(const char *filename, int fd_console)
{
	int keymap, keycode, keyval, line;
	char buf[BUFSIZ], *tok;
	struct kbentry kbe;
	size_t len, i, j;
	FILE *fp;

	/* open input file */
	fp = fopen(filename, "r");
	if (!fp) {
		perror(filename);
		return 1;
	}

	/* read file */
	for (line = 1; ; line++) {
		/* get next line */
		if (!fgets(buf, BUFSIZ, fp))
			break;

		/* skip empty line */
		len = strnlen(buf, BUFSIZ);
		if (!len)
			continue;
		
		/* remove end of line */
		if (buf[len - 1] == '\n')
			buf[len - 1] = 0;

		/* parse key map */
		tok = strtok(buf, " ");
		if (!tok)
			goto err_line;
		else if (strcmp(tok, "plain") == 0)
			keymap = 0;
		else if (strcmp(tok, "shift") == 0)
			keymap = 1;
		else if (strcmp(tok, "control") == 0)
			keymap = 4;
		else if (strcmp(tok, "alt") == 0)
			keymap = 8;
		else if (strcmp(tok, "altgr") == 0)
			keymap = 2;
		else
			goto err_line;

		/* next token must be "keycode" */
		tok = strtok(NULL, " ");
		if (!tok || strcmp(tok, "keycode") != 0)
			goto err_line;

		/* parse keycode */
		tok = strtok(NULL, " ");
		if (tok)
			keycode = atoi(tok);
		else
			goto err_line;

		/* next token must be "=" */
		tok = strtok(NULL, " ");
		if (!tok || strcmp(tok, "=") != 0)
			goto err_line;

		/* get keyval */
		tok = strtok(NULL, " ");
		if (!tok)
			goto err_line;

		/* try to find symbol */
		for (i = 0; i < syms_size; i++) {
			for (j = 0; j < syms[i].size; j++) {
				if (strcmp(tok, syms[i].table[j]) == 0) {
					keyval = K(i, j);
					goto found;
				}
			}
		}

		goto err_line;
found:
		/* create kb entry */
		kbe.kb_table = keymap;
		kbe.kb_index = keycode;
		kbe.kb_value = keyval;

		/* set console kb entry */
		if (ioctl(fd_console, KDSKBENT, &kbe) < 0) {
			perror(CONSOLE_DEV);
			goto err;
		}
	}

	/* close input file */
	fclose(fp);

	return 0;
err_line:
	fprintf(stderr, "line %d malformed\n", line);
err:
	fclose(fp);
	return 1;
}

/*
 * Usage.
 */
static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s file\n", name);
	fprintf(stderr, "\t  , --help\t\tprint help and exit\n");
}

/* options */
struct option long_opts[] = {
	{ "help",	no_argument,	0,	OPT_HELP	},
	{ 0,		0,		0,	0		},
};

int main(int argc, char **argv)
{
	int i, c, ret = 0, fd_console;
	const char *name = argv[0];

	/* get options */
	while ((c = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
		switch (c) {
			case OPT_HELP:
				usage(name);
				exit(0);
				break;
			default:
				exit(1);
				break;
		}
	}

	/* skip options */
	argc -= optind;
	argv += optind;

	/* check arguments */
	if (argc != 1) {
		usage(name);
		exit(1);
	}

	/* open console */
	fd_console = open(CONSOLE_DEV, O_RDWR);
	if (fd_console < 0) {
		perror(CONSOLE_DEV);
		exit(1);
	}

	/* load keymaps */
	for (i = 0; i < argc; i++)
		ret |= load_keymap(argv[i], fd_console);

	/* close console */
	close(fd_console);

	return ret;
}
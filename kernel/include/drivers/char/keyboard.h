#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

#include <drivers/char/tty.h>
#include <stddef.h>

#define NR_KEYS				256
#define MAX_NR_KEYMAPS			256
#define MAX_NR_FUNCS			256
#define NR_DEAD				6
#define NR_LOCK				9
#define NR_BRL				11
#define NR_PAD				20
#define NR_ASCII			26
extern const int NR_TYPES;
extern const int max_vals[];

#define KEYBOARD_PORT			0x60
#define KEYBOARD_ACK			0x61
#define KEYBOARD_STATUS			0x64
#define KEYBOARD_LED_CODE		0xED
#define KEYBOARD_RESET			0xFE

#define K(t, v)				(((t) << 8) | (v))
#define KTYP(x)				((x) >> 8)
#define KVAL(x)				((x) & 0xFF)

#define KG_SHIFT			0
#define KG_CTRL				2
#define KG_ALT				3
#define KG_ALTGR			1
#define KG_SHIFTL			4
#define KG_SHIFTR			5
#define KG_CTRLL			6
#define KG_CTRLR			7
#define KG_CAPSSHIFT			8

#define NR_SHIFT			9

#define KT_LATIN			0
#define KT_LETTER			11
#define KT_FN				1
#define KT_SPEC				2
#define KT_PAD				3
#define KT_DEAD				4
#define KT_CONS				5
#define KT_CUR				6
#define KT_SHIFT			7
#define KT_META				8
#define KT_ASCII			9
#define KT_LOCK				10
#define KT_SLOCK			12

#define K_F1				K(KT_FN, 0)
#define K_F2				K(KT_FN, 1)
#define K_F3				K(KT_FN, 2)
#define K_F4				K(KT_FN, 3)
#define K_F5				K(KT_FN, 4)
#define K_F6				K(KT_FN, 5)
#define K_F7				K(KT_FN, 6)
#define K_F8				K(KT_FN, 7)
#define K_F9				K(KT_FN, 8)
#define K_F10				K(KT_FN, 9)
#define K_F11				K(KT_FN, 10)
#define K_F12				K(KT_FN, 11)
#define K_F13				K(KT_FN, 12)
#define K_F14				K(KT_FN, 13)
#define K_F15				K(KT_FN, 14)
#define K_F16				K(KT_FN, 15)
#define K_F17				K(KT_FN, 16)
#define K_F18				K(KT_FN, 17)
#define K_F19				K(KT_FN, 18)
#define K_F20				K(KT_FN, 19)
#define K_FIND				K(KT_FN, 20)
#define K_INSERT			K(KT_FN, 21)
#define K_REMOVE			K(KT_FN, 22)
#define K_SELECT			K(KT_FN, 23)
#define K_PGUP				K(KT_FN, 24)
#define K_PGDN				K(KT_FN, 25)
#define K_MACRO	 			K(KT_FN, 26)
#define K_HELP				K(KT_FN, 27)
#define K_DO				K(KT_FN, 28)
#define K_PAUSE	 			K(KT_FN, 29)
#define K_F21				K(KT_FN, 30)
#define K_F22				K(KT_FN, 31)
#define K_F23				K(KT_FN, 32)
#define K_F24				K(KT_FN, 33)
#define K_F25				K(KT_FN, 34)
#define K_F26				K(KT_FN, 35)
#define K_F27				K(KT_FN, 36)
#define K_F28				K(KT_FN, 37)
#define K_F29				K(KT_FN, 38)
#define K_F30				K(KT_FN, 39)
#define K_F31				K(KT_FN, 40)
#define K_F32				K(KT_FN, 41)
#define K_F33				K(KT_FN, 42)
#define K_F34				K(KT_FN, 43)
#define K_F35				K(KT_FN, 44)
#define K_F36				K(KT_FN, 45)
#define K_F37				K(KT_FN, 46)
#define K_F38				K(KT_FN, 47)
#define K_F39				K(KT_FN, 48)
#define K_F40				K(KT_FN, 49)
#define K_F41				K(KT_FN, 50)
#define K_F42				K(KT_FN, 51)
#define K_F43				K(KT_FN, 52)
#define K_F44				K(KT_FN, 53)
#define K_F45				K(KT_FN, 54)
#define K_F46				K(KT_FN, 55)
#define K_F47				K(KT_FN, 56)
#define K_F48				K(KT_FN, 57)
#define K_F49				K(KT_FN, 58)
#define K_F50				K(KT_FN, 59)
#define K_F51				K(KT_FN, 60)
#define K_F52				K(KT_FN, 61)
#define K_F53				K(KT_FN, 62)
#define K_F54				K(KT_FN, 63)
#define K_F55				K(KT_FN, 64)
#define K_F56				K(KT_FN, 65)
#define K_F57				K(KT_FN, 66)
#define K_F58				K(KT_FN, 67)
#define K_F59				K(KT_FN, 68)
#define K_F60				K(KT_FN, 69)
#define K_F61				K(KT_FN, 70)
#define K_F62				K(KT_FN, 71)
#define K_F63				K(KT_FN, 72)
#define K_F64				K(KT_FN, 73)
#define K_F65				K(KT_FN, 74)
#define K_F66				K(KT_FN, 75)
#define K_F67				K(KT_FN, 76)
#define K_F68				K(KT_FN, 77)
#define K_F69				K(KT_FN, 78)
#define K_F70				K(KT_FN, 79)
#define K_F71				K(KT_FN, 80)
#define K_F72				K(KT_FN, 81)
#define K_F73				K(KT_FN, 82)
#define K_F74				K(KT_FN, 83)
#define K_F75				K(KT_FN, 84)
#define K_F76				K(KT_FN, 85)
#define K_F77				K(KT_FN, 86)
#define K_F78				K(KT_FN, 87)
#define K_F79				K(KT_FN, 88)
#define K_F80				K(KT_FN, 89)
#define K_F81				K(KT_FN, 90)
#define K_F82				K(KT_FN, 91)
#define K_F83				K(KT_FN, 92)
#define K_F84				K(KT_FN, 93)
#define K_F85				K(KT_FN, 94)
#define K_F86				K(KT_FN, 95)
#define K_F87				K(KT_FN, 96)
#define K_F88				K(KT_FN, 97)
#define K_F89				K(KT_FN, 98)
#define K_F90				K(KT_FN, 99)
#define K_F91				K(KT_FN, 100)
#define K_F92				K(KT_FN, 101)
#define K_F93				K(KT_FN, 102)
#define K_F94				K(KT_FN, 103)
#define K_F95				K(KT_FN, 104)
#define K_F96				K(KT_FN, 105)
#define K_F97				K(KT_FN, 106)
#define K_F98				K(KT_FN, 107)
#define K_F99				K(KT_FN, 108)
#define K_F100				K(KT_FN, 109)
#define K_F101				K(KT_FN, 110)
#define K_F102				K(KT_FN, 111)
#define K_F103				K(KT_FN, 112)
#define K_F104				K(KT_FN, 113)
#define K_F105				K(KT_FN, 114)
#define K_F106				K(KT_FN, 115)
#define K_F107				K(KT_FN, 116)
#define K_F108				K(KT_FN, 117)
#define K_F109				K(KT_FN, 118)
#define K_F110				K(KT_FN, 119)
#define K_F111				K(KT_FN, 120)
#define K_F112				K(KT_FN, 121)
#define K_F113				K(KT_FN, 122)
#define K_F114				K(KT_FN, 123)
#define K_F115				K(KT_FN, 124)
#define K_F116				K(KT_FN, 125)
#define K_F117				K(KT_FN, 126)
#define K_F118				K(KT_FN, 127)
#define K_F119				K(KT_FN, 128)
#define K_F120				K(KT_FN, 129)
#define K_F121				K(KT_FN, 130)
#define K_F122				K(KT_FN, 131)
#define K_F123				K(KT_FN, 132)
#define K_F124				K(KT_FN, 133)
#define K_F125				K(KT_FN, 134)
#define K_F126				K(KT_FN, 135)
#define K_F127				K(KT_FN, 136)
#define K_F128				K(KT_FN, 137)
#define K_F129				K(KT_FN, 138)
#define K_F130				K(KT_FN, 139)
#define K_F131				K(KT_FN, 140)
#define K_F132				K(KT_FN, 141)
#define K_F133				K(KT_FN, 142)
#define K_F134				K(KT_FN, 143)
#define K_F135				K(KT_FN, 144)
#define K_F136				K(KT_FN, 145)
#define K_F137				K(KT_FN, 146)
#define K_F138				K(KT_FN, 147)
#define K_F139				K(KT_FN, 148)
#define K_F140				K(KT_FN, 149)
#define K_F141				K(KT_FN, 150)
#define K_F142				K(KT_FN, 151)
#define K_F143				K(KT_FN, 152)
#define K_F144				K(KT_FN, 153)
#define K_F145				K(KT_FN, 154)
#define K_F146				K(KT_FN, 155)
#define K_F147				K(KT_FN, 156)
#define K_F148				K(KT_FN, 157)
#define K_F149				K(KT_FN, 158)
#define K_F150				K(KT_FN, 159)
#define K_F151				K(KT_FN, 160)
#define K_F152				K(KT_FN, 161)
#define K_F153				K(KT_FN, 162)
#define K_F154				K(KT_FN, 163)
#define K_F155				K(KT_FN, 164)
#define K_F156				K(KT_FN, 165)
#define K_F157				K(KT_FN, 166)
#define K_F158				K(KT_FN, 167)
#define K_F159				K(KT_FN, 168)
#define K_F160				K(KT_FN, 169)
#define K_F161				K(KT_FN, 170)
#define K_F162				K(KT_FN, 171)
#define K_F163				K(KT_FN, 172)
#define K_F164				K(KT_FN, 173)
#define K_F165				K(KT_FN, 174)
#define K_F166				K(KT_FN, 175)
#define K_F167				K(KT_FN, 176)
#define K_F168				K(KT_FN, 177)
#define K_F169				K(KT_FN, 178)
#define K_F170				K(KT_FN, 179)
#define K_F171				K(KT_FN, 180)
#define K_F172				K(KT_FN, 181)
#define K_F173				K(KT_FN, 182)
#define K_F174				K(KT_FN, 183)
#define K_F175				K(KT_FN, 184)
#define K_F176				K(KT_FN, 185)
#define K_F177				K(KT_FN, 186)
#define K_F178				K(KT_FN, 187)
#define K_F179				K(KT_FN, 188)
#define K_F180				K(KT_FN, 189)
#define K_F181				K(KT_FN, 190)
#define K_F182				K(KT_FN, 191)
#define K_F183				K(KT_FN, 192)
#define K_F184				K(KT_FN, 193)
#define K_F185				K(KT_FN, 194)
#define K_F186				K(KT_FN, 195)
#define K_F187				K(KT_FN, 196)
#define K_F188				K(KT_FN, 197)
#define K_F189				K(KT_FN, 198)
#define K_F190				K(KT_FN, 199)
#define K_F191				K(KT_FN, 200)
#define K_F192				K(KT_FN, 201)
#define K_F193				K(KT_FN, 202)
#define K_F194				K(KT_FN, 203)
#define K_F195				K(KT_FN, 204)
#define K_F196				K(KT_FN, 205)
#define K_F197				K(KT_FN, 206)
#define K_F198				K(KT_FN, 207)
#define K_F199				K(KT_FN, 208)
#define K_F200				K(KT_FN, 209)
#define K_F201				K(KT_FN, 210)
#define K_F202				K(KT_FN, 211)
#define K_F203				K(KT_FN, 212)
#define K_F204				K(KT_FN, 213)
#define K_F205				K(KT_FN, 214)
#define K_F206				K(KT_FN, 215)
#define K_F207				K(KT_FN, 216)
#define K_F208				K(KT_FN, 217)
#define K_F209				K(KT_FN, 218)
#define K_F210				K(KT_FN, 219)
#define K_F211				K(KT_FN, 220)
#define K_F212				K(KT_FN, 221)
#define K_F213				K(KT_FN, 222)
#define K_F214				K(KT_FN, 223)
#define K_F215				K(KT_FN, 224)
#define K_F216				K(KT_FN, 225)
#define K_F217				K(KT_FN, 226)
#define K_F218				K(KT_FN, 227)
#define K_F219				K(KT_FN, 228)
#define K_F220				K(KT_FN, 229)
#define K_F221				K(KT_FN, 230)
#define K_F222				K(KT_FN, 231)
#define K_F223				K(KT_FN, 232)
#define K_F224				K(KT_FN, 233)
#define K_F225				K(KT_FN, 234)
#define K_F226				K(KT_FN, 235)
#define K_F227				K(KT_FN, 236)
#define K_F228				K(KT_FN, 237)
#define K_F229				K(KT_FN, 238)
#define K_F230				K(KT_FN, 239)
#define K_F231				K(KT_FN, 240)
#define K_F232				K(KT_FN, 241)
#define K_F233				K(KT_FN, 242)
#define K_F234				K(KT_FN, 243)
#define K_F235				K(KT_FN, 244)
#define K_F236				K(KT_FN, 245)
#define K_F237				K(KT_FN, 246)
#define K_F238				K(KT_FN, 247)
#define K_F239				K(KT_FN, 248)
#define K_F240				K(KT_FN, 249)
#define K_F241				K(KT_FN, 250)
#define K_F242				K(KT_FN, 251)
#define K_F243				K(KT_FN, 252)
#define K_F244				K(KT_FN, 253)
#define K_F245				K(KT_FN, 254)
#define K_F246				K(KT_FN, 255)

#define K_HOLE				K(KT_SPEC, 0)
#define K_ALLOCATED			K(KT_SPEC, 126) 	/* dynamically allocated keymap */
#define K_NOSUCHMAP			K(KT_SPEC, 127) 	/* returned by KDGKBENT */

#define K_P0				K(KT_PAD, 0)
#define K_P1				K(KT_PAD, 1)
#define K_P2				K(KT_PAD, 2)
#define K_P3				K(KT_PAD, 3)
#define K_P4				K(KT_PAD, 4)
#define K_P5				K(KT_PAD, 5)
#define K_P6				K(KT_PAD, 6)
#define K_P7				K(KT_PAD, 7)
#define K_P8				K(KT_PAD, 8)
#define K_P9				K(KT_PAD, 9)
#define K_PPLUS				K(KT_PAD, 10)
#define K_PMINUS			K(KT_PAD, 11)
#define K_PSTAR				K(KT_PAD, 12)
#define K_PSLASH			K(KT_PAD, 13)
#define K_PENTER			K(KT_PAD, 14)
#define K_PCOMMA			K(KT_PAD, 15)
#define K_PDOT				K(KT_PAD, 16)
#define K_PPLUSMINUS			K(KT_PAD, 17)

#define K_DGRAVE			K(KT_DEAD, 0)
#define K_DACUTE			K(KT_DEAD, 1)
#define K_DCIRCM			K(KT_DEAD, 2)
#define K_DTILDE			K(KT_DEAD, 3)
#define K_DDIERE			K(KT_DEAD, 4)

#define K_DOWN				K(KT_CUR, 0)
#define K_LEFT				K(KT_CUR, 1)
#define K_RIGHT				K(KT_CUR, 2)
#define K_UP				K(KT_CUR, 3)

#define K_SHIFT				K(KT_SHIFT, KG_SHIFT)
#define K_CTRL				K(KT_SHIFT, KG_CTRL)
#define K_ALT				K(KT_SHIFT, KG_ALT)
#define K_ALTGR				K(KT_SHIFT, KG_ALTGR)
#define K_SHIFTL			K(KT_SHIFT, KG_SHIFTL)
#define K_SHIFTR			K(KT_SHIFT, KG_SHIFTR)
#define K_CTRLL	 			K(KT_SHIFT, KG_CTRLL)
#define K_CTRLR	 			K(KT_SHIFT, KG_CTRLR)
#define K_CAPSSHIFT			K(KT_SHIFT, KG_CAPSSHIFT)

#define E0_KPENTER 			96
#define E0_RCTRL   			97
#define E0_KPSLASH 			98
#define E0_PRSCR   			99
#define E0_RALT    			100
#define E0_BREAK   			101
#define E0_HOME    			102
#define E0_UP      			103
#define E0_PGUP    			104
#define E0_LEFT    			105
#define E0_RIGHT   			106
#define E0_END     			107
#define E0_DOWN    			108
#define E0_PGDN    			109
#define E0_INS     			110
#define E0_DEL     			111
#define E0_MACRO   			112
#define E0_F13     			113
#define E0_F14     			114
#define E0_HELP    			115
#define E0_DO      			116
#define E0_F17     			117
#define E0_KPMINPLUS 			118
#define E0_OK				124
#define E0_MSLW				125
#define E0_MSRW				126
#define E0_MSTM				127
#define E1_PAUSE   			119
#define E_TABSZ				256

#define VC_XLATE			0	/* translate keycodes using keymap */
#define VC_MEDIUMRAW			1	/* medium raw (keycode) mode */
#define VC_RAW				2	/* raw (scancode) mode */
#define VC_UNICODE			3	/* unicode mode */

#define KBD_DEFLEDS			0
#define LED_SHOW_FLAGS			0

/*
 * Keyboard structure.
 */
struct kbd {
	uint8_t		kbd_mode;
	uint8_t		kbd_led_mode;
	uint8_t		kbd_led_flag_state;
	uint8_t		kbd_default_led_flag_state;
};

extern struct kbd kbd_table[NR_CONSOLES];
extern uint16_t *key_maps[MAX_NR_KEYMAPS];
extern uint16_t plain_map[NR_KEYS];
extern uint8_t *func_table[MAX_NR_FUNCS];
extern uint8_t e0_keys[128]; 
extern int shift_state;

void init_keyboard();

#endif

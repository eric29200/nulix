#ifndef _FONT_H_
#define _FONT_H_

#include <stddef.h>

#define PSF2_MAGIC0             0x72
#define PSF2_MAGIC1             0xb5
#define PSF2_MAGIC2             0x4a
#define PSF2_MAGIC3             0x86

#define PSF2_HAS_UNICODE_TABLE  0x01

#define PSF2_MAXVERSION         0

#define PSF2_SEPARATOR          0xFF
#define PSF2_STARTSEQ           0xFE

/*
 * Font v2 header.
 */
struct psf2_header_t {
  unsigned char magic[4];
  unsigned int version;
  unsigned int headersize;
  unsigned int flags;
  unsigned int length;
  unsigned int charsize;
  unsigned int height;
  unsigned int width;
};

#define UC_MAP_SIZE       1024

/*
 * Font structure.
 */
struct font_t {
  uint32_t        height;               /* font height */
  uint32_t        width;                /* font width */
  uint32_t        char_size;            /* glyph size in bytes */
  uint32_t        char_count;           /* number of glyphs */
  uint32_t        uc_size;              /* number of unicode mappings */
  uint32_t        uc_map[UC_MAP_SIZE];  /* unicode mapping */
  unsigned char   *data;                /* pointer to first glyph */
};

int load_font(struct font_t *font, void *font_start, void *font_end);
int get_glyph(struct font_t *font, uint16_t unicode);

#endif

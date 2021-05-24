#ifndef _FONT_H_
#define _FONT_H_

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

#endif

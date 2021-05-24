#include <font.h>
#include <stderr.h>

/*
 * Decode a UTF8 value to a unicode value.
 */
static int get_utf8(unsigned char **uptr, unsigned char *font_end)
{
  unsigned char *ptr = *uptr;
  int utf8;

  /* utf8 value on 1 byte */
  if ((*ptr & 0x80) == 0) {
    if (ptr >= font_end)
      return -1;

    utf8 = (int) (*ptr);
    *uptr += 1;
    return utf8;
  }

  /* utf8 value on 2 bytes */
  if ((*ptr & 0xE0) == 0xC0) {
    if (ptr + 1 >= font_end)
      return -1;

    utf8 = (*ptr++ & ~0xE0) << 6;
    utf8 |= *ptr++ & ~0xC0;
    *uptr += 2;
    return utf8;
  }

  /* utf8 value on 3 bytes */
  if ((*ptr & 0xF0) == 0xE0) {
    if (ptr + 2 >= font_end)
      return -1;

    utf8 = (*ptr++ & ~0xF0) << 12;
    utf8 |= (*ptr++ & ~0xC0) << 6;
    utf8 |= *ptr++ & ~0xC0;
    *uptr += 3;
    return utf8;
  }

  return -1;
}

/*
 * Convert a unicode value to a glyph.
 */
int get_glyph(struct font_t *font, uint16_t unicode)
{
  uint32_t s;

  for (s = 0; s < font->uc_size; s++)
    if (unicode == (font->uc_map[s] & 0xFFFF))
      return (font->uc_map[s] & 0xFFFF0000) >> 16;

  return -1;
}

/*
 * Map a glyph to a font structure.
 */
static int map_glyph(struct font_t *font, uint16_t unicode, uint16_t glyph_index)
{
  if (font->uc_size >= UC_MAP_SIZE)
    return -ENOSPC;

  font->uc_map[font->uc_size++] = unicode | (glyph_index << 16);
  return 0;
}

/*
 * Map unicode glyphes.
 */
static int map_unicode(struct font_t *font, struct psf2_header_t *header, unsigned char *font_end)
{
  int font_pos, utf8, ret;
  unsigned char *uptr;

  /* skip header */
  uptr = (unsigned char *) ((uint32_t) header + header->headersize + header->length * header->charsize);

  /* map all glyphes */
  for (font_pos = 0; uptr < font_end;) {
    /* skip psf separators */
    if (*uptr == PSF2_SEPARATOR) {
      font_pos++;
      uptr++;
      continue;
    }

    /* ignore sequences */
    if (*uptr == PSF2_STARTSEQ) {
      while (*uptr != PSF2_SEPARATOR && uptr < font_end)
        uptr++;
      continue;
    }

    /* get utf8 character */
    utf8 = get_utf8(&uptr, font_end);
    if (utf8 < 0) {
      uptr++;
      continue;
    }

    /* map glyph */
    ret = map_glyph(font, utf8, font_pos);
    if (ret != 0)
      return ret;
  }

  return 0;
}

/*
 * Map ascii glyphes.
 */
static int map_ascii(struct font_t *font)
{
  int i, ret;

  for (i = 0; i < 0x100; i++) {
    ret = map_glyph(font, i, i);
    if (ret != 0)
      return ret;
  }

  return 0;
}

/*
 * Load a font.
 */
int load_font(struct font_t *font, void *font_start, void *font_end)
{
  struct psf2_header_t *header;
  int ret;

  /* check header */
  header = (struct psf2_header_t *) font_start;
  if (header->magic[0] != PSF2_MAGIC0
      || header->magic[1] != PSF2_MAGIC1
      || header->magic[2] != PSF2_MAGIC2
      || header->magic[3] != PSF2_MAGIC3)
      return -EINVAL;

  /* set font */
  font->height = header->height;
  font->width = header->width;
  font->char_size = header->charsize;
  font->char_count = header->length;
  font->uc_size = 0;
  font->data = font_start + header->headersize;

  /* map glyphes to font */
  if (header->flags & PSF2_HAS_UNICODE_TABLE)
    ret = map_unicode(font, header, font_end);
  else
    ret = map_ascii(font);

  return ret;
}

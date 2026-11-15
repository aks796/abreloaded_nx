/* ETC2 RGBA8/EAC -> BC3/DXT5 bridge for the Switch Mesa driver.
 *
 * GM20B can sample BC3 and ASTC, but not ETC2.  Mesa 20.1 therefore expands
 * every Android ETC2_RGBA8 atlas to RGBA8.  Angry Birds Reloaded has many
 * 4096x4096 atlases: each 16 MiB asset becomes a persistent 64 MiB BO and the
 * driver reaches GL_OUT_OF_MEMORY before Level1 can upload its theme.
 *
 * Mesa already contains both pieces needed for a blockwise transcode:
 *   - its ETC2 decoder (local to texcompress_etc.c.o), and
 *   - its public DXT5 pack routine.
 * The link map pins switch-mesa 20.1.0-5.  Makefile exposes the decoder under
 * abr_mesa_unpack_etc2; the final ELF/map must resolve that alias to the same
 * address as the local _mesa_unpack_etc2_format symbol.
 * Processing one 4-pixel block row at a time limits scratch memory to 64 KiB,
 * rather than materialising another 64 MiB RGBA copy.
 *
 * Mesa's decoder and DXT compressor are MIT licensed; this file only calls
 * those routines and contains no game asset data.
 */

#include "etc2_dxt5.h"

#ifdef __SWITCH2__

/* OunceVK's GLES implementation accepts ETC2 natively. The Switch 1
 * Mesa-private decoder is unavailable and unnecessary on Switch 2. */
uint8_t *abr_etc2_rgba8_to_dxt5(const void *src, size_t src_size,
                                unsigned width, unsigned height,
                                int srgb, size_t *dst_size) {
  (void)src; (void)src_size; (void)width; (void)height; (void)srgb;
  if (dst_size) *dst_size = 0;
  return NULL;
}

#else

#include <stdbool.h>
#include <stdlib.h>

/* Private Mesa 20.1 signature.  mesa_format is an enum (ABI: int). */
extern void abr_mesa_unpack_etc2(uint8_t *dst_row, unsigned dst_stride,
                                 const uint8_t *src_row, unsigned src_stride,
                                 unsigned src_width, unsigned src_height,
                                 int format, bool bgra);

extern void util_format_dxt5_rgba_pack_rgba_8unorm(
    uint8_t *dst_row, unsigned dst_stride,
    const uint8_t *src_row, unsigned src_stride,
    unsigned width, unsigned height);

enum {
  /* Exact values from Mesa 20.1's pipe/p_format.h. */
  ABR_MESA_FORMAT_ETC2_RGBA8  = 302,
  ABR_MESA_FORMAT_ETC2_SRGBA8 = 303,
};

uint8_t *abr_etc2_rgba8_to_dxt5(const void *src_void, size_t src_size,
                                unsigned width, unsigned height,
                                int srgb, size_t *dst_size) {
  if (dst_size) *dst_size = 0;
  if (!src_void || !width || !height) return NULL;

  const size_t blocks_x = ((size_t)width + 3) / 4;
  const size_t blocks_y = ((size_t)height + 3) / 4;
  if (!blocks_x || !blocks_y || blocks_x > SIZE_MAX / blocks_y ||
      blocks_x * blocks_y > SIZE_MAX / 16)
    return NULL;

  const size_t out_bytes = blocks_x * blocks_y * 16;
  if (src_size < out_bytes || blocks_x > (size_t)UINT32_MAX / 16)
    return NULL;

  uint8_t *out = (uint8_t *)malloc(out_bytes);
  const size_t padded_width = blocks_x * 4;
  if (padded_width > SIZE_MAX / 16) {
    free(out);
    return NULL;
  }
  uint8_t *rgba_row = (uint8_t *)malloc(padded_width * 16); /* width * 4 rows * RGBA */
  if (!out || !rgba_row) {
    free(rgba_row);
    free(out);
    return NULL;
  }

  const uint8_t *src = (const uint8_t *)src_void;
  const unsigned compressed_stride = (unsigned)(blocks_x * 16);
  const unsigned rgba_stride = (unsigned)(padded_width * 4);
  const int mesa_format = srgb ? ABR_MESA_FORMAT_ETC2_SRGBA8
                               : ABR_MESA_FORMAT_ETC2_RGBA8;

  for (size_t by = 0; by < blocks_y; ++by) {
    /* Decode the complete stored block row, including padding texels on the
     * last partial mip block.  BC3 needs the same 4x4 block footprint. */
    abr_mesa_unpack_etc2(rgba_row, rgba_stride,
                          src + by * compressed_stride, compressed_stride,
                          (unsigned)padded_width, 4, mesa_format, false);
    util_format_dxt5_rgba_pack_rgba_8unorm(
        out + by * compressed_stride, compressed_stride,
        rgba_row, rgba_stride, (unsigned)padded_width, 4);
  }

  free(rgba_row);
  if (dst_size) *dst_size = out_bytes;
  return out;
}

#endif /* __SWITCH2__ */

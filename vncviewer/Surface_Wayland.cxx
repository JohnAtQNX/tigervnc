#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdexcept>

#include <FL/Fl_RGB_Image.H>
#include <FL/fl_draw.H>

#include "Surface.h"

void Surface::clear(unsigned char r, unsigned char g, unsigned char b,
                    unsigned char a)
{
  unsigned char* p = data;
  unsigned char pm_r = (unsigned)r * a / 255;
  unsigned char pm_g = (unsigned)g * a / 255;
  unsigned char pm_b = (unsigned)b * a / 255;

  for (int i = 0; i < w * h; i++) {
    *p++ = pm_r;
    *p++ = pm_g;
    *p++ = pm_b;
    *p++ = a;
  }
}

void Surface::draw(int src_x, int src_y, int dst_x, int dst_y,
                   int dst_w, int dst_h)
{
  // D=3: reads R,G,B from bytes 0,1,2; byte 3 (A) skipped.
  // L=w*4: stride in bytes.
  fl_draw_image(data + (src_y * w + src_x) * 4,
                dst_x, dst_y, dst_w, dst_h, 3, w * 4);
}

void Surface::draw(Surface* dst, int src_x, int src_y,
                   int dst_x, int dst_y, int dst_w, int dst_h)
{
  for (int y = 0; y < dst_h; y++) {
    memcpy(dst->data + ((dst_y + y) * dst->w + dst_x) * 4,
           data     + ((src_y + y) *      w + src_x) * 4,
           dst_w * 4);
  }
}

static void blend_row(const unsigned char* src, unsigned char* dst,
                      int w, int a)
{
  for (int x = 0; x < w; x++) {
    unsigned char sa = (unsigned)src[3] * a / 255;
    dst[0] = (unsigned)src[0] * sa / 255 + (unsigned)dst[0] * (255 - sa) / 255;
    dst[1] = (unsigned)src[1] * sa / 255 + (unsigned)dst[1] * (255 - sa) / 255;
    dst[2] = (unsigned)src[2] * sa / 255 + (unsigned)dst[2] * (255 - sa) / 255;
    dst[3] = sa + (unsigned)dst[3] * (255 - sa) / 255;
    src += 4;
    dst += 4;
  }
}

void Surface::blend(int src_x, int src_y, int dst_x, int dst_y,
                    int dst_w, int dst_h, int a)
{
  // Blend to window: software-blend into a temporary RGB buffer then draw.
  unsigned char* tmp = new unsigned char[dst_w * dst_h * 3];
  unsigned char* out = tmp;

  for (int y = 0; y < dst_h; y++) {
    const unsigned char* row = data + ((src_y + y) * w + src_x) * 4;
    for (int x = 0; x < dst_w; x++) {
      unsigned char sa = (unsigned)row[3] * a / 255;
      *out++ = (unsigned)row[0] * sa / 255;
      *out++ = (unsigned)row[1] * sa / 255;
      *out++ = (unsigned)row[2] * sa / 255;
      row += 4;
    }
  }

  fl_draw_image(tmp, dst_x, dst_y, dst_w, dst_h, 3);
  delete[] tmp;
}

void Surface::blend(Surface* dst, int src_x, int src_y,
                    int dst_x, int dst_y, int dst_w, int dst_h, int a)
{
  for (int y = 0; y < dst_h; y++) {
    blend_row(data      + ((src_y + y) *      w + src_x) * 4,
              dst->data + ((dst_y + y) * dst->w + dst_x) * 4,
              dst_w, a);
  }
}

void Surface::alloc()
{
  data = new unsigned char[w * h * 4]();
}

void Surface::dealloc()
{
  delete[] data;
  data = nullptr;
}

void Surface::update(const Fl_RGB_Image* image)
{
  const unsigned char* in;
  unsigned char* out;

  assert(image->w() == w);
  assert(image->h() == h);

  in  = (const unsigned char*)image->data()[0];
  out = data;

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      switch (image->d()) {
      case 1:
        *out++ = in[0]; *out++ = in[0]; *out++ = in[0]; *out++ = 0xff;
        break;
      case 2:
        *out++ = (unsigned)in[0] * in[1] / 255;
        *out++ = (unsigned)in[0] * in[1] / 255;
        *out++ = (unsigned)in[0] * in[1] / 255;
        *out++ = in[1];
        break;
      case 3:
        *out++ = in[0]; *out++ = in[1]; *out++ = in[2]; *out++ = 0xff;
        break;
      case 4:
        *out++ = (unsigned)in[0] * in[3] / 255;
        *out++ = (unsigned)in[1] * in[3] / 255;
        *out++ = (unsigned)in[2] * in[3] / 255;
        *out++ = in[3];
        break;
      }
      in += image->d();
    }
    if (image->ld() != 0)
      in += image->ld() - image->w() * image->d();
  }
}

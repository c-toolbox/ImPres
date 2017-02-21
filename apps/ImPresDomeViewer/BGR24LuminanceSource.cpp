/*
*  Copyright 2017 Erik Sundén
*
*  Based on ImageReaderSource
*  Copyright 2010-2011 ZXing authors
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "BGR24LuminanceSource.h"
#include <string>
#include <limits>

using zxing::Ref;
using zxing::ArrayRef;
using zxing::LuminanceSource;

inline char BGR24LuminanceSource::convertPixel(char const* pixel_) const {
  unsigned char const* pixel = (unsigned char const*)pixel_;
   // Blue, Green, Red
   // We assume 16 bit values here
   // 0x200 = 1<<9, half an lsb of the result to force rounding
   return (char)((306 * (int)pixel[2] + 601 * (int)pixel[1] +
        117 * (int)pixel[0] + 0x200) >> 10);
}

BGR24LuminanceSource::BGR24LuminanceSource(ArrayRef<char> image_, int width, int height)
    : Super(width, height), image(image_) {}

Ref<LuminanceSource> BGR24LuminanceSource::create(uint8_t* data, int width, int height) {
  zxing::ArrayRef<char> image = zxing::ArrayRef<char>(width * height * 3);
  memcpy(&image[0], &data[0], image->size()*sizeof(uint8_t));
  return Ref<LuminanceSource>(new BGR24LuminanceSource(image, width, height));
}

zxing::ArrayRef<char> BGR24LuminanceSource::getRow(int y, zxing::ArrayRef<char> row) const {
  const char* pixelRow = &image[0] + y * getWidth() * 3;
  if (!row) {
    row = zxing::ArrayRef<char>(getWidth());
  }
  for (int x = 0; x < getWidth(); x++) {
    row[x] = convertPixel(pixelRow + (x * 3));
  }
  return row;
}

zxing::ArrayRef<char> BGR24LuminanceSource::getMatrix() const {
  const char* p = &image[0];
  zxing::ArrayRef<char> matrix(getWidth() * getHeight());
  char* m = &matrix[0];
  for (int y = 0; y < getHeight(); y++) {
    for (int x = 0; x < getWidth(); x++) {
      *m = convertPixel(p);
      m++;
      p += 3;
    }
  }
  return matrix;
}

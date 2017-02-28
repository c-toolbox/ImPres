#ifndef _QR_CODE_INTERPRETER_H_
#define _QR_CODE_INTERPRETER_H_
/*
 *  Copyright 2017 Erik Sundén
 *
 *  Based on main.cpp in zxing/cli
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

#include <zxing/LuminanceSource.h>

class QRCodeInterpreter {
public:
  static std::string decodeImage(zxing::Ref<zxing::LuminanceSource> source, bool print_exceptions = false, bool hybrid = false, bool tryhard = false);
  static std::vector<std::string> decodeImageMulti(zxing::Ref<zxing::LuminanceSource> source, bool print_exceptions = false, bool hybrid = false, bool tryhard = false);
};

#endif /* _QR_CODE_INTERPRETER_H_ */

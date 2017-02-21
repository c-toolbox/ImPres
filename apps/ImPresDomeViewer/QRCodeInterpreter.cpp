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

#include "QRCodeInterpreter.h"
#include <string>
#include <sstream>
#include <sgct.h>
#include <zxing/Binarizer.h>
#include <zxing/Result.h>
#include <zxing/ReaderException.h>
#include <zxing/common/GlobalHistogramBinarizer.h>
#include <zxing/common/HybridBinarizer.h>
#include <zxing/DecodeHints.h>
#include <exception>
#include <zxing/Exception.h>
#include <zxing/common/IllegalArgumentException.h>
#include <zxing/qrcode/QRCodeReader.h>
#include <zxing/multi/qrcode/QRCodeMultiReader.h>

using namespace zxing;
using namespace zxing::qrcode;

std::string QRCodeInterpreter::decodeImage(Ref<LuminanceSource> source, bool print_exceptions, bool hybrid, bool tryhard) {
	try {
		Ref<Binarizer> binarizer;
		if (hybrid) {
			binarizer = new HybridBinarizer(source);
		}
		else {
			binarizer = new GlobalHistogramBinarizer(source);
		}

		DecodeHints hints(DecodeHints::QR_CODE_HINT);
		hints.setTryHarder(tryhard);
		
		Ref<BinaryBitmap> binary(new BinaryBitmap(binarizer));

		Ref<Reader> reader(new QRCodeReader);
		
		Ref<Result> result = reader->decode(binary, hints);
		
		Ref<String> resultText = result->getText();

		/*std::string resultStr = "";
		for(int i=0; i < resultText->size(); ++i)
			resultStr.push_back(resultText->charAt(i));*/

		return resultText->getText();
	}
	catch (const ReaderException& e) {
		if(print_exceptions)
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "zxing::ReaderException: %s\n", std::string(e.what()));
	}
	catch (const zxing::IllegalArgumentException& e) {
		if (print_exceptions)
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "zxing::IllegalArgumentException: %s\n", std::string(e.what()));
	}
	catch (const zxing::Exception& e) {
		if (print_exceptions)
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "zxing::Exception: %s\n", std::string(e.what()));
	}
	catch (const std::exception& e) {
		if (print_exceptions)
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "std::exception: %s\n", std::string(e.what()));
	}

	return "";
}

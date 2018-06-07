/*******************************************************************************

Copyright (c) 2017 Erik Sundén
All rights reserved.

For conditions of distribution and use, see copyright notice in sgct.h

*******************************************************************************/

#ifndef __RGBEASY_CAPTURE_CPU_
#define __RGBEASY_CAPTURE_CPU_

#include <string>
#include <functional>

class RGBEasyCaptureCPU
{
public:
	RGBEasyCaptureCPU();
	~RGBEasyCaptureCPU();
	bool initialize();
	void deinitialize();

	void runCapture();

    void setCaptureHost(std::string hostAdress);
	void setCaptureInput(int input);
    void setCaptureCallback(std::function<void(void* data, unsigned long width, unsigned long height)> cb);

	unsigned long getWidth();
	unsigned long getHeight();

    std::string getCaptureHost() const;

private:
    std::string mCaptureHost;
    int mCaptureInput;
};

#endif
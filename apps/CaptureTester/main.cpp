/*
*  Copyright 2016-2017 Erik Sundén
*
* All rights reserved.
* 
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
* 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
* 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
* IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <map>
#include <sstream>
#include <iterator>
#include <algorithm> //used for transform string to lowercase
#include <sgct.h>
#include <FFmpegCapture.hpp>

#ifdef RGBEASY_ENABLED
#include <RGBEasyCapture.hpp>
#endif

std::string getFileName(const std::string& s) {

	char sep = '/';

#ifdef _WIN32
	sep = '\\';
#endif

	size_t i = s.rfind(sep, s.length());
	if (i != std::string::npos) {
		return(s.substr(i + 1, s.length() - i));
	}

	return("");
}

template<typename Out>
void split(const std::string &s, char delim, Out result) {
	std::stringstream ss;
	ss.str(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		*(result++) = item;
	}
}

std::vector<std::string> split(const std::string &s, char delim) {
	std::vector<std::string> elems;
	split(s, delim, std::back_inserter(elems));
	return elems;
}

sgct::Engine * gEngine;
FFmpegCapture* gFFmpegCapture = NULL;
#ifdef RGBEASY_ENABLED
RGBEasyCapture* gRGBEasyCapture = NULL;
#endif

//sgct callbacks
void myPreSyncFun();
void myPostSyncPreDrawFun();
void myDraw3DFun();
void myDraw2DFun();
void myPostDrawFun();
void myInitOGLFun();
void myEncodeFun();
void myDecodeFun();
void myCleanUpFun();
void myKeyCallback(int key, int action);
void myContextCreationCallback(GLFWwindow * win);

sgct_utils::SGCTPlane * RTsquare = NULL;

GLFWwindow * hiddenFFmpegCaptureWindow;
GLFWwindow * hiddenRGBEasyCaptureWindow;
GLFWwindow * sharedWindow;

//variables to share across cluster
sgct::SharedDouble curr_time(0.0);
sgct::SharedBool info(false);
sgct::SharedBool stats(false);
sgct::SharedFloat fadingTime(2.0f);

//Captures (FFmpegCapture and RGBEasyCapture)
void uploadCaptureData(uint8_t ** data, int width, int height);
void parseArguments(int& argc, char**& argv);
GLuint allocateCaptureTexture();
void ffmpegCaptureLoop();
void startFFmpegCapture();
void stopFFmpegCapture();

#ifdef RGBEASY_ENABLED
void rgbEasyCaptureLoop();
void rgbEasyCapturePollAndDraw();
void rgbEasyRenderToTextureSetup();
void startRGBEasyCapture();
void stopRGBEasyCapture();
#endif

void calculateStats();

struct RT
{
	unsigned int texture;
	unsigned int fbo;
	unsigned int renderBuffer;
	unsigned long width;
	unsigned long height;
};
RT captureRT;

GLint Matrix_Loc = -1;
GLint Matrix_Loc_RT = -1;
GLint ScaleUV_Loc = -1;
GLint OffsetUV_Loc = -1;
GLint flipFrame_Loc = -1;

GLuint ffmpegCaptureTexId = GL_FALSE;

std::thread * ffmpegCaptureThread;
std::thread * rgbEasyCaptureThread;

bool flipFrame = false;
bool ffmpegCaptureRequested = false;
bool rgbEasyCaptureRequested = false;

sgct::SharedBool ffmpegCaptureRunning(true);
sgct::SharedBool rgbEasyCaptureRunning(true);
sgct::SharedInt captureLockStatus(0);
sgct::SharedDouble captureRate(0.0);

int main( int argc, char* argv[] )
{
    //sgct::MessageHandler::instance()->setNotifyLevel(sgct::MessageHandler::NOTIFY_ALL);
    
    gEngine = new sgct::Engine( argc, argv );
    gFFmpegCapture = new FFmpegCapture();
#ifdef RGBEASY_ENABLED
	gRGBEasyCapture = new RGBEasyCapture();
#endif

    // arguments:
    // -host <host which should capture>
    // -video <device name>
    // -option <key> <val>
    // -flip
    // -plane <azimuth> <elevation> <roll>
    //
    // to obtain video device names in windows use:
    // ffmpeg -list_devices true -f dshow -i dummy
    // for mac:
    // ffmpeg -f avfoundation -list_devices true -i ""
    // 
    // to obtain device properties in windows use:
    // ffmpeg -f dshow -list_options true -i video=<device name>
    //
    // For options look at: http://ffmpeg.org/ffmpeg-devices.html

    parseArguments(argc, argv);
    
    gEngine->setInitOGLFunction( myInitOGLFun );
	gEngine->setPreSyncFunction(myPreSyncFun);
	gEngine->setPostSyncPreDrawFunction(myPostSyncPreDrawFun);
    gEngine->setDrawFunction(myDraw3DFun);
    gEngine->setDraw2DFunction(myDraw2DFun);
	gEngine->setPostDrawFunction(myPostDrawFun);
    gEngine->setCleanUpFunction( myCleanUpFun );
    gEngine->setKeyboardCallbackFunction(myKeyCallback);
    gEngine->setContextCreationCallback(myContextCreationCallback);

    if( !gEngine->init( sgct::Engine::OpenGL_3_3_Core_Profile ) )
    {
        delete gEngine;
        return EXIT_FAILURE;
    }

    sgct::SharedData::instance()->setEncodeFunction(myEncodeFun);
    sgct::SharedData::instance()->setDecodeFunction(myDecodeFun);

    // Main loop
    gEngine->render();

	if(ffmpegCaptureRunning.getVal())
		stopFFmpegCapture();

#ifdef RGBEASY_ENABLED
	if (rgbEasyCaptureRunning.getVal())
		stopRGBEasyCapture();
#endif

    // Clean up
    delete gFFmpegCapture;
#ifdef RGBEASY_ENABLED
	delete gRGBEasyCapture;
#endif
    delete gEngine;

    // Exit program
    exit( EXIT_SUCCESS );
}

void myPreSyncFun()
{
	if (gEngine->isMaster())
	{
		curr_time.setVal(sgct::Engine::getTime());
	}
}

void myPostSyncPreDrawFun()
{
	gEngine->setDisplayInfoVisibility(info.getVal());
	gEngine->setStatsGraphVisibility(stats.getVal());

#ifdef RGBEASY_ENABLED
	// Run a poll from the capturing
	// If we are not doing that in the background
	// Storing the result in captureRT.texture
	if (rgbEasyCaptureRunning.getVal()) {
		rgbEasyCapturePollAndDraw();
	}
#endif
}

void myDraw3DFun()
{
    glm::mat4 MVP = gEngine->getCurrentModelViewProjectionMatrix();

	sgct::ShaderManager::instance()->bindShaderProgram("xform");
	glActiveTexture(GL_TEXTURE0);
	if(ffmpegCaptureRequested) {
		glBindTexture(GL_TEXTURE_2D, ffmpegCaptureTexId);
	}
	else if (rgbEasyCaptureRequested) {
		glBindTexture(GL_TEXTURE_2D, captureRT.texture);
	}
	glUniform2f(ScaleUV_Loc, 1.f, 1.f);
	glUniform2f(OffsetUV_Loc, 0.f, 0.f);
	glUniform2f(OffsetUV_Loc, 0.f, 0.f);
	glUniform1i(flipFrame_Loc, flipFrame);
	glUniformMatrix4fv(Matrix_Loc, 1, GL_FALSE, &MVP[0][0]);

    //draw square
	RTsquare->draw();

	//stop capture if necassary
	bool captureStarted = ffmpegCaptureRunning.getVal();
	if (captureStarted && captureLockStatus.getVal() == 1) {
		stopFFmpegCapture();
	}

    sgct::ShaderManager::instance()->unBindShaderProgram();
}

void myDraw2DFun()
{
    if (info.getVal())
    {
        unsigned int font_size = static_cast<unsigned int>(9.0f*gEngine->getCurrentWindowPtr()->getXScale());
        sgct_text::Font * font = sgct_text::FontManager::instance()->getFont("SGCTFont", font_size);
        float padding = 10.0f;

        sgct_text::print(font, sgct_text::TOP_LEFT,
            padding, static_cast<float>(gEngine->getCurrentWindowPtr()->getYFramebufferResolution() - font_size) - padding, //x and y pos
            glm::vec4(1.0, 1.0, 1.0, 1.0), //color
            "Format: %s\nResolution: %d x %d\nRate: %.2lf Hz",
            gFFmpegCapture->getFormat(),
            gFFmpegCapture->getWidth(),
            gFFmpegCapture->getHeight(),
            captureRate.getVal());
    }
}

void myPostDrawFun()
{
}

void startFFmpegCapture()
{
	//start capture thread if host or load thread if master and not host
	sgct_core::SGCTNode * thisNode = sgct_core::ClusterManager::instance()->getThisNodePtr();
	if (thisNode->getAddress() == gFFmpegCapture->getVideoHost()) {
		ffmpegCaptureRunning.setVal(true);
		ffmpegCaptureThread = new (std::nothrow) std::thread(ffmpegCaptureLoop);
	}
}

void stopFFmpegCapture()
{
	//kill capture thread
	ffmpegCaptureRunning.setVal(false);
	if (ffmpegCaptureThread)
	{
		ffmpegCaptureThread->join();
		delete ffmpegCaptureThread;
	}
}

#ifdef RGBEASY_ENABLED
void startRGBEasyCapture()
{
	//start capture thread if host or load thread if master and not host
	sgct_core::SGCTNode * thisNode = sgct_core::ClusterManager::instance()->getThisNodePtr();	
	if (thisNode->getAddress() == gRGBEasyCapture->getCaptureHost()) {
		rgbEasyCaptureRunning.setVal(true);
		rgbEasyCaptureThread = new (std::nothrow) std::thread(rgbEasyCaptureLoop);
	}
}

void stopRGBEasyCapture()
{
	//kill capture thread
	rgbEasyCaptureRunning.setVal(false);
	if (rgbEasyCaptureThread)
	{
		rgbEasyCaptureThread->join();
		delete rgbEasyCaptureThread;
	}
}

void rgbEasyCapturePollAndDraw() {
	if (gRGBEasyCapture->prepareForRendering()) {
		//Rendering to square texture when assumig ganing (i.e. from 2x1 to 1x2) as 1x2 does not seem to function properly
		if (gRGBEasyCapture->getGanging()) {
			sgct::ShaderManager::instance()->bindShaderProgram("sbs2tb");

			//transform
			glm::mat4 planeTransform = glm::mat4(1.0f);
			glUniformMatrix4fv(Matrix_Loc, 1, GL_FALSE, &planeTransform[0][0]);
		}
		else {
			sgct::ShaderManager::instance()->bindShaderProgram("xform");

			//transform
			glm::mat4 planeTransform = glm::mat4(1.0f);
			glUniform1i(flipFrame_Loc, false);
			glUniformMatrix4fv(Matrix_Loc, 1, GL_FALSE, &planeTransform[0][0]);
		}

		sgct_core::OffScreenBuffer * fbo = gEngine->getCurrentFBO();

		//get viewport data and set the viewport
		glViewport(0, 0, captureRT.width, captureRT.height);

		//bind fbo
		glBindFramebuffer(GL_FRAMEBUFFER, captureRT.fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, captureRT.texture, 0);

		glCullFace(GL_BACK);

		//draw square
		RTsquare->draw();

		sgct::ShaderManager::instance()->unBindShaderProgram();

		//give capture buffer back to RGBEasy
		gRGBEasyCapture->renderingCompleted();

		//restore
		if (fbo)
			fbo->bind();
		sgct::ShaderManager::instance()->bindShaderProgram("xform");
		const int * coords = gEngine->getCurrentViewportPixelCoords();
		glViewport(coords[0], coords[1], coords[2], coords[3]);
	}
}

void rgbEasyCaptureLoop() {
	gRGBEasyCapture->runCapture();
	while (rgbEasyCaptureRunning.getVal()) {
		// Frame capture running...
	}
}

void rgbEasyRenderToTextureSetup() {
	// check if we are ganing inputs
	// thus we assume 2x1 (sbs) which we want to change to 1x2 (tb)
	if (gRGBEasyCapture->getGanging()) {
		captureRT.width = gRGBEasyCapture->getWidth() / 2;
		captureRT.height = gRGBEasyCapture->getHeight() * 2;
	}
	else {
		captureRT.width = gRGBEasyCapture->getWidth();
		captureRT.height = gRGBEasyCapture->getHeight();
	}

	captureRT.fbo = GL_FALSE;
	captureRT.renderBuffer = GL_FALSE;
	captureRT.texture = GL_FALSE;

	//create targets
	glEnable(GL_TEXTURE_2D);

	glGenTextures(1, &(captureRT.texture));
	glBindTexture(GL_TEXTURE_2D, captureRT.texture);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, captureRT.width, captureRT.height);

	//---------------------
	// Disable mipmaps
	//---------------------
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, captureRT.width, captureRT.height, 0, GL_BGR, GL_UNSIGNED_BYTE, NULL);

	gEngine->checkForOGLErrors();

	glBindTexture(GL_TEXTURE_2D, GL_FALSE);

	glGenFramebuffers(1, &(captureRT.fbo));
	glGenRenderbuffers(1, &(captureRT.renderBuffer));

	//setup color buffer
	glBindFramebuffer(GL_FRAMEBUFFER, captureRT.fbo);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRT.renderBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB, captureRT.width, captureRT.height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, captureRT.renderBuffer);

	//Does the GPU support current FBO configuration?
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		sgct::MessageHandler::instance()->print("Something went wrong creating FBO!\n");

	gEngine->checkForOGLErrors();

	//unbind
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
#endif

void myInitOGLFun()
{
	if (ffmpegCaptureRequested) {
		bool captureReady = gFFmpegCapture->init();
		//allocate texture
		if (captureReady) {
			ffmpegCaptureTexId = allocateCaptureTexture();
		}

		if (captureReady) {
			//start capture
			startFFmpegCapture();
		}
	}

#ifdef RGBEASY_ENABLED
	// due a high-frame rate rgbEasy capturing (if requested)
	if (rgbEasyCaptureRequested) {
		sgct_core::SGCTNode * thisNode = sgct_core::ClusterManager::instance()->getThisNodePtr();
		captureRT.texture = GL_FALSE;
		if (thisNode->getAddress() == gRGBEasyCapture->getCaptureHost()) {
			if (gRGBEasyCapture->initialize()) {

				// Perform Capture OpenGL operations on MAIN thread
				// initalize capture OpenGL (running on this thread)
				gRGBEasyCapture->initializeGL();
				rgbEasyRenderToTextureSetup();

				// Perform frame capture on BACKGROUND thread
				// start capture thread (which will intialize RGBEasy things)
				startRGBEasyCapture();
			}
		}
	}
#endif

    std::function<void(uint8_t ** data, int width, int height)> callback = uploadCaptureData;
    gFFmpegCapture->setVideoDecoderCallback(callback);

	//create RT square
	RTsquare = new sgct_utils::SGCTPlane(2.0f, 2.0f);

    sgct::ShaderManager::instance()->addShaderProgram( "xform",
            "xform.vert",
            "xform.frag" );

    sgct::ShaderManager::instance()->bindShaderProgram( "xform" );

    Matrix_Loc = sgct::ShaderManager::instance()->getShaderProgram( "xform").getUniformLocation( "MVP" );
    ScaleUV_Loc = sgct::ShaderManager::instance()->getShaderProgram( "xform").getUniformLocation("scaleUV");
    OffsetUV_Loc = sgct::ShaderManager::instance()->getShaderProgram( "xform").getUniformLocation("offsetUV");
	flipFrame_Loc = sgct::ShaderManager::instance()->getShaderProgram("xform").getUniformLocation("flipFrame");
    GLint Tex_Loc = sgct::ShaderManager::instance()->getShaderProgram( "xform").getUniformLocation( "Tex" );
    glUniform1i( Tex_Loc, 0 );

	sgct::ShaderManager::instance()->unBindShaderProgram();

    sgct::Engine::checkForOGLErrors();
}

void myEncodeFun()
{
    sgct::SharedData::instance()->writeDouble(&curr_time);
    sgct::SharedData::instance()->writeBool(&info);
    sgct::SharedData::instance()->writeBool(&stats);
}

void myDecodeFun()
{
    sgct::SharedData::instance()->readDouble(&curr_time);
    sgct::SharedData::instance()->readBool(&info);
    sgct::SharedData::instance()->readBool(&stats);
}

void myCleanUpFun()
{
	if (RTsquare)
		delete RTsquare;

    if (ffmpegCaptureTexId)
    {
        glDeleteTextures(1, &ffmpegCaptureTexId);
		ffmpegCaptureTexId = GL_FALSE;
    }
     
    if(hiddenFFmpegCaptureWindow)
        glfwDestroyWindow(hiddenFFmpegCaptureWindow);
}

void myKeyCallback(int key, int action)
{
    if (gEngine->isMaster())
    {
		switch (key)
		{
		case SGCT_KEY_S:
			if (action == SGCT_PRESS)
				stats.toggle();
			break;

		case SGCT_KEY_I:
			if (action == SGCT_PRESS)
				info.toggle();
			break;
		}
    }
}

void myContextCreationCallback(GLFWwindow * win)
{
    glfwWindowHint( GLFW_VISIBLE, GL_FALSE );
    
    sharedWindow = win;

    hiddenFFmpegCaptureWindow = glfwCreateWindow( 1, 1, "Thread Capture Window", NULL, sharedWindow ); 
    if( !hiddenFFmpegCaptureWindow)
    {
        sgct::MessageHandler::instance()->print("Failed to create capture context!\n");
    }
    
    //restore to normal
    glfwMakeContextCurrent( sharedWindow );
}

void parseArguments(int& argc, char**& argv)
{
    int i = 0;
    while (i < argc)
    {
        if (strcmp(argv[i], "-host") == 0 && argc > (i + 1))
        {
            gFFmpegCapture->setVideoHost(std::string(argv[i + 1]));
#ifdef RGBEASY_ENABLED
			gRGBEasyCapture->setCaptureHost(std::string(argv[i + 1]));
#endif
        }
		else if (strcmp(argv[i], "-ffmpegcapture") == 0)
		{
			ffmpegCaptureRequested = true;
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "FFmpeg capture requested\n");
		}
        else if (strcmp(argv[i], "-video") == 0 && argc > (i + 1))
        {
            gFFmpegCapture->setVideoDevice(std::string(argv[i + 1]));
        }
		else if (strcmp(argv[i], "-option") == 0 && argc > (i + 2))
		{
			std::string option = std::string(argv[i + 1]);
			std::string value = std::string(argv[i + 2]);
			gFFmpegCapture->addOption(std::make_pair(option, value));
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Added capture option %s, parameter %s\n", option.c_str(), value.c_str());
		}
		else if (strcmp(argv[i], "-flip") == 0)
		{
			flipFrame = true;
		}
#ifdef RGBEASY_ENABLED
		else if (strcmp(argv[i], "-rgbeasycapture") == 0)
		{
			rgbEasyCaptureRequested = true;
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "RGBEasy capture requested\n");
		}
		else if (strcmp(argv[i], "-rgbeasyinput") == 0 && argc >(i + 1))
		{
			int captureInput = static_cast<int>(atoi(argv[i + 1]));
			gRGBEasyCapture->setCaptureInput(captureInput);
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "RGBEasy capture on input %i\n", captureInput);

		}
		else if (strcmp(argv[i], "-rgbeasyganging") == 0)
		{
			gRGBEasyCapture->setCaptureGanging(true);
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "RGBEasy/capture ganging enabled\n");
		}
#endif

        i++; //iterate
    }
}

GLuint allocateCaptureTexture()
{
    int w = gFFmpegCapture->getWidth();
    int h = gFFmpegCapture->getHeight();

    if (w * h <= 0)
    {
        sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Invalid texture size (%dx%d)!\n", w, h);
        return 0;
    }
	sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Capture texture size (%dx%d)!\n", w, h);

	GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, w, h);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return texId;
}

void uploadCaptureData(uint8_t ** data, int width, int height)
{
    // At least two textures and GLSync objects
    // should be used to control that the uploaded texture is the same
    // for all viewports to prevent any tearing and maintain frame sync

	if (ffmpegCaptureTexId)
	{
		unsigned char * GPU_ptr = reinterpret_cast<unsigned char*>(glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));
		if (GPU_ptr)
		{
			int dataOffset = 0;
			int stride = width * 3; //Assuming BGR24
			/*if (gFFmpegCapture->isFormatYUYV422()) {
				stride = width * 2;
			}*/

			for (int row = height - 1; row > -1; row--)
			{
				memcpy(GPU_ptr + dataOffset, data[0] + row * stride, stride);
				dataOffset += stride;
			}

			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, ffmpegCaptureTexId);

			/*if (gFFmpegCapture->isFormatYUYV422()) {
				//AV_PIX_FMT_YUYV422
				//int y1, u, y2, v;
				glTexImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, 0);
			}
			else { //Assuming BGR24*/
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, 0);
			//}
		}

		//calculateStats();
	}
}

void ffmpegCaptureLoop()
{
    glfwMakeContextCurrent(hiddenFFmpegCaptureWindow);

    int dataSize = gFFmpegCapture->getWidth() * gFFmpegCapture->getHeight() * 3;
    GLuint PBO;
    glGenBuffers(1, &PBO);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, 0, GL_DYNAMIC_DRAW);

    while (ffmpegCaptureRunning.getVal())
    {
		gFFmpegCapture->poll();
		sgct::Engine::sleep(0.02); //take a short break to offload the cpu
    }

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    glDeleteBuffers(1, &PBO);

    glfwMakeContextCurrent(NULL); //detach context
}

void calculateStats()
{
    double timeStamp = sgct::Engine::getTime();
    static double previousTimeStamp = timeStamp;
    static double numberOfSamples = 0.0;
    static double duration = 0.0;

    timeStamp = sgct::Engine::getTime();
    duration += timeStamp - previousTimeStamp;
    previousTimeStamp = timeStamp;
    numberOfSamples++;

    if (duration >= 1.0)
    {
        captureRate.setVal(numberOfSamples / duration);
        duration = 0.0;
        numberOfSamples = 0.0;
    }
}
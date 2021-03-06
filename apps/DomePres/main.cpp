/*
*  Copyright 2016-2017 Erik Sund�n
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
#include <RGBEasyCaptureCPU.hpp>
#include <RGBEasyCaptureGPU.hpp>
#endif

#ifdef ZXING_ENABLED
#include <BGR24LuminanceSource.h>
#include <QRCodeInterpreter.h>
#endif

#ifdef OPENVR_SUPPORT
#include <SGCTOpenVR.h>
sgct::SGCTWindow* FirstOpenVRWindow = NULL;
#endif

#include <imgui.h>
#include <imgui_impl_glfw_gl3.h>
namespace ImGui
{
	static auto vector_getter = [](void* vec, int idx, const char** out_text)
	{
		auto& vector = *static_cast<std::vector<std::string>*>(vec);
		if (idx < 0 || idx >= static_cast<int>(vector.size())) { return false; }
		*out_text = vector.at(idx).c_str();
		return true;
	};

	bool Combo(const char* label, int* currIndex, std::vector<std::string>& values)
	{
		if (values.empty()) { return false; }
		return Combo(label, currIndex, vector_getter,
			static_cast<void*>(&values), static_cast<int>(values.size()));
	}

	bool ListBox(const char* label, int* currIndex, std::vector<std::string>& values)
	{
		if (values.empty()) { return false; }
		return ListBox(label, currIndex, vector_getter,
			static_cast<void*>(&values), static_cast<int>(values.size()));
	}

}
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
FFmpegCapture* gPlaneCapture = NULL;
#ifdef RGBEASY_ENABLED
RGBEasyCaptureCPU* gPlaneDPCapture = NULL;
RGBEasyCaptureGPU* gFisheyeCapture = NULL;
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
void myCharCallback(unsigned int c);
void myMouseButtonCallback(int button, int action);
void myMouseScrollCallback(double xoffset, double yoffset);
void myContextCreationCallback(GLFWwindow * win);

std::vector<sgct_utils::SGCTPlane *> captureContentPlanes;
std::vector<sgct_utils::SGCTPlane *> masterContentPlanes;
sgct_utils::SGCTDome * dome = NULL;
sgct_utils::SGCTPlane * RTsquare = NULL;

GLFWwindow * hiddenPlaneCaptureWindow;
GLFWwindow * hiddenPlaneDPCaptureWindow;
GLFWwindow * hiddenTransferWindow;
GLFWwindow * sharedWindow;

//variables to share across cluster
sgct::SharedDouble curr_time(0.0);
sgct::SharedBool info(false);
sgct::SharedBool stats(false);
sgct::SharedFloat fadingTime(2.0f);

//structs
struct ContentPlaneGlobalAttribs {
	std::string name;
	float height;
	float azimuth;
	float elevation;
	float roll;
	float distance;
	int planeStrId;
	int planeTexId;

	ContentPlaneGlobalAttribs(std::string n, float h, float a, float e, float r, float d, int pls = 0, int pli = 0) :
		name(n),
		height(h), 
		azimuth(a) ,
		elevation(e) ,
		roll(r) ,
		distance (d),
		planeStrId(pls) ,
		planeTexId(pli)
	{};
};

struct ContentPlaneLocalAttribs {
	std::string name;
	bool currentlyVisible;
	bool previouslyVisible;
	double fadeStartTime;
	bool freeze;

	ContentPlaneLocalAttribs(std::string n, bool ca = true, bool pa = true, double f = -1.0, bool fr = false) :
		name(n),
		currentlyVisible(ca),
		previouslyVisible(pa),
		fadeStartTime(f),
		freeze(fr)
	{};
};

struct ContentPlane {
	std::string name;
	float height;
	float azimuth;
	float elevation;
	float roll;
	float distance;
	bool currentlyVisible;
	bool previouslyVisible;
	double fadeStartTime;
	int planeStrId;
	int planeTexId;
	bool freeze;

	ContentPlane(std::string n, float h, float a, float e, float r, float d, bool ca = true, bool pa = true, double f = -1.0, int pls = 0, int pli = 0, bool fr = false) :
		name(n),
		height(h),
		azimuth(a),
		elevation(e),
		roll(r),
		distance(d),
		currentlyVisible(ca),
		previouslyVisible(pa),
		fadeStartTime(f),
		planeStrId(pls),
		planeTexId(pli),
		freeze(fr)
	{};

	ContentPlaneGlobalAttribs getGlobal() {
		return ContentPlaneGlobalAttribs(name, height, azimuth, elevation, roll, distance, planeStrId, planeTexId);
	}

	ContentPlaneLocalAttribs getLocal() {
		return ContentPlaneLocalAttribs(name, currentlyVisible, previouslyVisible, fadeStartTime, freeze);
	}
};

//DomeImageViewer
void myDropCallback(int count, const char** paths);
void myDataTransferDecoder(void * receivedData, int receivedlength, int packageId, int clientIndex);
void myDataTransferStatus(bool connected, int clientIndex);
void myDataTransferAcknowledge(int packageId, int clientIndex);
void transferSupportedFiles(std::string pathStr);
void startDataTransfer();
void readImage(unsigned char * data, int len);
void uploadTexture();
void threadWorker();

std::thread * loadThread;
std::mutex mutex;
std::vector<sgct_core::Image *> transImages;

sgct::SharedInt32 domeTexIndex(-1);
sgct::SharedInt32 incrIndex(1);
sgct::SharedInt32 numSyncedTex(0);
int currentDomeTexIdx = -1;
int previousDomeTexIndex = 0;
double domeBlendStartTime = -1.0;
bool takeScreenshot = false;
bool screenshotPassOn = false;

std::vector<std::pair<std::string, int>> imagePathsVec;
std::map<std::string, int> imagePathsMap;
std::vector<std::string> domeImageFileNames;
std::vector<std::string> planeImageFileNames;
std::vector<std::string> defaultFisheyes;
double defaultFisheyeDelay = 0.0;
double defaultFisheyeTime = 0.0;

sgct::SharedBool running(true);
sgct::SharedInt32 lastPackage(-1);
sgct::SharedBool transfer(false);
sgct::SharedBool serverUploadDone(false);
sgct::SharedInt32 serverUploadCount(0);
sgct::SharedBool clientsUploadDone(false);
sgct::SharedVector<GLuint> texIds;
sgct::SharedVector<float> texAspectRatio;
double sendTimer = 0.0;

enum imageType { IM_JPEG, IM_PNG };
const int headerSize = 1;

//Captures (FFmpegCapture and RGBEasyCaptureGPU)
void uploadCaptureData(uint8_t ** data, int width, int height);
void parseArguments(int& argc, char**& argv);
GLuint allocateCaptureTexture();
void planeCaptureLoop();
void calculateStats();
void startPlaneCapture();
void stopPlaneCapture();
void updateCapturePlaneTexIDs();
void allocateCapturePlanes();
void createPlanes();

struct RT
{
	unsigned int texture;
	unsigned int fbo;
	unsigned int renderBuffer;
	unsigned long width;
	unsigned long height;
};
RT fisheyeCaptureRT;

#ifdef RGBEASY_ENABLED
void uploadRGBEasyCapturePlaneData(void* data, unsigned long width, unsigned long height);
void planeDPCaptureLoop();
void startPlaneDPCapture();
void stopPlaneDPCapture();

void fisheyeCaptureLoop();
void startFisheyeCapture();
void stopFisheyeCapture();
void RGBEasyCaptureGPUPollAndDraw(RGBEasyCaptureGPU* capture, RT& captureRT);
void RGBEasyRenderToTextureSetup(RGBEasyCaptureGPU* capture, RT& captureRT);
#endif

#ifdef ZXING_ENABLED
bool checkQRoperations(uint8_t** data, int width, int height, bool flipped);
#endif

GLint Matrix_Loc = -1;
GLint Matrix_Loc_RT = -1;
GLint ScaleUV_Loc = -1;
GLint OffsetUV_Loc = -1;
GLint Opacity_Loc = -1;
GLint flipFrame_Loc = -1;
GLint Matrix_Loc_BLEND = -1;
GLint ScaleUV_Loc_BLEND = -1;
GLint OffsetUV_Loc_BLEND = -1;
GLint TexMix_Loc_BLEND = -1;
GLint Matrix_Loc_CK = -1;
GLint ScaleUV_Loc_CK = -1;
GLint OffsetUV_Loc_CK = -1;
GLint Opacity_Loc_CK = -1;
GLint ChromaKeyColor_Loc_CK = -1;
GLint ChromaKeyFactor_Loc_CK = -1;

GLuint planeCaptureTexId = GL_FALSE;
GLuint planeDPCaptureTexId = GL_FALSE;
GLuint planeDPCapturePBO = GL_FALSE;
int planceCaptureWidth = 0;
int planeCaptureHeight = 0;

std::thread * planeCaptureThread;
std::thread * planeDPCaptureThread;
std::thread * fisheyeCaptureThread;
bool flipFrame = false;
bool fulldomeMode = false;
bool planeDPCaptureRequested = false;
bool fisheyeCaptureRequested = false;
ContentPlaneLocalAttribs fullDomeAttribs = ContentPlaneLocalAttribs("fullDome");
glm::vec2 planeScaling(1.0f, 1.0f);
glm::vec2 planeOffset(0.0f, 0.0f);

sgct::SharedBool planeCaptureRunning(true);
sgct::SharedBool planeDPCaptureRunning(true);
sgct::SharedBool fisheyeCaptureRunning(true);
sgct::SharedInt captureLockStatus(0);
sgct::SharedBool renderDome(fulldomeMode);
sgct::SharedDouble captureRate(0.0);
sgct::SharedInt32 domeCut(2);
sgct::SharedInt planeScreenAspect(1610);
sgct::SharedInt planeMaterialAspect(169);
sgct::SharedBool planeCapturePresMode(false);
sgct::SharedBool planeUseCaptureSize(false);
sgct::SharedVector<ContentPlaneGlobalAttribs> planeAttributesGlobal;
sgct::SharedVector<ContentPlaneLocalAttribs> planeAttributesLocal;
sgct::SharedBool planeReCreate(false);
sgct::SharedBool chromaKey(false);
sgct::SharedObject<glm::vec3> chromaKeyColor(glm::vec3(0.f, 177.f, 64.f));
sgct::SharedFloat chromaKeyFactor(22.f);

std::vector<GLuint> planeTexOwnedIds;

#ifdef ZXING_ENABLED
std::vector<std::string> operationsQueue;
#endif

//ImGUI variables
std::vector<std::string> imPresets;
int imPresetIdx = 0;
std::vector<std::string> imPlanes;
int imPlaneIdx = 0;
int imPlanePreviousIdx = 0;
int imPlaneScreenAspect = 1610;
int imPlaneMaterialAspect = 169;
bool imPlaneCapturePresMode = false;
bool imPlaneUseCaptureSize = false;
float imPlaneHeight = 3.5f;
float imPlaneAzimuth = 0.0f;
float imPlaneElevation = 35.0f;
float imPlaneRoll = 0.0f;
float imPlaneDistance = -5.5f;
bool imPlaneShow = true;
int imPlaneImageIdx = 0;
float imFadingTime = 2.0f;
bool imChromaKey = false;
ImVec4 imChromaKeyColor = ImColor(0, 190, 0);
float imChromaKeyFactor = 22.f;

int main( int argc, char* argv[] )
{
    //sgct::MessageHandler::instance()->setNotifyLevel(sgct::MessageHandler::NOTIFY_ALL);
    
    gEngine = new sgct::Engine( argc, argv );
    gPlaneCapture = new FFmpegCapture();
#ifdef RGBEASY_ENABLED
    gPlaneDPCapture = new RGBEasyCaptureCPU();
	gFisheyeCapture = new RGBEasyCaptureGPU();
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
	gEngine->setCharCallbackFunction(myCharCallback);
	gEngine->setMouseButtonCallbackFunction(myMouseButtonCallback);
	gEngine->setMouseScrollCallbackFunction(myMouseScrollCallback);
    gEngine->setContextCreationCallback(myContextCreationCallback);
    gEngine->setDropCallbackFunction(myDropCallback);

    if( !gEngine->init( sgct::Engine::OpenGL_3_3_Core_Profile ) )
    {
        delete gEngine;
        return EXIT_FAILURE;
    }
    
    gEngine->setDataTransferCallback(myDataTransferDecoder);
    gEngine->setDataTransferStatusCallback(myDataTransferStatus);
    gEngine->setDataAcknowledgeCallback(myDataTransferAcknowledge);
    //gEngine->setDataTransferCompression(true, 6);

    //sgct::SharedData::instance()->setCompression(true);
    sgct::SharedData::instance()->setEncodeFunction(myEncodeFun);
    sgct::SharedData::instance()->setDecodeFunction(myDecodeFun);

    // Main loop
    gEngine->render();

#ifdef OPENVR_SUPPORT
	// Clean up OpenVR
	sgct::SGCTOpenVR::shutdown();
#endif

	if(planeCaptureRunning.getVal())
		stopPlaneCapture();

#ifdef RGBEASY_ENABLED
	if (planeDPCaptureRunning.getVal())
		stopPlaneDPCapture();

	if (fisheyeCaptureRunning.getVal())
		stopFisheyeCapture();
#endif

    running.setVal(false);
    if (loadThread)
    {
        loadThread->join();
        delete loadThread;
    }

	// Clean up
	if (gEngine->isMaster())
		ImGui_ImplGlfwGL3_Shutdown();

    // Clean up
    delete gPlaneCapture;
#ifdef RGBEASY_ENABLED
	delete gPlaneDPCapture;
	delete gFisheyeCapture;
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

		//load one default fisheye at certain intervals, if their are any
		if (!defaultFisheyes.empty()) {
			if (defaultFisheyeTime == 0) {
				defaultFisheyeTime = curr_time.getVal() + defaultFisheyeDelay;
			}
			else if (curr_time.getVal() > defaultFisheyeTime) {
				serverUploadCount.setVal(0);
				transferSupportedFiles(defaultFisheyes.at(0));
				defaultFisheyes.erase(defaultFisheyes.begin());
				/*for each (std::string defaultFisheye in defaultFisheyes) {
				transferSupportedFiles(defaultFisheye);
				}
				defaultFisheyes.clear();*/
				defaultFisheyeTime = 0;

			}
		}

		//if texture is uploaded then iterate the index
		if (serverUploadDone.getVal() && clientsUploadDone.getVal())
		{
			numSyncedTex = static_cast<int32_t>(texIds.getSize());

			//only iterate up if we have no image
			if (domeTexIndex < 0) {
				domeTexIndex = numSyncedTex - serverUploadCount.getVal();
				currentDomeTexIdx = domeTexIndex.getVal();
			}

			serverUploadDone = false;
			clientsUploadDone = false;
		}
	}

	if (screenshotPassOn) {
		screenshotPassOn = false;
	}
}

void myPostSyncPreDrawFun()
{
	gEngine->setDisplayInfoVisibility(info.getVal());
	gEngine->setStatsGraphVisibility(stats.getVal());

	if (takeScreenshot) {
		gEngine->takeScreenshot();
		takeScreenshot = false;
		screenshotPassOn = true;
	}

	if (planeReCreate.getVal())
		createPlanes();

#ifdef RGBEASY_ENABLED
	// Run a poll from the capturing
	// If we are not doing that in the background
	if (fisheyeCaptureRunning.getVal()) {
		RGBEasyCaptureGPUPollAndDraw(gFisheyeCapture, fisheyeCaptureRT);
	}
#endif

#ifdef OPENVR_SUPPORT
	if (FirstOpenVRWindow) {
		//Update pose matrices for all tracked OpenVR devices once per frame
		sgct::SGCTOpenVR::updatePoses();
	}
#endif
}

float getContentPlaneOpacity(int planeIdx) {
    //If planeIdx=-1 we assume the attribs fulldome view is asked for
	std::vector<ContentPlaneLocalAttribs> pA = planeAttributesLocal.getVal();
    ContentPlaneLocalAttribs pl = ContentPlaneLocalAttribs("dummy");

    if (planeIdx>=0){
        pl = pA[planeIdx];
    }
    else {
        pl = fullDomeAttribs;
    }

	float planeOpacity = 1.f;

	if (pl.currentlyVisible != pl.previouslyVisible && pl.fadeStartTime == -1.0) {
		pl.fadeStartTime = curr_time.getVal();
		pl.previouslyVisible = pl.currentlyVisible;
		
        if (planeIdx >= 0) {
		    pA[planeIdx] = pl;
		    planeAttributesLocal.setVal(pA);
        }
        else {
            fullDomeAttribs = pl;
        }
	}

	if (pl.fadeStartTime != -1.0) {
		if (pl.currentlyVisible) {
			planeOpacity = static_cast<float>((curr_time.getVal() - pl.fadeStartTime)) / fadingTime.getVal();
		}
		else {
			planeOpacity = 1.f - (static_cast<float>((curr_time.getVal() - pl.fadeStartTime)) / fadingTime.getVal());
		}

		bool abort = false;
		if (planeOpacity < 0.f) {
			planeOpacity = 0.f;
			abort = true;
		}
		else if (planeOpacity > 1.f) {
			planeOpacity = 1.f;
			abort = true;
		}

		if (abort) {
			pl.fadeStartTime = -1.0;
			if (planeIdx >= 0) {
		        pA[planeIdx] = pl;
		        planeAttributesLocal.setVal(pA);
            }
            else {
                fullDomeAttribs = pl;
            }
		}
	}
	else if (!pl.currentlyVisible) {
		planeOpacity = 0.f;
	}

	return planeOpacity;
}

void myDraw3DFun()
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

#ifdef OPENVR_SUPPORT
	glm::mat4 MVP;
	if (sgct::SGCTOpenVR::isHMDActive() &&
		(FirstOpenVRWindow == gEngine->getCurrentWindowPtr() || gEngine->getCurrentWindowPtr()->checkIfTagExists("OpenVR"))) {
		MVP = sgct::SGCTOpenVR::getHMDCurrentViewProjectionMatrix(gEngine->getCurrentFrustumMode());

		if (gEngine->getCurrentFrustumMode() == sgct_core::Frustum::MonoEye) {
			//Reversing rotation around z axis (so desktop view is more pleasent to look at).
			glm::quat inverserotation = sgct::SGCTOpenVR::getInverseRotation(sgct::SGCTOpenVR::getHMDPoseMatrix());
			inverserotation.x = inverserotation.y = 0.f;
			MVP *= glm::mat4_cast(inverserotation);
		}

		//Tilt dome
		glm::mat4 tiltMat = glm::mat4(glm::rotate(glm::mat4(1.0f), glm::radians(-27.f), glm::vec3(1.0f, 0.0f, 0.0f)));
		MVP *= tiltMat;
	}
	else {
		MVP = gEngine->getCurrentModelViewProjectionMatrix();
	}
#else
    glm::mat4 MVP = gEngine->getCurrentModelViewProjectionMatrix();
#endif

    //Set up backface culling
    glCullFace(GL_BACK);

    fullDomeAttribs.currentlyVisible = fulldomeMode;
    float fulldomeOpacity = getContentPlaneOpacity(-1);

    if (domeTexIndex.getVal() != -1
        && fulldomeOpacity <= 0.f)  // && texIds.getSize() > domeTexIndex.getVal())
    {
		float mix = -1;
		if (previousDomeTexIndex != domeTexIndex.getVal() && domeBlendStartTime == -1.0) {
			domeBlendStartTime = curr_time.getVal();
		}
		if (domeBlendStartTime != -1) {
			mix = static_cast<float>((curr_time.getVal() - domeBlendStartTime)) / fadingTime.getVal();
			if (mix > 1) {
				domeBlendStartTime = -1.0;
				previousDomeTexIndex = domeTexIndex.getVal();
			}
		}

		if(mix != -1){
			sgct::ShaderManager::instance()->bindShaderProgram("textureblend");
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texIds.getValAt(previousDomeTexIndex));
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, texIds.getValAt(domeTexIndex.getVal()));
			glUniform2f(ScaleUV_Loc_BLEND, 1.f, 1.f);
			glUniform2f(OffsetUV_Loc_BLEND, 0.f, 0.f);
			glUniformMatrix4fv(Matrix_Loc_BLEND, 1, GL_FALSE, &MVP[0][0]);
			glUniform1f(TexMix_Loc_BLEND, mix);
		}
		else {
			sgct::ShaderManager::instance()->bindShaderProgram("flipxform");
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texIds.getValAt(domeTexIndex.getVal()));
			glUniform2f(ScaleUV_Loc, 1.f, 1.f);
			glUniform2f(OffsetUV_Loc, 0.f, 0.f);
            glUniform1i(flipFrame_Loc, 0);
			glUniform1f(Opacity_Loc, 1.f);
			glUniformMatrix4fv(Matrix_Loc, 1, GL_FALSE, &MVP[0][0]);
		}

        glFrontFace(GL_CW);

        dome->draw();
        sgct::ShaderManager::instance()->unBindShaderProgram();

    }

    GLint ScaleUV_L = ScaleUV_Loc;
    GLint OffsetUV_L = OffsetUV_Loc;
    GLint Matrix_L = Matrix_Loc;
	GLint Opacity_L = Opacity_Loc;
    if (chromaKey.getVal())
    {
        sgct::ShaderManager::instance()->bindShaderProgram("chromakey");
        glUniform3f(ChromaKeyColor_Loc_CK
            , chromaKeyColor.getVal().r
            , chromaKeyColor.getVal().g
            , chromaKeyColor.getVal().b);
		glUniform1f(ChromaKeyFactor_Loc_CK, chromaKeyFactor.getVal());
        ScaleUV_L = ScaleUV_Loc_CK;
        OffsetUV_L = OffsetUV_Loc_CK;
        Matrix_L = Matrix_Loc_CK;
		Opacity_L = Opacity_Loc_CK;
    }
    else
    {
        sgct::ShaderManager::instance()->bindShaderProgram("flipxform");
        glUniform1i(flipFrame_Loc, 0);
    }

    glFrontFace(GL_CCW);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//stop capture if necassary
	bool captureStarted = planeCaptureRunning.getVal();
	if (captureStarted && captureLockStatus.getVal() == 1) {
		stopPlaneCapture();
	}

    glCullFace(GL_BACK);

	//No capture planes when taking screenshot
	if (!screenshotPassOn) {
		glUniform2f(ScaleUV_L, planeScaling.x, planeScaling.y);
		glUniform2f(OffsetUV_L, planeOffset.x, planeOffset.y);

		for (int i = 0; i < captureContentPlanes.size(); i++) {
			glActiveTexture(GL_TEXTURE0);
			if (planeAttributesLocal.getVal()[i].freeze) {
				glBindTexture(GL_TEXTURE_2D, planeTexOwnedIds[i]);
			}
			else if (planeAttributesGlobal.getVal()[i].planeStrId > 0) {
				glBindTexture(GL_TEXTURE_2D, texIds.getValAt(planeAttributesGlobal.getVal()[i].planeTexId));
			}
			else {
                glBindTexture(GL_TEXTURE_2D, planeCaptureTexId);
			}

			float planeOpacity = getContentPlaneOpacity(i);
			if (planeOpacity > 0.f) {
				glUniform1f(Opacity_L, planeOpacity);

				glm::mat4 capturePlaneTransform = glm::mat4(1.0f);
				capturePlaneTransform = glm::rotate(capturePlaneTransform, glm::radians(planeAttributesGlobal.getVal()[i].azimuth), glm::vec3(0.0f, -1.0f, 0.0f)); //azimuth
				capturePlaneTransform = glm::rotate(capturePlaneTransform, glm::radians(planeAttributesGlobal.getVal()[i].elevation), glm::vec3(1.0f, 0.0f, 0.0f)); //elevation
				capturePlaneTransform = glm::rotate(capturePlaneTransform, glm::radians(planeAttributesGlobal.getVal()[i].roll), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
				capturePlaneTransform = glm::translate(capturePlaneTransform, glm::vec3(0.0f, 0.0f, planeAttributesGlobal.getVal()[i].distance)); //distance

				capturePlaneTransform = MVP * capturePlaneTransform;
				glUniformMatrix4fv(Matrix_L, 1, GL_FALSE, &capturePlaneTransform[0][0]);

				captureContentPlanes[i]->draw();
			}
		}
	}

	float planeOpacity;
	for (int i = static_cast<int>(captureContentPlanes.size()); i < planeAttributesGlobal.getSize(); i++) {
		planeOpacity = getContentPlaneOpacity(i);
		if (planeOpacity > 0.f && masterContentPlanes.size() > i-captureContentPlanes.size()) {
			glActiveTexture(GL_TEXTURE0);
			if (planeAttributesGlobal.getVal()[i].planeStrId > 0) {
				glBindTexture(GL_TEXTURE_2D, texIds.getValAt(planeAttributesGlobal.getVal()[i].planeTexId));
			}
			else {
				glBindTexture(GL_TEXTURE_2D, planeCaptureTexId);
			}

			glUniform1f(Opacity_L, planeOpacity);

			glUniform2f(ScaleUV_L, 1.0f, 1.0f);
			glUniform2f(OffsetUV_L, 0.0f, 0.0f);

			//transform and draw plane
			glm::mat4 contentPlaneTransform = glm::mat4(1.0f);
			contentPlaneTransform = glm::rotate(contentPlaneTransform, glm::radians(planeAttributesGlobal.getVal()[i].azimuth), glm::vec3(0.0f, -1.0f, 0.0f)); //azimuth
			contentPlaneTransform = glm::rotate(contentPlaneTransform, glm::radians(planeAttributesGlobal.getVal()[i].elevation), glm::vec3(1.0f, 0.0f, 0.0f)); //elevation
			contentPlaneTransform = glm::rotate(contentPlaneTransform, glm::radians(planeAttributesGlobal.getVal()[i].roll), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
			contentPlaneTransform = glm::translate(contentPlaneTransform, glm::vec3(0.0f, 0.0f, planeAttributesGlobal.getVal()[i].distance)); //distance

			contentPlaneTransform = MVP * contentPlaneTransform;
			glUniformMatrix4fv(Matrix_L, 1, GL_FALSE, &contentPlaneTransform[0][0]);

			masterContentPlanes[i- captureContentPlanes.size()]->draw();
		}
	}

    if (fulldomeOpacity > 0.f) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, planeCaptureTexId);
        glm::vec2 texSize = glm::vec2(static_cast<float>(planceCaptureWidth),
                                        static_cast<float>(planeCaptureHeight));

        glUniform1f(Opacity_L, fulldomeOpacity);

        // TextureCut 2 equals showing only the middle square of a capturing a
        // widescreen input
        if (domeCut.getVal() == 2) {
            glUniform2f(ScaleUV_L, texSize.y / texSize.x, 1.f);
            glUniform2f(OffsetUV_L, ((texSize.x - texSize.y) * 0.5f) / texSize.x,
                        0.f);
        }
        else {
            glUniform2f(ScaleUV_L, 1.f, 1.f);
            glUniform2f(OffsetUV_L, 0.f, 0.f);
        }

        glCullFace(GL_FRONT);  // camera on the inside of the dome

        glUniformMatrix4fv(Matrix_L, 1, GL_FALSE, &MVP[0][0]);
        dome->draw();
    }

    sgct::ShaderManager::instance()->unBindShaderProgram();

	glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}

void myDraw2DFun()
{
    if (info.getVal())
    {
        unsigned int font_size = static_cast<unsigned int>(9.0f*gEngine->getCurrentWindowPtr()->getXScale());
        sgct_text::Font * font = sgct_text::FontManager::instance()->getFont("SGCTFont", font_size);
        float padding = 10.0f;

        /*sgct_text::print(font, sgct_text::TOP_LEFT,
            padding, static_cast<float>(gEngine->getCurrentWindowPtr()->getYFramebufferResolution() - font_size) - padding, //x and y pos
            glm::vec4(1.0, 1.0, 1.0, 1.0), //color
            "Format: %s\nResolution: %d x %d\nRate: %.2lf Hz",
            gPlaneCapture->getFormat(),
            planceCaptureWidth,
            planeCaptureHeight,
            captureRate.getVal());
			*/
    }

	bool drawGUI = true;
#ifdef OPENVR_SUPPORT
	if (FirstOpenVRWindow == gEngine->getCurrentWindowPtr())
		drawGUI = false;
#endif
	if (gEngine->isMaster() && !screenshotPassOn && drawGUI)
	{
		ImGui_ImplGlfwGL3_NewFrame(gEngine->getCurrentWindowPtr()->getXFramebufferResolution(), gEngine->getCurrentWindowPtr()->getYFramebufferResolution());

		std::string menu_action = "";
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Load Preset...")) menu_action = "Load";
				if (ImGui::MenuItem("Save Preset...")) menu_action = "SaveAs";

				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}

		if (menu_action == "Load") ImGui::OpenPopup("Load");
		if (menu_action == "SaveAs") ImGui::OpenPopup("SaveAs");

		if (ImGui::BeginPopup("Load"))
		{
			if (imPresets.empty()) {
				ImGui::Text("No presets saved yet...");
			}
			else {
				if (ImGui::Button("Reload list"))
				{
					
				}

				ImGui::Text("Load Preset:");
				ImGui::Combo("", &imPresetIdx, imPresets);

				if (ImGui::Button("Load"))
				{
                    std::string filename = "test";
                    std::string filepath = "presets/" + filename;
                    std::ifstream file(filepath, std::ios::out | std::ios::binary);

                    if (!file.eof() && !file.fail())
                    {
                        file.read((char*)sgct::SharedData::instance()->getDataBlock(), sgct::SharedData::instance()->getDataSize());
                        myDecodeFun();
                    }   

                    /*

                    file.write((const char*)sgct::SharedData::instance()->getDataBlock(), sgct::SharedData::instance()->getDataSize());
                    file.close();*/

					/*s.map.reset();
					s.map = std::make_unique<Map>(i_id);
					if (!s.map->Exists())
					{
						s.map.reset();
						menu_action = "ErrorLoad";
					}*/
					//else ImGui::CloseCurrentPopup();
				}

				if (menu_action == "ErrorLoad") ImGui::OpenPopup("ErrorLoad");

				if (ImGui::BeginPopupModal("ErrorLoad", NULL, ImGuiWindowFlags_NoResize))
				{
					ImGui::Text("Could not load map file of given ID.");
					if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();

					ImGui::EndPopup();
				}
			}
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("SaveAs"))
		{
			ImGui::Text("Save Preset As:");

			static char buf[64] = "...";
			ImGui::InputText("", buf, 64);

			if (ImGui::Button("Save"))
			{
                std::string filename = std::string(buf);
                std::string filepath = "presets/" + filename;
                std::ofstream file(filepath, std::ios::out | std::ios::binary);

                file.write((const char*)sgct::SharedData::instance()->getDataBlock(), sgct::SharedData::instance()->getDataSize());
                file.close();

				imPresets.push_back(buf);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Settings");
		if (currentDomeTexIdx >= 0) {
			ImGui::Combo("Current Fisheye Image", &currentDomeTexIdx, domeImageFileNames);
		}
		ImGui::SliderFloat("Fading Time", &imFadingTime, 0.f, 5.0f);
		if (ImGui::CollapsingHeader("Flat Content", 0, true, true))
		{
			if (!imPlaneUseCaptureSize) {
				ImGui::PushID("ScreenAspect");
				ImGui::RadioButton("16:10", &imPlaneScreenAspect, 1610); ImGui::SameLine();
				ImGui::RadioButton("16:9", &imPlaneScreenAspect, 169); ImGui::SameLine();
				ImGui::RadioButton("5:4", &imPlaneScreenAspect, 54); ImGui::SameLine();
				ImGui::RadioButton("4:3", &imPlaneScreenAspect, 43); ImGui::SameLine();
				ImGui::PopID();
				ImGui::Text("  -  Capture Screen Aspect Ratio");
				ImGui::PushID("MaterialAspect");
				ImGui::RadioButton("16:10", &imPlaneMaterialAspect, 1610); ImGui::SameLine();
				ImGui::RadioButton("16:9", &imPlaneMaterialAspect, 169); ImGui::SameLine();
				ImGui::RadioButton("5:4", &imPlaneMaterialAspect, 54); ImGui::SameLine();
				ImGui::RadioButton("4:3", &imPlaneMaterialAspect, 43); ImGui::SameLine();
				ImGui::PopID();
				ImGui::Text("  -  Capture Material Aspect Ratio");
			}
			ImGui::Checkbox("Use Capture Size For Aspect Ratio", &imPlaneUseCaptureSize);
			if (!imPlaneCapturePresMode) {
				if (ImGui::Button("Add New Plane")) {
					std::string name = "Content " + std::to_string((imPlanes.size() - 4));
					imPlanes.push_back(name);
					//ContentPlane p(name, 1.6f, 0.f, 85.f, 0.f, -5.5f);
					planeAttributesGlobal.addVal(ContentPlaneGlobalAttribs(name, 1.6f, 0.f, 85.f, 0.f, -5.5f));
					planeAttributesLocal.addVal(ContentPlaneLocalAttribs(name));
				}
			}
			ImGui::Combo("Currently Editing", &imPlaneIdx, imPlanes);
			if (!imPlaneCapturePresMode) {
				ImGui::Checkbox("Show Plane", &imPlaneShow);
				ImGui::Combo("Plane Source", &imPlaneImageIdx, planeImageFileNames);
			}
			ImGui::SliderFloat("Plane Height", &imPlaneHeight, 0.1f, 10.0f);
			ImGui::SliderFloat("Plane Azimuth", &imPlaneAzimuth, -180.f, 180.f);
			ImGui::SliderFloat("Plane Elevation", &imPlaneElevation, -180.f, 180.f);
			ImGui::SliderFloat("Plane Roll", &imPlaneRoll, -180.f, 180.f);
			ImGui::SliderFloat("Plane Distance", &imPlaneDistance, -10.f, 0.f);
			ImGui::Checkbox("Capture Presentation Mode (DomePres)", &imPlaneCapturePresMode);
			ImGui::Checkbox("Capture Fulldome Mode (Key1=All, Key2=MiddleSquare)", &fulldomeMode);
			if (ImGui::Button("Export Fisheye To Application Folder")) {
				takeScreenshot = true;
			}
			ImGui::Checkbox("Chroma Key On/Off", &imChromaKey);
			ImGui::ColorEdit3("Chroma Key Color", (float*)&imChromaKeyColor);
			ImGui::SliderFloat("Chroma Key Factor", &imChromaKeyFactor, 1.f, 100.0f);
		}
		ImGui::End();

		ImGui::Render();
	}
}

void myPostDrawFun()
{
#ifdef OPENVR_SUPPORT
	if (FirstOpenVRWindow) {
		//Copy the first OpenVR window to the HMD
		sgct::SGCTOpenVR::copyWindowToHMD(FirstOpenVRWindow);
	}
#endif
}

void startPlaneCapture()
{
	//start capture thread if host or load thread if master and not host
	sgct_core::SGCTNode * thisNode = sgct_core::ClusterManager::instance()->getThisNodePtr();
	if (thisNode->getAddress() == gPlaneCapture->getVideoHost()) {
		planeCaptureRunning.setVal(true);
		planeCaptureThread = new (std::nothrow) std::thread(planeCaptureLoop);
	}
}

void stopPlaneCapture()
{
	//kill capture thread
	planeCaptureRunning.setVal(false);
	if (planeCaptureThread)
	{
		planeCaptureThread->join();
		delete planeCaptureThread;
	}
}

#ifdef RGBEASY_ENABLED
void uploadRGBEasyCapturePlaneData(void* data, unsigned long width, unsigned long height) {
    if (!planeDPCaptureRunning.getVal())
        return;

    int dataSize = gPlaneDPCapture->getWidth() * gPlaneDPCapture->getHeight() * 3;

    if (!hiddenPlaneDPCaptureWindow) {
        glfwWindowHint(GLFW_VISIBLE, GL_FALSE);

        hiddenPlaneDPCaptureWindow = glfwCreateWindow(1, 1, "Thread RGBEasy Capture Window", NULL, sharedWindow);
        if (!hiddenPlaneDPCaptureWindow)
        {
            sgct::MessageHandler::instance()->print("Failed to create capture context!\n");
        }

        glfwMakeContextCurrent(hiddenPlaneDPCaptureWindow);

        if (!planeCaptureTexId)
        {
            planceCaptureWidth = width;
            planeCaptureHeight = height;
            planeCaptureTexId = allocateCaptureTexture();

            //update capture textures
            updateCapturePlaneTexIDs();

            planeReCreate.setVal(true);
        }

        glGenBuffers(1, &planeDPCapturePBO);

        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, planeDPCapturePBO);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, 0, GL_DYNAMIC_DRAW);
    }
    else
        glfwMakeContextCurrent(hiddenPlaneDPCaptureWindow);

    if (planeCaptureTexId)
    {
#ifdef ZXING_ENABLED
        uint8_t* dataUC = (uint8_t*)data;
        if (checkQRoperations(&dataUC, width, height, !flipFrame)) {
#endif

        void* GPU_ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
        if (GPU_ptr)
        {

            if (flipFrame)
            {
                int dataOffset = 0;
                int stride = width * 3; //Assuming BGR24
                unsigned char* GPU_ptrUC = static_cast<unsigned char*>(GPU_ptr);
                unsigned char* dataUC = static_cast<unsigned char*>(data);
                for (int row = height - 1; row > -1; row--)
                {
                    memcpy(GPU_ptrUC + dataOffset, dataUC + row * stride, stride);
                    dataOffset += stride;
                }
            }
            else
            {
            
                memcpy(GPU_ptr, data, dataSize);
            }

            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, planeCaptureTexId);

            //Assuming BGR24
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, 0);
        }

#ifdef ZXING_ENABLED
        }
#endif
    }

    glfwMakeContextCurrent(NULL); //detach context
}

void startPlaneDPCapture()
{
	//start capture thread if host or load thread if master and not host
	sgct_core::SGCTNode * thisNode = sgct_core::ClusterManager::instance()->getThisNodePtr();
	if (thisNode->getAddress() == gPlaneDPCapture->getCaptureHost()) {
		planeDPCaptureRunning.setVal(true);
		planeDPCaptureThread = new (std::nothrow) std::thread(planeDPCaptureLoop);
	}
}

void stopPlaneDPCapture()
{
	//kill capture thread
	planeDPCaptureRunning.setVal(false);
	if (planeDPCaptureThread)
	{
		planeDPCaptureThread->join();
		delete planeDPCaptureThread;
	}
}

void planeDPCaptureLoop() {
	gPlaneDPCapture->runCapture();
	while (planeDPCaptureRunning.getVal()) {
		// Frame capture running...
        sgct::Engine::sleep(0.02);
    }

    glfwMakeContextCurrent(hiddenPlaneDPCaptureWindow);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    glDeleteBuffers(1, &planeDPCapturePBO);

    glfwMakeContextCurrent(NULL); //detach context
}

void startFisheyeCapture()
{
	//start capture thread if host or load thread if master and not host
	sgct_core::SGCTNode * thisNode = sgct_core::ClusterManager::instance()->getThisNodePtr();	
	if (thisNode->getAddress() == gFisheyeCapture->getCaptureHost()) {
		fisheyeCaptureRunning.setVal(true);
		fisheyeCaptureThread = new (std::nothrow) std::thread(fisheyeCaptureLoop);
	}
}

void stopFisheyeCapture()
{
	//kill capture thread
	fisheyeCaptureRunning.setVal(false);
	if (fisheyeCaptureThread)
	{
		fisheyeCaptureThread->join();
		delete fisheyeCaptureThread;
	}
}

void fisheyeCaptureLoop() {
	gFisheyeCapture->runCapture();
	while (fisheyeCaptureRunning.getVal()) {
		// Frame capture running...
	}
}

void RGBEasyCaptureGPUPollAndDraw(RGBEasyCaptureGPU* capture, RT& captureRT) {
	if (capture->prepareForRendering()) {
		//Rendering to square texture when assumig ganing (i.e. from 2x1 to 1x2) as 1x2 does not seem to function properly
		if (capture->getGanging()) {
			sgct::ShaderManager::instance()->bindShaderProgram("sbs2tb");

			//transform
			glm::mat4 planeTransform = glm::mat4(1.0f);
			glUniformMatrix4fv(Matrix_Loc, 1, GL_FALSE, &planeTransform[0][0]);
		}
		else {
			sgct::ShaderManager::instance()->bindShaderProgram("flipxform");

			//transform
			glm::mat4 planeTransform = glm::mat4(1.0f);
			glUniformMatrix4fv(Matrix_Loc, 1, GL_FALSE, &planeTransform[0][0]);
            glUniform1i(flipFrame_Loc, flipFrame);
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
		capture->renderingCompleted();

		//restore
		if (fbo)
			fbo->bind();
		sgct::ShaderManager::instance()->bindShaderProgram("flipxform");
		const int * coords = gEngine->getCurrentViewportPixelCoords();
		glViewport(coords[0], coords[1], coords[2], coords[3]);
	}
}

void RGBEasyRenderToTextureSetup(RGBEasyCaptureGPU* capture, RT& captureRT) {
	// check if we are ganing inputs
	// thus we assume 2x1 (sbs) which we want to change to 1x2 (tb)
	if (capture->getGanging()) {
		captureRT.width = capture->getWidth() / 2;
		captureRT.height = capture->getHeight() * 2;
	}
	else {
		captureRT.width = capture->getWidth();
		captureRT.height = capture->getHeight();
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

#ifdef ZXING_ENABLED
bool checkQRoperations(uint8_t** data, int width, int height, bool flipped)
{
    // If result is not empty, we have to interpret the message to decide it the plane should lock the capture to the previous frame or update it.
    std::vector<std::string> decodedResults;
    if (planeCapturePresMode.getVal())
        decodedResults = QRCodeInterpreter::decodeImageMulti(BGR24LuminanceSource::create(data, width, height, flipped));

    if (!decodedResults.empty()) {
        //Save only unique operations
        for each(std::string decodedResult in decodedResults) {
            if (std::find(operationsQueue.begin(), operationsQueue.end(), decodedResult) == operationsQueue.end()) {
                //Operation not in queue, add it
                sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Decode %i characters, with resulting string: %s\n", decodedResult.size(), decodedResult.c_str());
                operationsQueue.push_back(decodedResult.c_str());
            }
        }
        return false;
    }
    else {
        std::vector<ContentPlaneLocalAttribs> pAL = planeAttributesLocal.getVal();
        std::vector<ContentPlaneGlobalAttribs> pAG = planeAttributesGlobal.getVal();
        if (!operationsQueue.empty()) {
            // Now we can process them when decodedResults is empty
            for (size_t i = 0; i < operationsQueue.size(); i++) {
                sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Applying Operation: %s\n", operationsQueue[i].c_str());
                std::vector<std::string> operation = split(operationsQueue[i].c_str(), ';');
                if (operation.size() > 1) {
                    int capturePlaneIdx = -1;
                    for (int p = 0; p < captureContentPlanes.size(); p++) {
                        if (pAL[p].name == operation[0]) {
                            capturePlaneIdx = p;
                            break;
                        }
                    }
                    if (capturePlaneIdx >= 0) {
                        for (int o = 1; o < operation.size(); o++) {
                            if (operation[o] == "SetActive") {
                                // Setting capturePlaneIdx as active capture plane
                                pAL[capturePlaneIdx].previouslyVisible = false;
                                pAL[capturePlaneIdx].freeze = false;
                                pAG[capturePlaneIdx].planeTexId = 0;

                                //Freezing other planes which are not already frozen
                                for (int p = 0; p < captureContentPlanes.size(); p++) {
                                    if (p != capturePlaneIdx && !pAL[p].freeze) {
                                        pAL[p].freeze = true;
                                        glCopyImageSubData(planeCaptureTexId, GL_TEXTURE_2D, 0, 0, 0, 0, planeTexOwnedIds[p], GL_TEXTURE_2D, 0, 0, 0, 0, width, height, 1);
                                        glFlush();
                                    }
                                }

                                // Setting capturePlaneIdx as active capture plane
                                pAL[capturePlaneIdx].currentlyVisible = true;
                            }
                        }
                    }
                    else if (operation[0] == "AllCaptures") {
                        if (operation[1] == "Clear") {
                            //Making all planes fade out
                            for (int p = 0; p < captureContentPlanes.size(); p++) {
                                //Need to freeze all planes
                                if (!pAL[p].freeze) {
                                    pAL[p].freeze = true;
                                    glCopyImageSubData(planeCaptureTexId, GL_TEXTURE_2D, 0, 0, 0, 0, planeTexOwnedIds[p], GL_TEXTURE_2D, 0, 0, 0, 0, width, height, 1);
                                    glFlush();
                                }
                                pAL[p].currentlyVisible = false;
                            }
                        }
                    }
                    else {
                        sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Could not find plane named: %s\n", operation[0].c_str());
                    }
                }
            }
            planeAttributesLocal.setVal(pAL);
            operationsQueue.clear();
        }
        return true;
    }
}
#endif

void updateCapturePlaneTexIDs() {
    size_t planesTexCount = planeTexOwnedIds.size();
    planeTexOwnedIds.clear();

    for (size_t i = 0; i < planesTexCount; i++)
        planeTexOwnedIds.push_back(allocateCaptureTexture());
}

void allocateCapturePlanes() {
    //define capture planes
    imPlanes.push_back("FrontCapture");
    ContentPlane frontCapture = ContentPlane("FrontCapture", imPlaneHeight, imPlaneAzimuth, imPlaneElevation, imPlaneRoll, imPlaneDistance, true);
    planeTexOwnedIds.push_back(allocateCaptureTexture());
    planeAttributesGlobal.addVal(frontCapture.getGlobal());
    planeAttributesLocal.addVal(frontCapture.getLocal());
    captureContentPlanes.push_back(nullptr);

    imPlanes.push_back("BackCapture");
    ContentPlane backCapture = ContentPlane("BackCapture", 1.8f, -155.f, 20.f, imPlaneRoll, imPlaneDistance, false);
    planeTexOwnedIds.push_back(allocateCaptureTexture());
    planeAttributesGlobal.addVal(backCapture.getGlobal());
    planeAttributesLocal.addVal(backCapture.getLocal());
    captureContentPlanes.push_back(nullptr);

    imPlanes.push_back("LeftCapture");
    ContentPlane leftCapture = ContentPlane("LeftCapture", 2.865f, -75.135f, 26.486f, imPlaneRoll, imPlaneDistance, false);
    planeTexOwnedIds.push_back(allocateCaptureTexture());
    planeAttributesGlobal.addVal(leftCapture.getGlobal());
    planeAttributesLocal.addVal(leftCapture.getLocal());
    captureContentPlanes.push_back(nullptr);

    imPlanes.push_back("RightCapture");
    ContentPlane rightCapture = ContentPlane("RightCapture", 2.865f, 75.135f, 26.486f, imPlaneRoll, imPlaneDistance, false);
    planeTexOwnedIds.push_back(allocateCaptureTexture());
    planeAttributesGlobal.addVal(rightCapture.getGlobal());
    planeAttributesLocal.addVal(rightCapture.getLocal());
    captureContentPlanes.push_back(nullptr);

    imPlanes.push_back("TopCapture");
    ContentPlane topCapture = ContentPlane("TopCapture", imPlaneHeight, 0.f, 75.135f, imPlaneRoll, imPlaneDistance, false);
    planeTexOwnedIds.push_back(allocateCaptureTexture());
    planeAttributesGlobal.addVal(topCapture.getGlobal());
    planeAttributesLocal.addVal(topCapture.getLocal());
    captureContentPlanes.push_back(nullptr);

    //define default content plane
    //imPlanes.push_back("Content 1");
    //planeAttributes.addVal(ContentPlane(1.6f, 0.f, 95.0f, 0.f));
}

void createPlanes() {
	//Capture planes
	int capturePlaneSize = static_cast<int>(captureContentPlanes.size());
	for (int i = 0; i < capturePlaneSize; i++) {
		delete captureContentPlanes[i];
	}
	captureContentPlanes.clear();

	float captureRatio = (static_cast<float>(planceCaptureWidth) / static_cast<float>(planeCaptureHeight));

	for (int i = 0; i < capturePlaneSize; i++) {
		float planeWidth = planeAttributesGlobal.getVal()[i].height * captureRatio;

		if (planeUseCaptureSize.getVal())
		{
			captureContentPlanes.push_back(new sgct_utils::SGCTPlane(planeWidth, planeAttributesGlobal.getVal()[i].height));
		}
		else
		{
			switch (planeMaterialAspect.getVal())
			{
			case 1610:
				captureContentPlanes.push_back(new sgct_utils::SGCTPlane((planeAttributesGlobal.getVal()[i].height / 10.0f) * 16.0f, planeAttributesGlobal.getVal()[i].height));
				break;
			case 169:
				captureContentPlanes.push_back(new sgct_utils::SGCTPlane((planeAttributesGlobal.getVal()[i].height / 9.0f) * 16.0f, planeAttributesGlobal.getVal()[i].height));
				break;
			case 54:
				captureContentPlanes.push_back(new sgct_utils::SGCTPlane((planeAttributesGlobal.getVal()[i].height / 4.0f) * 5.0f, planeAttributesGlobal.getVal()[i].height));
				break;
			case 43:
				captureContentPlanes.push_back(new sgct_utils::SGCTPlane((planeAttributesGlobal.getVal()[i].height / 3.0f) * 4.0f, planeAttributesGlobal.getVal()[i].height));
				break;
			default:
				captureContentPlanes.push_back(new sgct_utils::SGCTPlane(planeWidth, planeAttributesGlobal.getVal()[i].height));
				break;
			}
		}
	}

	if (planeUseCaptureSize.getVal())
	{
		planeScaling = glm::vec2(1.0f, 1.0f);
		planeOffset = glm::vec2(0.0f, 0.0f);
	}
	else
	{
		switch (planeScreenAspect.getVal())
		{
		case 1610:
			switch (planeMaterialAspect.getVal())
			{
			case 169:
				//16:10 -> 16:9
				planeScaling = glm::vec2(1.0f, 0.90f);
				planeOffset = glm::vec2(0.01f, 0.05f);
				break;
			case 54:
				//16:10 -> 5:4
				planeScaling = glm::vec2(10.0f / 12.0f, 1.0f);
				planeOffset = glm::vec2(1.0f / 12.0f, 0.0f);
				break;
			case 43:
				//16:10 -> 4:3
				planeScaling = glm::vec2(50.0f / 64.0f, 1.0f);
				planeOffset = glm::vec2(7.0f / 64.0f, 0.0f);
				break;
			case 1610:
			default:
				planeScaling = glm::vec2(1.0f, 1.0f);
				planeOffset = glm::vec2(0.01f, 0.0f);
				break;
			}
			break;
		case 169:
			switch (planeMaterialAspect.getVal())
			{
			case 1610:
				//16:9 -> 16:10
				planeScaling = glm::vec2(0.90f, 1.0f);
				planeOffset = glm::vec2(0.05f, 0.0f);
				break;
			case 54:
				//16:9 -> 5:4
				planeScaling = glm::vec2(1.0f, 270.0f / 384.0f);
				planeOffset = glm::vec2(0.01f, 57.0f / 384.0f);
				break;
			case 43:
				//16:9 -> 4:3
				planeScaling = glm::vec2(1.0f, 0.75f);
				planeOffset = glm::vec2(0.01f, 0.125f);
				break;
			case 169:
			default:
				planeScaling = glm::vec2(1.0f, 1.0f);
				planeOffset = glm::vec2(0.01f, 0.0f);
				break;
			}
			break;
		case 54:
			switch (planeMaterialAspect.getVal())
			{
			case 1610:
				//5:4 -> 16:10
				planeScaling = glm::vec2(10.0f / 12.0f, 1.0f);
				planeOffset = glm::vec2(1.0f / 12.0f, 0.0f);
				break;
			case 169:
				//5:4 -> 16:9
				planeScaling = glm::vec2(270.0f / 384.0f, 1.0f);
				planeOffset = glm::vec2(57.0f / 384.0f, 0.0f);
				break;
			case 43:
				//5:4 -> 4:3
				planeScaling = glm::vec2(1.0f, 0.9375f);
				planeOffset = glm::vec2(0.01f, 0.03125f);
				break;
			case 54:
			default:
				planeScaling = glm::vec2(1.0f, 1.0f);
				planeOffset = glm::vec2(0.01f, 0.0f);
				break;
			}
			break;
		case 43:
			switch (planeMaterialAspect.getVal())
			{
			case 1610:
				//4:3 -> 16:10
				planeScaling = glm::vec2(1.0f, 50.0f / 64.0f);
				planeOffset = glm::vec2(0.01f, 7.0f / 64.0f);
				break;
			case 169:
				//4:3 -> 16:9
				planeScaling = glm::vec2(1.0f, 0.75f);
				planeOffset = glm::vec2(0.01f, 0.125f);
				break;
			case 54:
				//4:3 -> 5:4
				planeScaling = glm::vec2(0.9375f, 1.0f);
				planeOffset = glm::vec2(0.03125f, 0.0f);
				break;
			case 43:
			default:
				planeScaling = glm::vec2(1.0f, 1.0f);
				planeOffset = glm::vec2(0.01f, 0.0f);
				break;
			}
			break;
		default:
			planeScaling = glm::vec2(1.0f, 1.0f);
			planeOffset = glm::vec2(0.01f, 0.0f);
			break;
		}
	}

	//Content planes
	for (int i = 0; i < masterContentPlanes.size(); i++) {
		delete masterContentPlanes[i];
	}
	masterContentPlanes.clear();

	for (size_t i = captureContentPlanes.size(); i < planeAttributesGlobal.getSize(); i++) {
		float width = planeAttributesGlobal.getVal()[i].height * texAspectRatio.getValAt(planeAttributesGlobal.getVal()[i].planeTexId);
		masterContentPlanes.push_back(new sgct_utils::SGCTPlane(width, planeAttributesGlobal.getVal()[i].height));
	}

	//Reset re-creation
	planeReCreate.setVal(false);
}

void myInitOGLFun()
{
#ifdef OPENVR_SUPPORT
	//Find if we have at least one OpenVR window
	//Save reference to first OpenVR window, which is the one we will copy to the HMD.
	for (size_t i = 0; i < gEngine->getNumberOfWindows(); i++) {
		if (gEngine->getWindowPtr(i)->checkIfTagExists("OpenVR")) {
			FirstOpenVRWindow = gEngine->getWindowPtr(i);
			break;
		}
	}
	//If we have an OpenVRWindow, initialize OpenVR.
	if (FirstOpenVRWindow) {
		sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "OpenVR Initalized!\n");
		sgct::SGCTOpenVR::initialize(gEngine->getNearClippingPlane(), gEngine->getFarClippingPlane());
	}
#endif

    fullDomeAttribs.currentlyVisible = false;
    fullDomeAttribs.previouslyVisible = false;

	// do directshow if we don't use the better RGBEasy solution
	if (!planeDPCaptureRequested) {
		bool captureReady = gPlaneCapture->init();
		planceCaptureWidth = gPlaneCapture->getWidth();
		planeCaptureHeight = gPlaneCapture->getHeight();

		//allocate texture
		if (captureReady) {
			planeCaptureTexId = allocateCaptureTexture();
		}
		planeImageFileNames.push_back("Single Capture");

		if (captureReady) {
			//start capture
			startPlaneCapture();
		}

		std::function<void(uint8_t ** data, int width, int height)> callback = uploadCaptureData;
		gPlaneCapture->setVideoDecoderCallback(callback);
	}

#ifdef RGBEASY_ENABLED
	// do a high-frame rate fisheye capturing (if requested)
	if (planeDPCaptureRequested) {
		sgct_core::SGCTNode * thisNode = sgct_core::ClusterManager::instance()->getThisNodePtr();
		if (thisNode->getAddress() == gPlaneDPCapture->getCaptureHost()) {
			if (gPlaneDPCapture->initialize()) {
                std::function<void(void* data, unsigned long width, unsigned long height)> callback = uploadRGBEasyCapturePlaneData;
                gPlaneDPCapture->setCaptureCallback(callback);
				startPlaneDPCapture();

                // TODO : Don't know dimensions in this stage...
                planceCaptureWidth = gPlaneDPCapture->getWidth();
                planeCaptureHeight = gPlaneDPCapture->getHeight();
			}
		}

		planeImageFileNames.push_back("Single DP Capture");
	}

	// do a high-frame rate fisheye capturing (if requested)
	if (fisheyeCaptureRequested) {
		sgct_core::SGCTNode * thisNode = sgct_core::ClusterManager::instance()->getThisNodePtr();
		fisheyeCaptureRT.texture = GL_FALSE;
		if (thisNode->getAddress() == gFisheyeCapture->getCaptureHost()) {
			if (gFisheyeCapture->initialize()) {

				// Perform Capture OpenGL operations on MAIN thread
				// initalize capture OpenGL (running on this thread)
				gFisheyeCapture->initializeGL();
				RGBEasyRenderToTextureSetup(gFisheyeCapture, fisheyeCaptureRT);

				// Perform frame capture on BACKGROUND thread
				// start capture thread (which will intialize RGBEasy things)
				startFisheyeCapture();
			}
		}
	}
    domeImageFileNames.push_back("Fisheye Capture");
    texIds.addVal(fisheyeCaptureRT.texture);
    texAspectRatio.addVal(1.f);
    imagePathsVec.push_back(std::pair<std::string, int>("", 0));
    imagePathsMap.insert(std::pair<std::string, int>(domeImageFileNames[0], 0));
    lastPackage.setVal(0);
    domeTexIndex.setVal(0);
    currentDomeTexIdx = domeTexIndex.getVal();
    numSyncedTex = 1;
#endif

	//start load thread
    if (gEngine->isMaster())
        loadThread = new (std::nothrow) std::thread(threadWorker);

    //define capture planes
    allocateCapturePlanes();

    //create plane
	createPlanes();

	//create RT square
	RTsquare = new sgct_utils::SGCTPlane(2.0f, 2.0f);

    //create dome
    dome = new sgct_utils::SGCTDome(7.4f, 165.f, 256, 128);

    sgct::ShaderManager::instance()->addShaderProgram( "flipxform",
            "flip.vert",
            "xform.frag" );

    sgct::ShaderManager::instance()->bindShaderProgram( "flipxform" );

    Matrix_Loc = sgct::ShaderManager::instance()->getShaderProgram( "flipxform").getUniformLocation( "MVP" );
    ScaleUV_Loc = sgct::ShaderManager::instance()->getShaderProgram( "flipxform").getUniformLocation("scaleUV");
    OffsetUV_Loc = sgct::ShaderManager::instance()->getShaderProgram( "flipxform").getUniformLocation("offsetUV");
    flipFrame_Loc = sgct::ShaderManager::instance()->getShaderProgram("flipxform").getUniformLocation("flipFrame");
	Opacity_Loc = sgct::ShaderManager::instance()->getShaderProgram("flipxform").getUniformLocation("opacity");
    GLint Tex_Loc = sgct::ShaderManager::instance()->getShaderProgram( "flipxform").getUniformLocation( "Tex" );
    glUniform1i( Tex_Loc, 0 );

	sgct::ShaderManager::instance()->unBindShaderProgram();

	sgct::ShaderManager::instance()->addShaderProgram("textureblend",
		"xform.vert",
		"textureblend.frag");

	sgct::ShaderManager::instance()->bindShaderProgram("textureblend");

	Matrix_Loc_BLEND = sgct::ShaderManager::instance()->getShaderProgram("textureblend").getUniformLocation("MVP");
	ScaleUV_Loc_BLEND = sgct::ShaderManager::instance()->getShaderProgram("textureblend").getUniformLocation("scaleUV");
	OffsetUV_Loc_BLEND = sgct::ShaderManager::instance()->getShaderProgram("textureblend").getUniformLocation("offsetUV");
	GLint Tex_Blend_Loc0 = sgct::ShaderManager::instance()->getShaderProgram("textureblend").getUniformLocation("Tex0");
	glUniform1i(Tex_Blend_Loc0, 0);
	GLint Tex_Blend_Loc1 = sgct::ShaderManager::instance()->getShaderProgram("textureblend").getUniformLocation("Tex1");
	glUniform1i(Tex_Blend_Loc1, 1);
	TexMix_Loc_BLEND = sgct::ShaderManager::instance()->getShaderProgram("textureblend").getUniformLocation("texMix");

    sgct::ShaderManager::instance()->unBindShaderProgram();

    sgct::ShaderManager::instance()->addShaderProgram("chromakey",
        "xform.vert",
        "chromakey.frag");

    sgct::ShaderManager::instance()->bindShaderProgram("chromakey");

    Matrix_Loc_CK = sgct::ShaderManager::instance()->getShaderProgram("chromakey").getUniformLocation("MVP");
    ScaleUV_Loc_CK = sgct::ShaderManager::instance()->getShaderProgram("chromakey").getUniformLocation("scaleUV");
    OffsetUV_Loc_CK = sgct::ShaderManager::instance()->getShaderProgram("chromakey").getUniformLocation("offsetUV");
	Opacity_Loc_CK = sgct::ShaderManager::instance()->getShaderProgram("chromakey").getUniformLocation("opacity");
    ChromaKeyColor_Loc_CK = sgct::ShaderManager::instance()->getShaderProgram("chromakey").getUniformLocation("chromaKeyColor");
	ChromaKeyFactor_Loc_CK = sgct::ShaderManager::instance()->getShaderProgram("chromakey").getUniformLocation("chromaKeyFactor");
    GLint Tex_Loc_CK = sgct::ShaderManager::instance()->getShaderProgram("chromakey").getUniformLocation("Tex");
    glUniform1i(Tex_Loc_CK, 0);

    sgct::ShaderManager::instance()->unBindShaderProgram();

	sgct::ShaderManager::instance()->addShaderProgram("sbs2tb",
		"xform.vert",
		"sbs2tb.frag");

	sgct::ShaderManager::instance()->bindShaderProgram("sbs2tb");

	Matrix_Loc_RT = sgct::ShaderManager::instance()->getShaderProgram("sbs2tb").getUniformLocation("MVP");
	GLint Tex_Loc_RT = sgct::ShaderManager::instance()->getShaderProgram("sbs2tb").getUniformLocation("Tex");
	glUniform1i(Tex_Loc_RT, 0);

	sgct::ShaderManager::instance()->unBindShaderProgram();

	// Setup ImGui binding
	if (gEngine->isMaster()) {
		ImGui_ImplGlfwGL3_Init(gEngine->getCurrentWindowPtr()->getWindowHandle());

		ImGuiStyle& style = ImGui::GetStyle();
		style.IndentSpacing = 25;
		style.ItemSpacing = { 4.f, 2.f };
		style.Colors[ImGuiCol_Border] = ImVec4(0.1f, 0.39f, 0.42f, 0.59f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.5f, 0.94f, 1.0f, 0.45f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.5f, 0.94f, 1.0f, 0.45f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.5f, 0.94f, 1.0f, 0.45f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.5f, 0.94f, 1.0f, 0.45f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.12f, 0.71f, 0.8f, 0.43f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.4f, 0.75f, 0.8f, 0.65f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.4f, 0.75f, 0.8f, 0.65f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.5f, 0.8f, 0.76f, 1.0f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.0f, 0.36f, 0.67f, 0.6f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.51f, 0.94f, 1.0f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.0f, 0.43f, 0.8f, 1.0f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.5f, 0.94f, 1.0f, 0.45f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.5f, 0.94f, 1.0f, 0.45f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.5f, 0.94f, 1.0f, 0.45f);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		style.Colors[ImGuiCol_CloseButton] = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
		style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.52f, 0.52f, 0.52f, 0.6f);
		style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.52f, 0.52f, 0.52f, 1.0f);
	}

    sgct::Engine::checkForOGLErrors();
}

void myEncodeFun()
{
    sgct::SharedData::instance()->writeDouble(&curr_time);
    sgct::SharedData::instance()->writeBool(&info);
    sgct::SharedData::instance()->writeBool(&stats);

	if (numSyncedTex > 0) {
		domeTexIndex.setVal(imagePathsMap[domeImageFileNames[currentDomeTexIdx]]);
	}
	sgct::SharedData::instance()->writeInt32(&domeTexIndex);

	renderDome.setVal(fulldomeMode);
    fullDomeAttribs.currentlyVisible = fulldomeMode;
    sgct::SharedData::instance()->writeBool(&renderDome);
    sgct::SharedData::instance()->writeInt32(&domeCut);
	fadingTime.setVal(imFadingTime);
	sgct::SharedData::instance()->writeFloat(&fadingTime);

	if (planeScreenAspect.getVal() != imPlaneScreenAspect) planeReCreate.setVal(true);
	planeScreenAspect.setVal(imPlaneScreenAspect);
	sgct::SharedData::instance()->writeInt32(&planeScreenAspect);
	if (planeMaterialAspect.getVal() != imPlaneMaterialAspect) planeReCreate.setVal(true);
	planeMaterialAspect.setVal(imPlaneMaterialAspect);
	sgct::SharedData::instance()->writeInt32(&planeMaterialAspect);
	planeCapturePresMode.setVal(imPlaneCapturePresMode);
	sgct::SharedData::instance()->writeBool(&planeCapturePresMode);
	if (planeUseCaptureSize.getVal() != imPlaneUseCaptureSize) planeReCreate.setVal(true);
	planeUseCaptureSize.setVal(imPlaneUseCaptureSize);
	sgct::SharedData::instance()->writeBool(&planeUseCaptureSize);

	std::vector<ContentPlaneGlobalAttribs> pAG = planeAttributesGlobal.getVal();
	std::vector<ContentPlaneLocalAttribs> pAL = planeAttributesLocal.getVal();
	if (imPlaneIdx != imPlanePreviousIdx) {
		imPlaneHeight = pAG[imPlaneIdx].height;
		imPlaneAzimuth = pAG[imPlaneIdx].azimuth;
		imPlaneElevation = pAG[imPlaneIdx].elevation;
		imPlaneRoll = pAG[imPlaneIdx].roll;
		imPlaneDistance = pAG[imPlaneIdx].distance;
		imPlaneShow = pAL[imPlaneIdx].currentlyVisible;
		imPlanePreviousIdx = imPlaneIdx;
		imPlaneImageIdx = pAG[imPlaneIdx].planeStrId;
	}

	if (planeAttributesGlobal.getVal()[imPlaneIdx].height != imPlaneHeight) planeReCreate.setVal(true);
	if (planeAttributesGlobal.getVal()[imPlaneIdx].planeStrId != imPlaneImageIdx) planeReCreate.setVal(true);
	pAG[imPlaneIdx] = ContentPlaneGlobalAttribs(pAG[imPlaneIdx].name, imPlaneHeight, imPlaneAzimuth, imPlaneElevation, imPlaneRoll, imPlaneDistance, imPlaneImageIdx, imagePathsMap[planeImageFileNames[imPlaneImageIdx]]);
	pAL[imPlaneIdx] = ContentPlaneLocalAttribs(pAG[imPlaneIdx].name, imPlaneShow, pAL[imPlaneIdx].previouslyVisible, pAL[imPlaneIdx].fadeStartTime, pAL[imPlaneIdx].freeze);
	planeAttributesGlobal.setVal(pAG);
	sgct::SharedData::instance()->writeVector<ContentPlaneGlobalAttribs>(&planeAttributesGlobal);
	if (!planeCapturePresMode.getVal()) {
		planeAttributesLocal.setVal(pAL); //Local is same as global on master, but maybe not on slaves.
		sgct::SharedData::instance()->writeVector<ContentPlaneLocalAttribs>(&planeAttributesLocal);
	}
	sgct::SharedData::instance()->writeBool(&planeReCreate);

	chromaKey.setVal(imChromaKey);
	sgct::SharedData::instance()->writeBool(&chromaKey);
	chromaKeyColor.setVal(glm::vec3(imChromaKeyColor.x, imChromaKeyColor.y, imChromaKeyColor.z));
	sgct::SharedData::instance()->writeObj(&chromaKeyColor);
	chromaKeyFactor.setVal(imChromaKeyFactor);
	sgct::SharedData::instance()->writeFloat(&chromaKeyFactor);
}

void myDecodeFun()
{
    sgct::SharedData::instance()->readDouble(&curr_time);
    sgct::SharedData::instance()->readBool(&info);
    sgct::SharedData::instance()->readBool(&stats);
    sgct::SharedData::instance()->readInt32(&domeTexIndex);
    sgct::SharedData::instance()->readBool(&renderDome);
	fulldomeMode = renderDome.getVal();
    fullDomeAttribs.currentlyVisible = fulldomeMode;
    sgct::SharedData::instance()->readInt32(&domeCut);
	sgct::SharedData::instance()->readFloat(&fadingTime);

	sgct::SharedData::instance()->readInt32(&planeScreenAspect);
	sgct::SharedData::instance()->readInt32(&planeMaterialAspect);
	sgct::SharedData::instance()->readBool(&planeCapturePresMode);
	sgct::SharedData::instance()->readBool(&planeUseCaptureSize);
	sgct::SharedData::instance()->readVector<ContentPlaneGlobalAttribs>(&planeAttributesGlobal);
	if (!planeCapturePresMode.getVal()) {
		// If we don't run in CapturePresMode, read local parameters from master as well
		sgct::SharedData::instance()->readVector<ContentPlaneLocalAttribs>(&planeAttributesLocal);
	}
	sgct::SharedData::instance()->readBool(&planeReCreate);

	sgct::SharedData::instance()->readBool(&chromaKey);
	sgct::SharedData::instance()->readObj(&chromaKeyColor);
	sgct::SharedData::instance()->readFloat(&chromaKeyFactor);
}

void myCleanUpFun()
{
    if (dome != NULL)
        delete dome;

	if (RTsquare)
		delete RTsquare;

	//Capture planes
	for (int i = 0; i < captureContentPlanes.size(); i++) {
		delete captureContentPlanes[i];
	}
	captureContentPlanes.clear();

	//Content planes
	for (int i = 0; i < masterContentPlanes.size(); i++) {
		delete masterContentPlanes[i];
	}
	masterContentPlanes.clear();

    if (planeCaptureTexId)
    {
        glDeleteTextures(1, &planeCaptureTexId);
		planeCaptureTexId = GL_FALSE;
    }
    
    for(std::size_t i=0; i < texIds.getSize(); i++)
    {
        GLuint tex = texIds.getValAt(i);
        if (tex)
        {
            glDeleteTextures(1, &tex);
            texIds.setValAt(i, GL_FALSE);
        }
    }
    texIds.clear();
    
    
    if(hiddenPlaneCaptureWindow)
        glfwDestroyWindow(hiddenPlaneCaptureWindow);

    if (hiddenPlaneDPCaptureWindow)
        glfwDestroyWindow(hiddenPlaneDPCaptureWindow);

	if (hiddenTransferWindow)
		glfwDestroyWindow(hiddenTransferWindow);
}

void myKeyCallback(int key, int action)
{
    if (gEngine->isMaster())
    {
		switch (key)
		{
		case SGCT_KEY_D:
			if (action == SGCT_PRESS)
				fulldomeMode = true;
			break;
		case SGCT_KEY_S:
			if (action == SGCT_PRESS)
				stats.toggle();
			break;

		case SGCT_KEY_I:
			if (action == SGCT_PRESS)
				info.toggle();
			break;

		case SGCT_KEY_1:
			if (action == SGCT_PRESS)
				domeCut.setVal(1);
			break;

		case SGCT_KEY_2:
			if (action == SGCT_PRESS)
				domeCut.setVal(2);
			break;

			//plane mode
		case SGCT_KEY_P:
			if (action == SGCT_PRESS)
				fulldomeMode = false;
			break;

		case SGCT_KEY_SPACE:
			if (action == SGCT_PRESS)
				imPlaneShow = !imPlaneShow;
			break;

		/*case SGCT_KEY_LEFT:
			if (action == SGCT_PRESS && numSyncedTex.getVal() > 0)
			{
				domeTexIndex.getVal() > incrIndex.getVal() - 1 ? domeTexIndex -= incrIndex.getVal() : domeTexIndex.setVal(numSyncedTex.getVal() - 1);
				//fprintf(stderr, "Index set to: %d\n", domeTexIndex.getVal());
			}
			break;

		case SGCT_KEY_RIGHT:
			if (action == SGCT_PRESS && numSyncedTex.getVal() > 0)
			{
				domeTexIndex.setVal((domeTexIndex.getVal() + incrIndex.getVal()) % numSyncedTex.getVal());
				//fprintf(stderr, "Index set to: %d\n", domeTexIndex.getVal());
			}
			break;*/
		}

		ImGui_ImplGlfwGL3_KeyCallback(gEngine->getCurrentWindowPtr()->getWindowHandle(), key, 0, action, 0);
    }
}

void myCharCallback(unsigned int c)
{
	if (gEngine->isMaster())
	{
		ImGui_ImplGlfwGL3_CharCallback(gEngine->getCurrentWindowPtr()->getWindowHandle(), c);
	}
}

void myMouseButtonCallback(int button, int action)
{
	if (gEngine->isMaster())
	{
		ImGui_ImplGlfwGL3_MouseButtonCallback(gEngine->getCurrentWindowPtr()->getWindowHandle(), button, action, 0);
	}
}

void myMouseScrollCallback(double xoffset, double yoffset)
{
	if (gEngine->isMaster())
	{
		ImGui_ImplGlfwGL3_ScrollCallback(gEngine->getCurrentWindowPtr()->getWindowHandle(), xoffset, yoffset);
	}
}

void myContextCreationCallback(GLFWwindow * win)
{
    glfwWindowHint( GLFW_VISIBLE, GL_FALSE );
    
    sharedWindow = win;

    hiddenPlaneCaptureWindow = glfwCreateWindow( 1, 1, "Thread Capture Window", NULL, sharedWindow ); 
    if( !hiddenPlaneCaptureWindow)
    {
        sgct::MessageHandler::instance()->print("Failed to create capture context!\n");
    }

	hiddenTransferWindow = glfwCreateWindow(1, 1, "Thread Transfer Window", NULL, sharedWindow);
	if (!hiddenTransferWindow)
	{
		sgct::MessageHandler::instance()->print("Failed to create transfer context!\n");
	}
    
    //restore to normal
    glfwMakeContextCurrent( sharedWindow );
}

void myDataTransferDecoder(void * receivedData, int receivedlength, int packageId, int clientIndex)
{
    sgct::MessageHandler::instance()->print("Decoding %d bytes in transfer id: %d on node %d\n", receivedlength, packageId, clientIndex);

    lastPackage.setVal(packageId);

	//stop capture
	bool captureStarted = planeCaptureRunning.getVal();
	if (captureStarted) {
		stopPlaneCapture();
		captureLockStatus.setVal(captureLockStatus.getVal() + 1);
	}
    
    //read the image on slave
    readImage( reinterpret_cast<unsigned char*>(receivedData), receivedlength);
    uploadTexture();

	//start capture
	if (captureStarted) {
		startPlaneCapture();
		captureLockStatus.setVal(captureLockStatus.getVal() - 1);
	}
}

void myDataTransferStatus(bool connected, int clientIndex)
{
    sgct::MessageHandler::instance()->print("Transfer node %d is %s.\n", clientIndex, connected ? "connected" : "disconnected");
}

void myDataTransferAcknowledge(int packageId, int clientIndex)
{
    sgct::MessageHandler::instance()->print("Transfer id: %d is completed on node %d.\n", packageId, clientIndex);
    
    static int counter = 0;
    if( packageId == lastPackage.getVal())
    {
        counter++;
        if( counter == (sgct_core::ClusterManager::instance()->getNumberOfNodes()-1) )
        {
            clientsUploadDone = true;
            counter = 0;
            
            sgct::MessageHandler::instance()->print("Time to distribute and upload textures on cluster: %f ms\n", (sgct::Engine::getTime() - sendTimer)*1000.0);
        }
    }
}

void threadWorker()
{
    while (running.getVal())
    {
        //runs only on master
        if (transfer.getVal() && !serverUploadDone.getVal() && !clientsUploadDone.getVal())
        {
			//stop capture
			bool captureStarted = planeCaptureRunning.getVal();
			if (captureStarted) {
				stopPlaneCapture();
				captureLockStatus.setVal(captureLockStatus.getVal() + 1);
			}

            startDataTransfer();
            transfer.setVal(false);
            
            //load textures on master
            uploadTexture();
            serverUploadDone = true;
            
            if(sgct_core::ClusterManager::instance()->getNumberOfNodes() == 1) //no cluster
            {
                clientsUploadDone = true;
            }

			//start capture
			if (captureStarted) {
				startPlaneCapture();
				captureLockStatus.setVal(captureLockStatus.getVal() - 1);
			}
        }

        sgct::Engine::sleep(0.1); //ten iteration per second
    }
}

void startDataTransfer()
{
    //iterate
    int id = lastPackage.getVal();
    id++;

    //make sure to keep within bounds
    if(static_cast<int>(imagePathsVec.size()) > id)
    {
        sendTimer = sgct::Engine::getTime();

        int imageCounter = static_cast<int32_t>(imagePathsVec.size());
        lastPackage.setVal(imageCounter - 1);

        for (int i = id; i < imageCounter; i++)
        {
            //load from file
            std::pair<std::string, int> tmpImagePair = imagePathsVec.at(static_cast<std::size_t>(i));

            std::ifstream file(tmpImagePair.first.c_str(), std::ios::binary);
            file.seekg(0, std::ios::end);
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<char> buffer(size + headerSize);
            char type = tmpImagePair.second;

            //write header (single unsigned char)
            buffer[0] = type;

            if (file.read(buffer.data() + headerSize, size))
            {
                //transfer
                gEngine->transferDataBetweenNodes(buffer.data(), static_cast<int>(buffer.size()), i);

                //read the image on master
                readImage(reinterpret_cast<unsigned char *>(buffer.data()), static_cast<int>(buffer.size()));
            }
        }
    }
}

void readImage(unsigned char * data, int len)
{
    mutex.lock();
    
    sgct_core::Image * img = new (std::nothrow) sgct_core::Image();
    
    char type = static_cast<char>(data[0]);
    
    bool result = false;
    switch( type )
    {
        case IM_JPEG:
            result = img->loadJPEG(reinterpret_cast<unsigned char*>(data + headerSize), len - headerSize);
            break;
            
        case IM_PNG:
            result = img->loadPNG(reinterpret_cast<unsigned char*>(data + headerSize), len - headerSize);
            break;
    }
    
    if (!result)
    {
        //clear if failed
        delete img;
    }
	else {
		transImages.push_back(img);
	}

    mutex.unlock();
}

void uploadTexture()
{
    mutex.lock();

    if (!transImages.empty())
    {
        glfwMakeContextCurrent(hiddenTransferWindow);

        for (std::size_t i = 0; i < transImages.size(); i++)
        {
            if (transImages[i])
            {
                //create texture
                GLuint tex;
                glGenTextures(1, &tex);
                glBindTexture(GL_TEXTURE_2D, tex);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

                GLenum internalformat;
                GLenum type;
                size_t bpc = transImages[i]->getBytesPerChannel();

                switch (transImages[i]->getChannels())
                {
                case 1:
                    internalformat = (bpc == 1 ? GL_R8 : GL_R16);
                    type = GL_RED;
                    break;

                case 2:
                    internalformat = (bpc == 1 ? GL_RG8 : GL_RG16);
                    type = GL_RG;
                    break;

                case 3:
                default:
                    internalformat = (bpc == 1 ? GL_RGB8 : GL_RGB16);
                    type = GL_BGR;
                    break;

                case 4:
                    internalformat = (bpc == 1 ? GL_RGBA8 : GL_RGBA16);
                    type = GL_BGRA;
                    break;
                }

                GLenum format = (bpc == 1 ? GL_UNSIGNED_BYTE : GL_UNSIGNED_SHORT);

                glTexStorage2D(GL_TEXTURE_2D, 1, internalformat, static_cast<GLsizei>(transImages[i]->getWidth()), static_cast<GLsizei>(transImages[i]->getHeight()));
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLsizei>(transImages[i]->getWidth()), static_cast<GLsizei>(transImages[i]->getHeight()), type, format, transImages[i]->getData());

                //---------------------
                // Disable mipmaps
                //---------------------
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                //unbind
                glBindTexture(GL_TEXTURE_2D, GL_FALSE);

                sgct::MessageHandler::instance()->print("Texture id %d loaded (%dx%dx%d).\n", tex, transImages[i]->getWidth(), transImages[i]->getHeight(), transImages[i]->getChannels());

                texIds.addVal(tex);
				float aspectRatio = (static_cast<float>(transImages[i]->getWidth()) / static_cast<float>(transImages[i]->getHeight()));
				texAspectRatio.addVal(aspectRatio);

                delete transImages[i];
                transImages[i] = NULL;
            }
            else //if invalid load
            {
                texIds.addVal(GL_FALSE);
				texAspectRatio.addVal(-1.f);
            }
        }//end for

        transImages.clear();
        glFinish();

        //restore
        glfwMakeContextCurrent(NULL);
    }//end if not empty

    mutex.unlock();
}

void myDropCallback(int count, const char** paths)
{
    if (gEngine->isMaster())
    {
        std::vector<std::string> pathStrings;
        for (int i = 0; i < count; i++)
        {
            //simpy pick the first path to transmit
            std::string tmpStr(paths[i]);

            //transform to lowercase
            std::transform(tmpStr.begin(), tmpStr.end(), tmpStr.begin(), ::tolower);

            pathStrings.push_back(tmpStr);
        }

        //sort in alphabetical order
        std::sort(pathStrings.begin(), pathStrings.end());

        serverUploadCount.setVal(0);

        //iterate all drop paths
        for (int i = 0; i < pathStrings.size(); i++)
        {
			//find and add supported file type
			transferSupportedFiles(pathStrings[i]);
        }
    }
}

void transferSupportedFiles(std::string pathStr) {
	//find and add supported file type
	bool found = false;
	int type = 0;
	if (pathStr.find(".jpg") != std::string::npos || pathStr.find(".jpeg") != std::string::npos) {
		type = IM_JPEG;
		found = true;
	}
	else if (pathStr.find(".png") != std::string::npos) {
		type = IM_PNG;
		found = true;
	}
	if (found) {
		imagePathsVec.push_back(std::pair<std::string, int>(pathStr, type));
		std::string fileName = getFileName(pathStr);
		imagePathsMap.insert(std::pair<std::string, int>(fileName, static_cast<int>(imagePathsVec.size() - 1)));
		domeImageFileNames.push_back(fileName);
		planeImageFileNames.push_back(fileName);
		transfer.setVal(true); //tell transfer thread to start processing data
		serverUploadCount++;
	}
}

void parseArguments(int& argc, char**& argv)
{
    int i = 0;
    while (i < argc)
    {
        if (strcmp(argv[i], "-host") == 0 && argc > (i + 1))
        {
			std::string host = std::string(argv[i + 1]);
            gPlaneCapture->setVideoHost(host);
#ifdef RGBEASY_ENABLED
			gPlaneDPCapture->setCaptureHost(host);
			gFisheyeCapture->setCaptureHost(host);
#endif
        }
        else if (strcmp(argv[i], "-video") == 0 && argc > (i + 1))
        {
            gPlaneCapture->setVideoDevice(std::string(argv[i + 1]));
        }
		else if (strcmp(argv[i], "-option") == 0 && argc > (i + 2))
		{
			std::string option = std::string(argv[i + 1]);
			std::string value = std::string(argv[i + 2]);
			gPlaneCapture->addOption(std::make_pair(option, value));
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Added capture option %s, parameter %s\n", option.c_str(), value.c_str());
		}
		else if (strcmp(argv[i], "-flip") == 0)
		{
			flipFrame = true;
		}
#ifdef RGBEASY_ENABLED
		else if (strcmp(argv[i], "-planecapture") == 0)
		{
			planeDPCaptureRequested = true;
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "RGBEasy plane capture requested\n");
		}
		else if (strcmp(argv[i], "-planeinput") == 0 && argc >(i + 1))
		{
			int captureInput = static_cast<int>(atoi(argv[i + 1]));
			gPlaneDPCapture->setCaptureInput(captureInput);
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "RGBEasy plane capture on input %i\n", captureInput);
		}
		else if (strcmp(argv[i], "-fisheyecapture") == 0)
		{
			fisheyeCaptureRequested = true;
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "RGBEasy fisheye capture requested\n");
		}
		else if (strcmp(argv[i], "-fisheyeinput") == 0 && argc >(i + 1))
		{
			int captureInput = static_cast<int>(atoi(argv[i + 1]));
			gFisheyeCapture->setCaptureInput(captureInput);
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "RGBEasy fisheye capture on input %i\n", captureInput);
		}
		else if (strcmp(argv[i], "-fisheyeganging") == 0)
		{
			gFisheyeCapture->setCaptureGanging(true);
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Fisheye/capture ganging enabled\n");
		}
#endif
		else if (strcmp(argv[i], "-defaultfisheye") == 0)
		{
			std::string defaultFisheye = std::string(argv[i + 1]);
			std::string delim = ";";
			size_t start = 0U;
			size_t end = defaultFisheye.find(delim);
			while (end != std::string::npos)
			{
				defaultFisheyes.push_back(defaultFisheye.substr(start, end - start));
				start = end + delim.length();
				end = defaultFisheye.find(delim, start);
			}
			defaultFisheyes.push_back(defaultFisheye.substr(start, end));
		}
		else if (strcmp(argv[i], "-defaultfisheyedelay") == 0)
		{
			defaultFisheyeDelay = std::stod(std::string(argv[i + 1]).c_str());
		}
        /*else if (strcmp(argv[i], "-plane") == 0 && argc > (i + 3))
        {
            planeAzimuth = static_cast<float>(atof(argv[i + 1]));
            planeElevation = static_cast<float>(atof(argv[i + 2]));
            planeRoll = static_cast<float>(atof(argv[i + 3]));
        }*/

        i++; //iterate
    }
}

GLuint allocateCaptureTexture()
{
    int w = planceCaptureWidth;
    int h = planeCaptureHeight;

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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 24);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return texId;
}

void uploadCaptureData(uint8_t ** data, int width, int height)
{
    // At least two textures and GLSync objects
    // should be used to control that the uploaded texture is the same
    // for all viewports to prevent any tearing and maintain frame sync

    if (planeCaptureTexId)
    {
#ifdef ZXING_ENABLED
        if(checkQRoperations(data, width, height, flipFrame)) {
		/*// If result is not empty, we have to interpret the message to decide it the plane should lock the capture to the previous frame or update it.
		std::vector<std::string> decodedResults;
		if(planeCapturePresMode.getVal())
			decodedResults = QRCodeInterpreter::decodeImageMulti(BGR24LuminanceSource::create(data, width, height, flipFrame));
		
		if (!decodedResults.empty()) {
			//Save only unique operations
			for each(std::string decodedResult in decodedResults) {
				if (std::find(operationsQueue.begin(), operationsQueue.end(), decodedResult) == operationsQueue.end()) {
					//Operation not in queue, add it
					sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Decode %i characters, with resulting string: %s\n", decodedResult.size(), decodedResult.c_str());
					operationsQueue.push_back(decodedResult.c_str());
				}
			}
		}
		else {
			std::vector<ContentPlaneLocalAttribs> pAL = planeAttributesLocal.getVal();
			std::vector<ContentPlaneGlobalAttribs> pAG = planeAttributesGlobal.getVal();
			if (!operationsQueue.empty()) {
				// Now we can process them when decodedResults is empty
				for (size_t i = 0; i < operationsQueue.size(); i++) {
					sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Applying Operation: %s\n", operationsQueue[i].c_str());
					std::vector<std::string> operation = split(operationsQueue[i].c_str(), ';');
					if (operation.size() > 1) {
						int capturePlaneIdx = -1;
						for (int p = 0; p < captureContentPlanes.size(); p++) {
							if (pAL[p].name == operation[0]) {
								capturePlaneIdx = p;
								break;
							}
						}
						if (capturePlaneIdx >= 0) {
							for (int o = 1; o < operation.size(); o++) {
								if (operation[o] == "SetActive") {
									// Setting capturePlaneIdx as active capture plane
									pAL[capturePlaneIdx].previouslyVisible = false;
									pAL[capturePlaneIdx].freeze = false;
									pAG[capturePlaneIdx].planeTexId = 0;

									//Freezing other planes which are not already frozen
									for (int p = 0; p < captureContentPlanes.size(); p++) {
										if (p != capturePlaneIdx && !pAL[p].freeze) {
											pAL[p].freeze = true;
											glCopyImageSubData(planeCaptureTexId, GL_TEXTURE_2D, 0, 0, 0, 0, planeTexOwnedIds[p], GL_TEXTURE_2D, 0, 0, 0, 0, width, height, 1);
											glFlush();
										}
									}
									
									// Setting capturePlaneIdx as active capture plane
									pAL[capturePlaneIdx].currentlyVisible = true;
								}
							}
						}
						else if (operation[0] == "AllCaptures") {
							if (operation[1] == "Clear") {
								//Making all planes fade out
								for (int p = 0; p < captureContentPlanes.size(); p++) {
									//Need to freeze all planes
									if (!pAL[p].freeze) {
										pAL[p].freeze = true;
										glCopyImageSubData(planeCaptureTexId, GL_TEXTURE_2D, 0, 0, 0, 0, planeTexOwnedIds[p], GL_TEXTURE_2D, 0, 0, 0, 0, width, height, 1);
										glFlush();
									}
									pAL[p].currentlyVisible = false;
								}
							}
						}
						else {
							sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Could not find plane named: %s\n", operation[0].c_str());
						}
					}
				}
			}*/
#endif
			unsigned char * GPU_ptr = reinterpret_cast<unsigned char*>(glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));
			if (GPU_ptr)
			{
				int dataOffset = 0;
				int stride = width * 3; //Assuming BGR24
				if (gPlaneCapture->isFormatYUYV422()) {
					stride = width * 2;
				}

				if (flipFrame)
				{
					for (int row = 0; row < height; row++)
					{
						memcpy(GPU_ptr + dataOffset, data[0] + row * stride, stride);
						dataOffset += stride;
					}
				}
				else
				{
					for (int row = height - 1; row > -1; row--)
					{
						memcpy(GPU_ptr + dataOffset, data[0] + row * stride, stride);
						dataOffset += stride;
					}
				}

				glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, planeCaptureTexId);

				if (gPlaneCapture->isFormatYUYV422()) {
					//AV_PIX_FMT_YUYV422
					//int y1, u, y2, v;
					glTexImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, 0);
				}
				else { //Assuming BGR24
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, 0);
				}
			}
#ifdef ZXING_ENABLED
			/*if (!operationsQueue.empty()) {
				planeAttributesLocal.setVal(pAL);
				operationsQueue.clear();
			}*/
		}
#endif

        //calculateStats();
    }
}

void planeCaptureLoop()
{
    glfwMakeContextCurrent(hiddenPlaneCaptureWindow);

    int dataSize = planceCaptureWidth * planeCaptureHeight * 3;
    GLuint PBO;
    glGenBuffers(1, &PBO);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, 0, GL_DYNAMIC_DRAW);

    while (planeCaptureRunning.getVal())
    {
		gPlaneCapture->poll();
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
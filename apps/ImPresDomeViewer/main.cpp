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
#include "Capture.hpp"

#ifdef ZXING_ENABLED
#include "BGR24LuminanceSource.h"
#include "QRCodeInterpreter.h"
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
Capture * gCapture = NULL;
Capture * gFisheyeCapture1 = NULL;
Capture * gFisheyeCapture2 = NULL;

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

GLFWwindow * hiddenCaptureWindow;
GLFWwindow * hiddenFisheyeCapture1Window;
GLFWwindow * hiddenFisheyeCapture2Window;
GLFWwindow * hiddenTransferWindow;
GLFWwindow * sharedWindow;

//variables to share across cluster
sgct::SharedDouble curr_time(0.0);
sgct::SharedBool info(false);
sgct::SharedBool stats(false);
sgct::SharedFloat fadingTime(2.0f);

//structs
struct ContentPlane {
	std::string name;
	float height;
	float azimuth;
	float elevation;
	float roll;
	bool currentlyVisible;
	bool previouslyVisible;
	double fadeStartTime;
	int planeStrId;
	int planeTexId;
	bool freeze;

	ContentPlane(std::string n, float h, float a, float e, float r, bool ca = true, bool pa = true, double f = -1.0, int pls = 0, int pli = 0, bool fr = false) :
		name(n),
		height(h), 
		azimuth(a) ,
		elevation(e) ,
		roll(r) , 
		currentlyVisible(ca) , 
		previouslyVisible(pa) ,
		fadeStartTime(f) ,
		planeStrId(pls) ,
		planeTexId(pli) ,
		freeze(fr)
	{};
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
void threadWorker(void *arg);

tthread::thread * loadThread;
tthread::mutex mutex;
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

//FFmpegCapture
void uploadCaptureData(uint8_t ** data, int width, int height);
void uploadFisheye1CaptureData(uint8_t ** data, int width, int height);
void uploadFisheye2CaptureData(uint8_t ** data, int width, int height);
void parseArguments(int& argc, char**& argv);
GLuint allocateCaptureTexture();
GLuint allocateFisheyeCaptureTexture();
GLuint allocateFisheyeTwinCaptureTexture();
void captureLoop(void *arg);
void fisheyeCaptureLoop1(void *arg);
void fisheyeCaptureLoop2(void *arg);
void calculateStats();
void startCapture();
void stopCapture();
void startFisheyeCapture();
void stopFisheyeCapture();
void createPlanes();

GLint Matrix_Loc = -1;
GLint ScaleUV_Loc = -1;
GLint OffsetUV_Loc = -1;
GLint Opacity_Loc = -1;
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
GLuint captureTexId = GL_FALSE;
GLuint fisheyeCaptureTexId = GL_FALSE;

tthread::thread * captureThread;
tthread::thread * fisheyeCapture1Thread;
tthread::thread * fisheyeCapture2Thread;
bool usingTwinFisheye = false;
bool flipFrame = false;
bool flipFisheye1 = false;
bool flipFisheye2 = false;
bool fulldomeMode = false;

glm::vec2 planeScaling(1.0f, 1.0f);
glm::vec2 planeOffset(0.0f, 0.0f);

sgct::SharedBool captureRunning(true);
sgct::SharedBool fisheyeCaptureLoop(true);
sgct::SharedInt captureLockStatus(0);
sgct::SharedBool renderDome(fulldomeMode);
sgct::SharedDouble captureRate(0.0);
sgct::SharedInt32 domeCut(2);
sgct::SharedInt planeScreenAspect(1610);
sgct::SharedInt planeMaterialAspect(169);
sgct::SharedBool planeCapturePresMode(false);
sgct::SharedBool planeUseCaptureSize(false);
sgct::SharedVector<ContentPlane> planeAttributes;
sgct::SharedBool planeReCreate(false);
sgct::SharedBool chromaKey(false);
sgct::SharedObject<glm::vec3> chromaKeyColor(glm::vec3(0.f, 177.f, 64.f));
sgct::SharedFloat chromaKeyFactor(22.f);

std::vector<GLuint> planeTexOwnedIds;

#ifdef ZXING_ENABLED
std::vector<std::string> operationsQueue;
#endif

//ImGUI variables
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
    gCapture = new Capture();
	gFisheyeCapture1 = new Capture();
	gFisheyeCapture2 = new Capture();

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

	if(captureRunning.getVal())
		stopCapture();

	if (fisheyeCaptureLoop.getVal())
		stopFisheyeCapture();

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
    delete gCapture;
	delete gFisheyeCapture1;
	delete gFisheyeCapture2;
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

#ifdef OPENVR_SUPPORT
	if (FirstOpenVRWindow) {
		//Update pose matrices for all tracked OpenVR devices once per frame
		sgct::SGCTOpenVR::updatePoses();
	}
#endif
}

float getContentPlaneOpacity(int planeIdx) {
	std::vector<ContentPlane> pA = planeAttributes.getVal();
	ContentPlane pl = pA[planeIdx];

	float planeOpacity = 1.f;

	if (pl.currentlyVisible != pl.previouslyVisible && pl.fadeStartTime == -1.0) {
		pl.fadeStartTime = curr_time.getVal();
		pl.previouslyVisible = pl.currentlyVisible;
		
		pA[planeIdx] = pl;
		planeAttributes.setVal(pA);
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
			pA[planeIdx] = pl;
			planeAttributes.setVal(pA);
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

    if (domeTexIndex.getVal() != -1 && !fulldomeMode)// && texIds.getSize() > domeTexIndex.getVal())
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
			sgct::ShaderManager::instance()->bindShaderProgram("xform");
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texIds.getValAt(domeTexIndex.getVal()));
			glUniform2f(ScaleUV_Loc, 1.f, 1.f);
			glUniform2f(OffsetUV_Loc, 0.f, 0.f);
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
        sgct::ShaderManager::instance()->bindShaderProgram("xform");
    }

    glFrontFace(GL_CCW);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//stop capture if necassary
	bool captureStarted = captureRunning.getVal();
	if (captureStarted && captureLockStatus.getVal() == 1) {
		stopCapture();
		stopFisheyeCapture();
	}

	//No capture planes when taking screenshot
	if (!screenshotPassOn) {
		if (fulldomeMode)
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, captureTexId);
			glm::vec2 texSize = glm::vec2(static_cast<float>(gCapture->getWidth()), static_cast<float>(gCapture->getHeight()));

			glUniform1f(Opacity_L, 1.f);

			// TextureCut 2 equals showing only the middle square of a capturing a widescreen input
			if (domeCut.getVal() == 2) {
				glUniform2f(ScaleUV_L, texSize.y / texSize.x, 1.f);
				glUniform2f(OffsetUV_L, ((texSize.x - texSize.y)*0.5f) / texSize.x, 0.f);
			}
			else {
				glUniform2f(ScaleUV_L, 1.f, 1.f);
				glUniform2f(OffsetUV_L, 0.f, 0.f);
			}

			glCullFace(GL_FRONT); //camera on the inside of the dome

			glUniformMatrix4fv(Matrix_L, 1, GL_FALSE, &MVP[0][0]);
			dome->draw();
		}
		else //plane mode
		{
			glUniform2f(ScaleUV_L, planeScaling.x, planeScaling.y);
			glUniform2f(OffsetUV_L, planeOffset.x, planeOffset.y);

			glCullFace(GL_BACK);

			for (int i = 0; i < captureContentPlanes.size(); i++) {
				glActiveTexture(GL_TEXTURE0);
				if (planeAttributes.getVal()[i].freeze) {
					glBindTexture(GL_TEXTURE_2D, planeTexOwnedIds[i]);
				}
				else if (planeAttributes.getVal()[i].planeStrId > 0) {
					glBindTexture(GL_TEXTURE_2D, texIds.getValAt(planeAttributes.getVal()[i].planeTexId));
				}
				else {
					glBindTexture(GL_TEXTURE_2D, captureTexId);
				}

				float planeOpacity = getContentPlaneOpacity(i);
				if (planeOpacity > 0.f) {
					glUniform1f(Opacity_L, planeOpacity);

					glm::mat4 capturePlaneTransform = glm::mat4(1.0f);
					capturePlaneTransform = glm::rotate(capturePlaneTransform, glm::radians(planeAttributes.getVal()[i].azimuth), glm::vec3(0.0f, -1.0f, 0.0f)); //azimuth
					capturePlaneTransform = glm::rotate(capturePlaneTransform, glm::radians(planeAttributes.getVal()[i].elevation), glm::vec3(1.0f, 0.0f, 0.0f)); //elevation
					capturePlaneTransform = glm::rotate(capturePlaneTransform, glm::radians(planeAttributes.getVal()[i].roll), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
					capturePlaneTransform = glm::translate(capturePlaneTransform, glm::vec3(0.0f, 0.0f, -5.0f)); //distance

					capturePlaneTransform = MVP * capturePlaneTransform;
					glUniformMatrix4fv(Matrix_L, 1, GL_FALSE, &capturePlaneTransform[0][0]);

					captureContentPlanes[i]->draw();
				}
			}
		}
	}

	float planeOpacity;
	for (int i = static_cast<int>(captureContentPlanes.size()); i < planeAttributes.getSize(); i++) {
		planeOpacity = getContentPlaneOpacity(i);
		if (planeOpacity > 0.f && masterContentPlanes.size() > i-captureContentPlanes.size()) {
			glActiveTexture(GL_TEXTURE0);
			if (planeAttributes.getVal()[i].planeStrId > 0) {
				glBindTexture(GL_TEXTURE_2D, texIds.getValAt(planeAttributes.getVal()[i].planeTexId));
			}
			else {
				glBindTexture(GL_TEXTURE_2D, captureTexId);
			}

			glUniform1f(Opacity_L, planeOpacity);

			glUniform2f(ScaleUV_L, 1.0f, 1.0f);
			glUniform2f(OffsetUV_L, 0.0f, 0.0f);

			//transform and draw plane
			glm::mat4 contentPlaneTransform = glm::mat4(1.0f);
			contentPlaneTransform = glm::rotate(contentPlaneTransform, glm::radians(planeAttributes.getVal()[i].azimuth), glm::vec3(0.0f, -1.0f, 0.0f)); //azimuth
			contentPlaneTransform = glm::rotate(contentPlaneTransform, glm::radians(planeAttributes.getVal()[i].elevation), glm::vec3(1.0f, 0.0f, 0.0f)); //elevation
			contentPlaneTransform = glm::rotate(contentPlaneTransform, glm::radians(planeAttributes.getVal()[i].roll), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
			contentPlaneTransform = glm::translate(contentPlaneTransform, glm::vec3(0.0f, 0.0f, -5.5f)); //distance

			contentPlaneTransform = MVP * contentPlaneTransform;
			glUniformMatrix4fv(Matrix_L, 1, GL_FALSE, &contentPlaneTransform[0][0]);

			masterContentPlanes[i- captureContentPlanes.size()]->draw();
		}
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

        sgct_text::print(font, sgct_text::TOP_LEFT,
            padding, static_cast<float>(gEngine->getCurrentWindowPtr()->getYFramebufferResolution() - font_size) - padding, //x and y pos
            glm::vec4(1.0, 1.0, 1.0, 1.0), //color
            "Format: %s\nResolution: %d x %d\nRate: %.2lf Hz",
            gCapture->getFormat(),
            gCapture->getWidth(),
            gCapture->getHeight(),
            captureRate.getVal());
    }

	bool drawGUI = true;
#ifdef OPENVR_SUPPORT
	if (FirstOpenVRWindow == gEngine->getCurrentWindowPtr())
		drawGUI = false;
#endif
	if (gEngine->isMaster() && !screenshotPassOn && drawGUI)
	{
		ImGui_ImplGlfwGL3_NewFrame(gEngine->getCurrentWindowPtr()->getXFramebufferResolution(), gEngine->getCurrentWindowPtr()->getYFramebufferResolution());

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
					planeAttributes.addVal(ContentPlane(name, 1.6f, 0.f, 85.f, 0.f));
				}
				ImGui::Combo("Currently Editing", &imPlaneIdx, imPlanes);
				ImGui::Checkbox("Show Plane", &imPlaneShow);
				ImGui::Combo("Plane Source", &imPlaneImageIdx, planeImageFileNames);
				ImGui::SliderFloat("Plane Height", &imPlaneHeight, 0.1f, 10.0f);
				ImGui::SliderFloat("Plane Azimuth", &imPlaneAzimuth, -180.f, 180.f);
				ImGui::SliderFloat("Plane Elevation", &imPlaneElevation, -180.f, 180.f);
				ImGui::SliderFloat("Plane Roll", &imPlaneRoll, -180.f, 180.f);
			}
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

void startCapture()
{
	//start capture thread if host or load thread if master and not host
	sgct_core::SGCTNode * thisNode = sgct_core::ClusterManager::instance()->getThisNodePtr();
	if (thisNode->getAddress() == gCapture->getVideoHost()) {
		captureRunning.setVal(true);
		captureThread = new (std::nothrow) tthread::thread(captureLoop, NULL);
	}
}

void stopCapture()
{
	//kill capture thread
	captureRunning.setVal(false);
	if (captureThread)
	{
		captureThread->join();
		delete captureThread;
	}
}

void startFisheyeCapture()
{
	//start capture thread if host or load thread if master and not host
	sgct_core::SGCTNode * thisNode = sgct_core::ClusterManager::instance()->getThisNodePtr();	
	if (usingTwinFisheye && thisNode->getAddress() == gFisheyeCapture2->getVideoHost()) {
		fisheyeCaptureLoop.setVal(true);
		fisheyeCapture2Thread = new (std::nothrow) tthread::thread(fisheyeCaptureLoop2, NULL);
	}
	if (thisNode->getAddress() == gFisheyeCapture1->getVideoHost()) {
		fisheyeCaptureLoop.setVal(true);
		fisheyeCapture1Thread = new (std::nothrow) tthread::thread(fisheyeCaptureLoop1, NULL);
	}
}

void stopFisheyeCapture()
{
	//kill capture thread
	fisheyeCaptureLoop.setVal(false);
	if (fisheyeCapture1Thread)
	{
		fisheyeCapture1Thread->join();
		delete fisheyeCapture1Thread;
	}
	if (fisheyeCapture2Thread)
	{
		fisheyeCapture2Thread->join();
		delete fisheyeCapture2Thread;
	}
}

void createPlanes() {
	//Capture planes
	int capturePlaneSize = static_cast<int>(captureContentPlanes.size());
	for (int i = 0; i < capturePlaneSize; i++) {
		delete captureContentPlanes[i];
	}
	captureContentPlanes.clear();

	float captureRatio = (static_cast<float>(gCapture->getWidth()) / static_cast<float>(gCapture->getHeight()));

	for (int i = 0; i < capturePlaneSize; i++) {
		float planeWidth = planeAttributes.getVal()[i].height * captureRatio;

		if (planeUseCaptureSize.getVal())
		{
			captureContentPlanes.push_back(new sgct_utils::SGCTPlane(planeWidth, planeAttributes.getVal()[i].height));
		}
		else
		{
			switch (planeMaterialAspect.getVal())
			{
			case 1610:
				captureContentPlanes.push_back(new sgct_utils::SGCTPlane((planeAttributes.getVal()[i].height / 10.0f) * 16.0f, planeAttributes.getVal()[i].height));
				break;
			case 169:
				captureContentPlanes.push_back(new sgct_utils::SGCTPlane((planeAttributes.getVal()[i].height / 9.0f) * 16.0f, planeAttributes.getVal()[i].height));
				break;
			case 54:
				captureContentPlanes.push_back(new sgct_utils::SGCTPlane((planeAttributes.getVal()[i].height / 4.0f) * 5.0f, planeAttributes.getVal()[i].height));
				break;
			case 43:
				captureContentPlanes.push_back(new sgct_utils::SGCTPlane((planeAttributes.getVal()[i].height / 3.0f) * 4.0f, planeAttributes.getVal()[i].height));
				break;
			default:
				captureContentPlanes.push_back(new sgct_utils::SGCTPlane(planeWidth, planeAttributes.getVal()[i].height));
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

	for (size_t i = captureContentPlanes.size(); i < planeAttributes.getSize(); i++) {
		float width = planeAttributes.getVal()[i].height * texAspectRatio.getValAt(planeAttributes.getVal()[i].planeTexId);
		masterContentPlanes.push_back(new sgct_utils::SGCTPlane(width, planeAttributes.getVal()[i].height));
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

    bool captureReady = gCapture->init();
    //allocate texture
	if (captureReady) {
		captureTexId = allocateCaptureTexture();
	}
	planeImageFileNames.push_back("Single Capture");

	if (captureReady) {
		//start capture
		startCapture();
	}

	bool fisheyeCaptureReady = gFisheyeCapture1->init();
	//allocate fisheye texture
	if (fisheyeCaptureReady) {
		if (usingTwinFisheye) {
			fisheyeCaptureReady = gFisheyeCapture2->init();
		}

		if (fisheyeCaptureReady && usingTwinFisheye) {
			fisheyeCaptureTexId = allocateFisheyeTwinCaptureTexture();		
		}
		else {
			usingTwinFisheye = false;
			fisheyeCaptureTexId = allocateFisheyeCaptureTexture();
		}
	}
	domeImageFileNames.push_back("Fisheye Capture");
	texIds.addVal(fisheyeCaptureTexId);
	texAspectRatio.addVal(1.f);
	imagePathsVec.push_back(std::pair<std::string, int>("", 0));
	imagePathsMap.insert(std::pair<std::string, int>(domeImageFileNames[0], 0));
	lastPackage.setVal(0);
	domeTexIndex.setVal(0);
	currentDomeTexIdx = domeTexIndex.getVal();
	numSyncedTex = 1;

	if (fisheyeCaptureReady) {
		//start capture
		startFisheyeCapture();
	}

	//start load thread
    if (gEngine->isMaster())
        loadThread = new (std::nothrow) tthread::thread(threadWorker, NULL);

    std::function<void(uint8_t ** data, int width, int height)> callback = uploadCaptureData;
    gCapture->setVideoDecoderCallback(callback);

	std::function<void(uint8_t ** data, int width, int height)> fisheyecallback1 = uploadFisheye1CaptureData;
	gFisheyeCapture1->setVideoDecoderCallback(fisheyecallback1);

	std::function<void(uint8_t ** data, int width, int height)> fisheyecallback2 = uploadFisheye2CaptureData;
	gFisheyeCapture2->setVideoDecoderCallback(fisheyecallback2);

	//define capture planes
	imPlanes.push_back("FrontCapture");
	ContentPlane frontCapture = ContentPlane("FrontCapture", imPlaneHeight, imPlaneAzimuth, imPlaneElevation, imPlaneRoll, true);
	planeTexOwnedIds.push_back(allocateCaptureTexture());
	planeAttributes.addVal(frontCapture);
	captureContentPlanes.push_back(nullptr);

	imPlanes.push_back("BackCapture");
	ContentPlane backCapture = ContentPlane("BackCapture", 1.8f, -155.f, 20.f, imPlaneRoll, false);
	planeTexOwnedIds.push_back(allocateCaptureTexture());
	planeAttributes.addVal(backCapture);
	captureContentPlanes.push_back(nullptr);

	imPlanes.push_back("LeftCapture");
	ContentPlane leftCapture = ContentPlane("LeftCapture", -2.865, -75.135f, 26.486f, imPlaneRoll, false);
	planeTexOwnedIds.push_back(allocateCaptureTexture());
	planeAttributes.addVal(leftCapture);
	captureContentPlanes.push_back(nullptr);

	imPlanes.push_back("RightCapture");
	ContentPlane rightCapture = ContentPlane("RightCapture", 2.865, 75.135f, 26.486f, imPlaneRoll, false);
	planeTexOwnedIds.push_back(allocateCaptureTexture());
	planeAttributes.addVal(rightCapture);
	captureContentPlanes.push_back(nullptr);

	imPlanes.push_back("TopCapture");
	ContentPlane topCapture = ContentPlane("TopCapture", imPlaneHeight, 0.f, 75.135, imPlaneRoll, false);
	planeTexOwnedIds.push_back(allocateCaptureTexture());
	planeAttributes.addVal(topCapture);
	captureContentPlanes.push_back(nullptr);

	//define default content plane
	//imPlanes.push_back("Content 1");
	//planeAttributes.addVal(ContentPlane(1.6f, 0.f, 95.0f, 0.f));

    //create plane
	createPlanes();

    //create dome
    dome = new sgct_utils::SGCTDome(7.4f, 180.0f, 256, 128);

    sgct::ShaderManager::instance()->addShaderProgram( "xform",
            "xform.vert",
            "xform.frag" );

    sgct::ShaderManager::instance()->bindShaderProgram( "xform" );

    Matrix_Loc = sgct::ShaderManager::instance()->getShaderProgram( "xform").getUniformLocation( "MVP" );
    ScaleUV_Loc = sgct::ShaderManager::instance()->getShaderProgram( "xform").getUniformLocation("scaleUV");
    OffsetUV_Loc = sgct::ShaderManager::instance()->getShaderProgram( "xform").getUniformLocation("offsetUV");
	Opacity_Loc = sgct::ShaderManager::instance()->getShaderProgram("xform").getUniformLocation("opacity");
    GLint Tex_Loc = sgct::ShaderManager::instance()->getShaderProgram( "xform").getUniformLocation( "Tex" );
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

	// Setup ImGui binding
	if (gEngine->isMaster()) {
		ImGui_ImplGlfwGL3_Init(gEngine->getCurrentWindowPtr()->getWindowHandle());

		ImGuiStyle& style = ImGui::GetStyle();
		style.IndentSpacing = 25;
		style.ItemSpacing = { 4.f, 2.f };
		style.Colors[ImGuiCol_Border] = ImVec4(0.1f, 0.39f, 0.42f, 0.59f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
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

	if (!planeCapturePresMode.getVal()) {
		std::vector<ContentPlane> pA = planeAttributes.getVal();
		if (imPlaneIdx != imPlanePreviousIdx) {
			imPlaneHeight = pA[imPlaneIdx].height;
			imPlaneAzimuth = pA[imPlaneIdx].azimuth;
			imPlaneElevation = pA[imPlaneIdx].elevation;
			imPlaneRoll = pA[imPlaneIdx].roll;
			imPlaneShow = pA[imPlaneIdx].currentlyVisible;
			imPlanePreviousIdx = imPlaneIdx;
			imPlaneImageIdx = pA[imPlaneIdx].planeStrId;
		}

		if (planeAttributes.getVal()[imPlaneIdx].height != imPlaneHeight) planeReCreate.setVal(true);
		if (planeAttributes.getVal()[imPlaneIdx].planeStrId != imPlaneImageIdx) planeReCreate.setVal(true);
		pA[imPlaneIdx] = ContentPlane(pA[imPlaneIdx].name, imPlaneHeight, imPlaneAzimuth, imPlaneElevation, imPlaneRoll, imPlaneShow, pA[imPlaneIdx].previouslyVisible,
			pA[imPlaneIdx].fadeStartTime, imPlaneImageIdx, imagePathsMap[planeImageFileNames[imPlaneImageIdx]], pA[imPlaneIdx].freeze);
		planeAttributes.setVal(pA);
		sgct::SharedData::instance()->writeVector<ContentPlane>(&planeAttributes);
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
    sgct::SharedData::instance()->readInt32(&domeCut);
	sgct::SharedData::instance()->readFloat(&fadingTime);

	sgct::SharedData::instance()->readInt32(&planeScreenAspect);
	sgct::SharedData::instance()->readInt32(&planeMaterialAspect);
	sgct::SharedData::instance()->readBool(&planeCapturePresMode);
	sgct::SharedData::instance()->readBool(&planeUseCaptureSize);
	if (!planeCapturePresMode.getVal()) {
		sgct::SharedData::instance()->readVector<ContentPlane>(&planeAttributes);
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

    if (captureTexId)
    {
        glDeleteTextures(1, &captureTexId);
		captureTexId = GL_FALSE;
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
    
    
    if(hiddenCaptureWindow)
        glfwDestroyWindow(hiddenCaptureWindow);

	if (hiddenFisheyeCapture1Window)
		glfwDestroyWindow(hiddenFisheyeCapture1Window);

	if (hiddenFisheyeCapture2Window)
		glfwDestroyWindow(hiddenFisheyeCapture2Window);

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

    hiddenCaptureWindow = glfwCreateWindow( 1, 1, "Thread Capture Window", NULL, sharedWindow ); 
    if( !hiddenCaptureWindow)
    {
        sgct::MessageHandler::instance()->print("Failed to create capture context!\n");
    }

	hiddenFisheyeCapture1Window = glfwCreateWindow(1, 1, "Thread Fisheye Capture 1 Window", NULL, sharedWindow);
	if (!hiddenFisheyeCapture1Window)
	{
		sgct::MessageHandler::instance()->print("Failed to create twin capture context!\n");
	}

	hiddenFisheyeCapture2Window = glfwCreateWindow(1, 1, "Thread Fisheye Capture 2 Window", NULL, sharedWindow);
	if (!hiddenFisheyeCapture2Window)
	{
		sgct::MessageHandler::instance()->print("Failed to create twin capture context!\n");
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
	bool captureStarted = captureRunning.getVal();
	if (captureStarted) {
		stopCapture();
		stopFisheyeCapture();
		captureLockStatus.setVal(captureLockStatus.getVal() + 1);
	}
    
    //read the image on slave
    readImage( reinterpret_cast<unsigned char*>(receivedData), receivedlength);
    uploadTexture();

	//start capture
	if (captureStarted) {
		startCapture();
		startFisheyeCapture();
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

void threadWorker(void *arg)
{
    while (running.getVal())
    {
        //runs only on master
        if (transfer.getVal() && !serverUploadDone.getVal() && !clientsUploadDone.getVal())
        {
			//stop capture
			bool captureStarted = captureRunning.getVal();
			if (captureStarted) {
				stopCapture();
				stopFisheyeCapture();
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
				startCapture();
				startFisheyeCapture();
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
                unsigned int bpc = transImages[i]->getBytesPerChannel();

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

                glTexStorage2D(GL_TEXTURE_2D, 1, internalformat, transImages[i]->getWidth(), transImages[i]->getHeight());
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, transImages[i]->getWidth(), transImages[i]->getHeight(), type, format, transImages[i]->getData());

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
            gCapture->setVideoHost(std::string(argv[i + 1]));
			gFisheyeCapture2->setVideoHost(std::string(argv[i + 1]));
			gFisheyeCapture1->setVideoHost(std::string(argv[i + 1]));
        }
        else if (strcmp(argv[i], "-video") == 0 && argc > (i + 1))
        {
            gCapture->setVideoDevice(std::string(argv[i + 1]));
        }
		else if (strcmp(argv[i], "-option") == 0 && argc > (i + 2))
		{
			gCapture->addOption(
				std::make_pair(std::string(argv[i + 1]), std::string(argv[i + 2])));
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Added capture option %s, parameter %s\n", std::string(argv[i + 1]), std::string(argv[i + 2]));
		}
		else if (strcmp(argv[i], "-flip") == 0)
		{
			flipFrame = true;
		}
		else if (strcmp(argv[i], "-fisheyetwinvideo") == 0 && argc > (i + 2))
		{
			usingTwinFisheye = true;
			gFisheyeCapture1->setVideoDevice(std::string(argv[i + 1]));
			gFisheyeCapture2->setVideoDevice(std::string(argv[i + 2]));
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Twin fisheye using devices %s and %s!\n", std::string(argv[i + 1]), std::string(argv[i + 2]));
		}
		else if (strcmp(argv[i], "-fisheyevideo") == 0 && argc > (i + 1))
		{
			usingTwinFisheye = false;
			gFisheyeCapture1->setVideoDevice(std::string(argv[i + 1]));
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Single fisheye using device %s!\n", std::string(argv[i + 1]));
		}
		else if (strcmp(argv[i], "-fisheyeoption") == 0 && argc > (i + 2))
		{
			gFisheyeCapture2->addOption(
				std::make_pair(std::string(argv[i + 1]), std::string(argv[i + 2])));
			gFisheyeCapture1->addOption(
				std::make_pair(std::string(argv[i + 1]), std::string(argv[i + 2])));
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Added fisheye option %s, parameter %s\n", std::string(argv[i + 1]), std::string(argv[i + 2]));
		}
		else if (strcmp(argv[i], "-fisheyeflip1") == 0)
		{
			flipFisheye1 = true;
		}
		else if (strcmp(argv[i], "-fisheyeflip2") == 0)
		{
			flipFisheye2 = true;
		}
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
    int w = gCapture->getWidth();
    int h = gCapture->getHeight();

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

GLuint allocateFisheyeCaptureTexture()
{
	int w = gFisheyeCapture1->getWidth();
	int h = gFisheyeCapture1->getHeight();

	if (w * h <= 0)
	{
		sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Invalid texture size (%dx%d)!\n", w, h);
		return 0;
	}
	sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Fisheye texture size (%dx%d)!\n", w, h);

	GLuint texId;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	if (gFisheyeCapture1->isFormatYUYV422()) {
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, w, h);
	}
	else { //Assuming BGR24
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, w, h);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return texId;
}

GLuint allocateFisheyeTwinCaptureTexture()
{
	int w = gFisheyeCapture2->getWidth();
	int h = gFisheyeCapture2->getHeight();
	h *= 2;

	if (w * h <= 0)
	{
		sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Invalid texture size (%dx%d)!\n", w, h);
		return 0;
	}
	sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Fisheye(Twins) texture size (%dx%d)!\n", w, h);

	GLuint texId;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	if (gFisheyeCapture1->isFormatYUYV422()) {
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, w, h);
	}
	else { //Assuming BGR24
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, w, h);
	}
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

    if (captureTexId)
    {
#ifdef ZXING_ENABLED
		// If result is not empty, we have to interpret the message to decide it the plane should lock the capture to the previous frame or update it.
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
			std::vector<ContentPlane> pA = planeAttributes.getVal();
			if (!operationsQueue.empty()) {
				// Now we can process them when decodedResults is empty
				for (size_t i = 0; i < operationsQueue.size(); i++) {
					sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Applying Operation: %s\n", operationsQueue[i].c_str());
					std::vector<std::string> operation = split(operationsQueue[i].c_str(), ';');
					if (operation.size() > 1) {
						int capturePlaneIdx = -1;
						for (int p = 0; p < captureContentPlanes.size(); p++) {
							if (pA[p].name == operation[0]) {
								capturePlaneIdx = p;
								break;
							}
						}
						if (capturePlaneIdx >= 0) {
							for (int o = 1; o < operation.size(); o++) {
								if (operation[o] == "SetActive") {
									// Setting capturePlaneIdx as active capture plane
									pA[capturePlaneIdx].previouslyVisible = false;
									pA[capturePlaneIdx].freeze = false;
									pA[capturePlaneIdx].planeTexId = 0;

									//Freezing other planes which are not already frozen
									for (int p = 0; p < captureContentPlanes.size(); p++) {
										if (p != capturePlaneIdx && !pA[p].freeze) {
											pA[p].freeze = true;
											glCopyImageSubData(captureTexId, GL_TEXTURE_2D, 0, 0, 0, 0, planeTexOwnedIds[p], GL_TEXTURE_2D, 0, 0, 0, 0, width, height, 1);
											glFlush();
										}
									}
									
									// Setting capturePlaneIdx as active capture plane
									pA[capturePlaneIdx].currentlyVisible = true;
								}
							}
						}
						else if (operation[0] == "AllCaptures") {
							if (operation[1] == "Clear") {
								//Making all planes fade out
								for (int p = 0; p < captureContentPlanes.size(); p++) {
									//Need to freeze all planes
									if (!pA[p].freeze) {
										pA[p].freeze = true;
										glCopyImageSubData(captureTexId, GL_TEXTURE_2D, 0, 0, 0, 0, planeTexOwnedIds[p], GL_TEXTURE_2D, 0, 0, 0, 0, width, height, 1);
										glFlush();
									}
									pA[p].currentlyVisible = false;
								}
							}
						}
						else {
							sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Could not find plane named: %s\n", operation[0].c_str());
						}
					}
				}
			}
#endif
			unsigned char * GPU_ptr = reinterpret_cast<unsigned char*>(glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));
			if (GPU_ptr)
			{
				int dataOffset = 0;
				int stride = width * 3; //Assuming BGR24
				if (gFisheyeCapture1->isFormatYUYV422()) {
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
				glBindTexture(GL_TEXTURE_2D, captureTexId);

				if (gCapture->isFormatYUYV422()) {
					//AV_PIX_FMT_YUYV422
					//int y1, u, y2, v;
					glTexImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, 0);
				}
				else { //Assuming BGR24
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, 0);
				}
			}
#ifdef ZXING_ENABLED
			if (!operationsQueue.empty()) {
				planeAttributes.setVal(pA);
				operationsQueue.clear();
			}
		}
#endif

        //calculateStats();
    }
}

void uploadFisheye1CaptureData(uint8_t ** data, int width, int height)
{
	// At least two textures and GLSync objects
	// should be used to control that the uploaded texture is the same
	// for all viewports to prevent any tearing and maintain frame sync

	unsigned char * GPU_ptr = reinterpret_cast<unsigned char*>(glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));
	if (GPU_ptr)
	{
		int dataOffset = 0;
		int stride = width * 3; //Assuming BGR24
		if (gFisheyeCapture1->isFormatYUYV422()) {
			stride = width * 2;
		}

		if (flipFisheye1)
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
		glBindTexture(GL_TEXTURE_2D, fisheyeCaptureTexId);

		if (gFisheyeCapture1->isFormatYUYV422()) {
			//AV_PIX_FMT_YUYV422
			//int y1, u, y2, v;
			glTexImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, 0);
		}
		else { //Assuming BGR24
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, 0);
		}
	}

	//calculateStats();
}

void uploadFisheye2CaptureData(uint8_t ** data, int width, int height)
{
	// At least two textures and GLSync objects
	// should be used to control that the uploaded texture is the same
	// for all viewports to prevent any tearing and maintain frame sync

	unsigned char * GPU_ptr = reinterpret_cast<unsigned char*>(glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));
	if (GPU_ptr)
	{
		int dataOffset = 0;
		int stride = width * 3; //Assuming BGR24
		if (gFisheyeCapture2->isFormatYUYV422()) {
			stride = width * 2;
		}

		if (flipFisheye2)
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
		glBindTexture(GL_TEXTURE_2D, fisheyeCaptureTexId);

		if (gFisheyeCapture2->isFormatYUYV422()) {
			//AV_PIX_FMT_YUYV422
			//int y1, u, y2, v;
			glTexImage2D(GL_TEXTURE_2D, 0, 0, height, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, 0);
		}
		else { //Assuming BGR24
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, height, width, height, GL_BGR, GL_UNSIGNED_BYTE, 0);
		}
	}

	//calculateStats();
}

void captureLoop(void *arg)
{
    glfwMakeContextCurrent(hiddenCaptureWindow);

    int dataSize = gCapture->getWidth() * gCapture->getHeight() * 3;
    GLuint PBO;
    glGenBuffers(1, &PBO);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, 0, GL_DYNAMIC_DRAW);

    while (captureRunning.getVal())
    {
		gCapture->poll();
		sgct::Engine::sleep(0.02); //take a short break to offload the cpu
    }

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    glDeleteBuffers(1, &PBO);

    glfwMakeContextCurrent(NULL); //detach context
}

void fisheyeCaptureLoop1(void *arg)
{
	glfwMakeContextCurrent(hiddenFisheyeCapture1Window);

	int dataSize = gFisheyeCapture1->getWidth() * gFisheyeCapture1->getHeight() * 3;
	GLuint PBO;
	glGenBuffers(1, &PBO);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, 0, GL_DYNAMIC_DRAW);

	while (fisheyeCaptureLoop.getVal())
	{
		gFisheyeCapture1->poll();
		sgct::Engine::sleep(0.02); //take a short break to offload the cpu
	}

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	glDeleteBuffers(1, &PBO);

	glfwMakeContextCurrent(NULL); //detach context
}

void fisheyeCaptureLoop2(void *arg)
{
	glfwMakeContextCurrent(hiddenFisheyeCapture2Window);

	int dataSize = gFisheyeCapture2->getWidth() * gFisheyeCapture2->getHeight() * 3;
	GLuint PBO;
	glGenBuffers(1, &PBO);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, 0, GL_DYNAMIC_DRAW);

	while (fisheyeCaptureLoop.getVal())
	{
		gFisheyeCapture2->poll();
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
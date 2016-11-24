#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <algorithm> //used for transform string to lowercase
#include <sgct.h>
#include "Capture.hpp"

#include <imgui.h>
#include <imgui_impl_glfw_gl3.h>

sgct::Engine * gEngine;
Capture * gCapture = NULL;

//sgct callbacks
void myDraw3DFun();
void myDraw2DFun();
void myPreSyncFun();
void myPostSyncPreDrawFun();
void myInitOGLFun();
void myEncodeFun();
void myDecodeFun();
void myCleanUpFun();
void myKeyCallback(int key, int action);
void myCharCallback(unsigned int c);
void myMouseButtonCallback(int button, int action);
void myMouseScrollCallback(double xoffset, double yoffset);
void myContextCreationCallback(GLFWwindow * win);

sgct_utils::SGCTPlane * plane = NULL;

sgct_utils::SGCTDome * dome = NULL;

GLFWwindow * hiddenWindow;
GLFWwindow * sharedWindow;

//variables to share across cluster
sgct::SharedDouble curr_time(0.0);
sgct::SharedBool info(false);
sgct::SharedBool stats(false);
sgct::SharedBool takeScreenshot(false);
sgct::SharedBool wireframe(false);

//DomeImageViewer
void myDropCallback(int count, const char** paths);
void myDataTransferDecoder(void * receivedData, int receivedlength, int packageId, int clientIndex);
void myDataTransferStatus(bool connected, int clientIndex);
void myDataTransferAcknowledge(int packageId, int clientIndex);
void startDataTransfer();
void readImage(unsigned char * data, int len);
void uploadTexture();
void threadWorker(void *arg);

tthread::thread * loadThread;
tthread::mutex mutex;
std::vector<sgct_core::Image *> transImages;

sgct::SharedInt32 texIndex(-1);
sgct::SharedInt32 incrIndex(1);
sgct::SharedInt32 numSyncedTex(0);

sgct::SharedBool running(true);
sgct::SharedInt32 lastPackage(-1);
sgct::SharedBool transfer(false);
sgct::SharedBool serverUploadDone(false);
sgct::SharedInt32 serverUploadCount(0);
sgct::SharedBool clientsUploadDone(false);
sgct::SharedVector<std::pair<std::string, int>> imagePaths;
sgct::SharedVector<GLuint> texIds;
double sendTimer = 0.0;

enum imageType { IM_JPEG, IM_PNG };
const int headerSize = 1;

//FFmpegCapture
void uploadData(uint8_t ** data, int width, int height);
void parseArguments(int& argc, char**& argv);
void allocateTexture();
void captureLoop(void *arg);
void calculateStats();
void startCapture();
void stopCapture();
void createPlane();

GLint Matrix_Loc = -1;
GLint ScaleUV_Loc = -1;
GLint OffsetUV_Loc = -1;
GLint Matrix_Loc_CK = -1;
GLint ScaleUV_Loc_CK = -1;
GLint OffsetUV_Loc_CK = -1;
GLint ChromaKeyColor_Loc_CK = -1;
GLint ChromaKeyCutOff_Loc_CK = -1;
GLint ChromaKeySensitivity_Loc_CK = -1;
GLint ChromaKeySmoothing_Loc_CK = -1;
GLuint texId = GL_FALSE;

tthread::thread * captureThread;
bool flipFrame = false;
bool fulldomeMode = false;
bool recreatePlane = false;
glm::vec2 planeScaling(1.0f, 1.0f);
glm::vec2 planeOffset(0.0f, 0.0f);

sgct::SharedBool captureRunning(true);
sgct::SharedBool renderDome(fulldomeMode);
sgct::SharedDouble captureRate(0.0);
sgct::SharedInt32 domeCut(2);
sgct::SharedInt planeScreenAspect(1610);
sgct::SharedInt planeMaterialAspect(169);
sgct::SharedBool planeUseCaptureSize(false);
sgct::SharedFloat planeWidth(8.0f);
sgct::SharedFloat planeAzimuth(0.0f);
sgct::SharedFloat planeElevation(33.0f);
sgct::SharedFloat planeRoll(0.0f);
sgct::SharedFloat planeDistance(-5.0f);
sgct::SharedBool chromaKey(false);
sgct::SharedObject<glm::vec3> chromaKeyColor(glm::vec3(0.f, 177.f, 64.f));
sgct::SharedFloat chromaKeyCutOff(0.1f);
sgct::SharedFloat chromaKeySensitivity(0.0f);
sgct::SharedFloat chromaKeySmoothing(0.0f);

//ImGUI variables
int imPlaneScreenAspect = 1610;
int imPlaneMaterialAspect = 169;
bool imPlaneUseCaptureSize = false;
float imPlaneWidth = 8.0f;
float imPlaneAzimuth = 0.0f;
float imPlaneElevation = 33.0f;
float imPlaneRoll = 0.0f;
float imPlaneDistance = -5.0f;
bool imChromaKey = false;
ImVec4 imChromaKeyColor = ImColor(0, 219, 0);
float imChromaKeyCutOff = 0.1f;
float imChromaKeySensitivity = 0.0f;
float imChromaKeySmoothing = 0.0f;

int main( int argc, char* argv[] )
{
    //sgct::MessageHandler::instance()->setNotifyLevel(sgct::MessageHandler::NOTIFY_ALL);
    
    gEngine = new sgct::Engine( argc, argv );
    gCapture = new Capture();

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
    gEngine->setDrawFunction( myDraw3DFun );
    gEngine->setDraw2DFunction( myDraw2DFun );
    gEngine->setPreSyncFunction( myPreSyncFun );
    gEngine->setPostSyncPreDrawFunction(myPostSyncPreDrawFun);
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

	stopCapture();

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
    delete gEngine;

    // Exit program
    exit( EXIT_SUCCESS );
}

void myDraw3DFun()
{
	if (recreatePlane)
		createPlane();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    glm::mat4 MVP = gEngine->getCurrentModelViewProjectionMatrix();

    //Set up backface culling
    glCullFace(GL_BACK);

    if (texIndex.getVal() != -1)
    {
        sgct::ShaderManager::instance()->bindShaderProgram("xform");
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texIds.getValAt(texIndex.getVal()));
        glUniform2f(ScaleUV_Loc, 1.f, 1.f);
        glUniform2f(OffsetUV_Loc, 0.f, 0.f);
        glUniformMatrix4fv(Matrix_Loc, 1, GL_FALSE, &MVP[0][0]);

        glFrontFace(GL_CW);

        dome->draw();
        sgct::ShaderManager::instance()->unBindShaderProgram();

    }

    GLint ScaleUV_L = ScaleUV_Loc;
    GLint OffsetUV_L = OffsetUV_Loc;
    GLint Matrix_L = Matrix_Loc;
    if (chromaKey.getVal())
    {
        sgct::ShaderManager::instance()->bindShaderProgram("chromakey");
        glUniform3f(ChromaKeyColor_Loc_CK
            , chromaKeyColor.getVal().r
            , chromaKeyColor.getVal().g
            , chromaKeyColor.getVal().b);
		glUniform1f(ChromaKeyCutOff_Loc_CK, chromaKeyCutOff.getVal());
		glUniform1f(ChromaKeySensitivity_Loc_CK, chromaKeySensitivity.getVal());
		glUniform1f(ChromaKeySmoothing_Loc_CK, chromaKeySmoothing.getVal());
        ScaleUV_L = ScaleUV_Loc_CK;
        OffsetUV_L = OffsetUV_Loc_CK;
        Matrix_L = Matrix_Loc_CK;
    }
    else
    {
        sgct::ShaderManager::instance()->bindShaderProgram("xform");
    }

    glFrontFace(GL_CCW);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texId);

    glm::vec2 texSize = glm::vec2(static_cast<float>(gCapture->getWidth()), static_cast<float>(gCapture->getHeight()));

    if (fulldomeMode)
    {
        // TextureCut 2 equals showing only the middle square of a capturing a widescreen input
        if (domeCut.getVal() == 2){
            glUniform2f(ScaleUV_L, texSize.y / texSize.x, 1.f);
            glUniform2f(OffsetUV_L, ((texSize.x - texSize.y)*0.5f) / texSize.x, 0.f);
        }
        else{
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

        //transform and draw plane
        glm::mat4 planeTransform = glm::mat4(1.0f);
        planeTransform = glm::rotate(planeTransform, glm::radians(planeAzimuth.getVal()), glm::vec3(0.0f, -1.0f, 0.0f)); //azimuth
        planeTransform = glm::rotate(planeTransform, glm::radians(planeElevation.getVal()), glm::vec3(1.0f, 0.0f, 0.0f)); //elevation
        planeTransform = glm::rotate(planeTransform, glm::radians(planeRoll.getVal()), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
        planeTransform = glm::translate(planeTransform, glm::vec3(0.0f, 0.0f, planeDistance.getVal())); //distance

        planeTransform = MVP * planeTransform;
        glUniformMatrix4fv(Matrix_L, 1, GL_FALSE, &planeTransform[0][0]);

		plane->draw();
    }

    sgct::ShaderManager::instance()->unBindShaderProgram();

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}

void myDraw2DFun()
{
    if (info.getVal())
    {
        unsigned int font_size = static_cast<unsigned int>(9.0f*gEngine->getCurrentWindowPtr()->getXScale());
        const sgct_text::Font * font = sgct_text::FontManager::instance()->getFont("SGCTFont", font_size);
        float padding = 10.0f;

        sgct_text::print(font,
            padding, static_cast<float>(gEngine->getCurrentWindowPtr()->getYFramebufferResolution() - font_size) - padding, //x and y pos
            glm::vec4(1.0, 1.0, 1.0, 1.0), //color
            "Format: %s\nResolution: %d x %d\nRate: %.2lf Hz",
            gCapture->getFormat(),
            gCapture->getWidth(),
            gCapture->getHeight(),
            captureRate.getVal());
    }

	if (gEngine->isMaster())
	{
		ImGui_ImplGlfwGL3_NewFrame();

		ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Settings");
		if (!imPlaneUseCaptureSize) {
			ImGui::RadioButton("16:10", &imPlaneScreenAspect, 1610); ImGui::SameLine();
			ImGui::RadioButton("16:9", &imPlaneScreenAspect, 169); ImGui::SameLine();
			ImGui::RadioButton("5:4", &imPlaneScreenAspect, 54); ImGui::SameLine();
			ImGui::RadioButton("4:3", &imPlaneScreenAspect, 43); ImGui::SameLine();
			ImGui::Text("  -  Screen Aspect Ratio");
			ImGui::RadioButton("16;10", &imPlaneMaterialAspect, 1610); ImGui::SameLine();
			ImGui::RadioButton("16;9", &imPlaneMaterialAspect, 169); ImGui::SameLine();
			ImGui::RadioButton("5;4", &imPlaneMaterialAspect, 54); ImGui::SameLine();
			ImGui::RadioButton("4;3", &imPlaneMaterialAspect, 43); ImGui::SameLine();
			ImGui::Text("  -  Material Aspect Ratio");
		}
		ImGui::Checkbox("Use Capture Size For Aspect Ratio", &imPlaneUseCaptureSize);
		ImGui::SliderFloat("Plane Width", &imPlaneWidth, 2.0f, 12.0f);
		ImGui::SliderFloat("Plane Azimuth", &imPlaneAzimuth, -180.f, 180.f);
		ImGui::SliderFloat("Plane Elevation", &imPlaneElevation, -180.f, 180.f);
		ImGui::SliderFloat("Plane Roll", &imPlaneRoll, -180.f, 180.f);
		ImGui::Checkbox("Chroma Key On/Off", &imChromaKey);
		ImGui::ColorEdit3("Chroma Key Color", (float*)&imChromaKeyColor);
		ImGui::SliderFloat("Chroma Key CutOff", &imChromaKeyCutOff, 0.f, 0.5f);
		ImGui::SliderFloat("Chroma Key Sensitivity", &imChromaKeySensitivity, 0.f, 0.05f);
		ImGui::SliderFloat("Chroma Key Smoothing", &imChromaKeySmoothing, 0.f, 0.05f);
		//ImGui::SliderFloat("Plane Distance", &imPlaneDistance, -50.f, 0.0f);
		ImGui::End();

		ImGui::Render();
	}
}

void myPreSyncFun()
{
    if( gEngine->isMaster() )
    {
        curr_time.setVal( sgct::Engine::getTime() );
        
        //if texture is uploaded then iterate the index
        if (serverUploadDone.getVal() && clientsUploadDone.getVal())
        {
            numSyncedTex = static_cast<int32_t>(texIds.getSize());
            
            //only iterate up to the first new image, even if multiple images was added
            texIndex = numSyncedTex - serverUploadCount.getVal();

            serverUploadDone = false;
            clientsUploadDone = false;
        }
    }
}

void myPostSyncPreDrawFun()
{
    gEngine->setDisplayInfoVisibility(info.getVal());
    gEngine->setStatsGraphVisibility(stats.getVal());
    //gEngine->setWireframe(wireframe.getVal());

    if (takeScreenshot.getVal())
    {
        gEngine->takeScreenshot();
        takeScreenshot.setVal(false);
    }

    fulldomeMode = renderDome.getVal(); //set the flag frame synchronized for all viewports
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

void createPlane() {
	if (plane != NULL)
		delete plane;

	float planeHeight = planeWidth.getVal() * (static_cast<float>(gCapture->getHeight()) / static_cast<float>(gCapture->getWidth()));
	if (planeUseCaptureSize.getVal())
	{
		plane = new sgct_utils::SGCTPlane(planeWidth.getVal(), planeHeight);
	}
	else
	{
		switch (planeMaterialAspect.getVal())
		{
		case 1610:
			plane = new sgct_utils::SGCTPlane(planeWidth.getVal(), (planeWidth.getVal() / 16.0f) * 10.0f);
			break;
		case 169:
			plane = new sgct_utils::SGCTPlane(planeWidth.getVal(), (planeWidth.getVal() / 16.0f) * 9.0f);
			break;
		case 54:
			plane = new sgct_utils::SGCTPlane(planeWidth.getVal(), (planeWidth.getVal() / 5.0f) * 4.0f);
			break;
		case 43:
			plane = new sgct_utils::SGCTPlane(planeWidth.getVal(), (planeWidth.getVal() / 4.0f) * 3.0f);
			break;
		default:
			plane = new sgct_utils::SGCTPlane(planeWidth.getVal(), planeHeight);
			break;
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
				planeOffset = glm::vec2(0.0f, 0.05f);
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
				planeOffset = glm::vec2(0.0f, 0.0f);
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
				planeOffset = glm::vec2(0.0f, 57.0f / 384.0f);
				break;
			case 43:
				//16:9 -> 4:3
				planeScaling = glm::vec2(1.0f, 0.75f);
				planeOffset = glm::vec2(0.0f, 0.125f);
				break;
			case 169:
			default:
				planeScaling = glm::vec2(1.0f, 1.0f);
				planeOffset = glm::vec2(0.0f, 0.0f);
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
				planeOffset = glm::vec2(0.0f, 0.03125f);
				break;
			case 54:
			default:
				planeScaling = glm::vec2(1.0f, 1.0f);
				planeOffset = glm::vec2(0.0f, 0.0f);
				break;
			}
			break;
		case 43:
			switch (planeMaterialAspect.getVal())
			{
			case 1610:
				//4:3 -> 16:10
				planeScaling = glm::vec2(1.0f, 50.0f / 64.0f);
				planeOffset = glm::vec2(0.0f, 7.0f / 64.0f);
				break;
			case 169:
				//4:3 -> 16:9
				planeScaling = glm::vec2(1.0f, 0.75f);
				planeOffset = glm::vec2(0.0f, 0.125f);
				break;
			case 54:
				//4:3 -> 5:4
				planeScaling = glm::vec2(0.9375f, 1.0f);
				planeOffset = glm::vec2(0.03125f, 0.0f);
				break;
			case 43:
			default:
				planeScaling = glm::vec2(1.0f, 1.0f);
				planeOffset = glm::vec2(0.0f, 0.0f);
				break;
			}
			break;
		default:
			planeScaling = glm::vec2(1.0f, 1.0f);
			planeOffset = glm::vec2(0.0f, 0.0f);
			break;
		}
	}

	recreatePlane = false;
}

void myInitOGLFun()
{
    gCapture->init();

    //allocate texture
    allocateTexture();

    //start capture
	startCapture();

	//start load thread
    if (gEngine->isMaster())
        loadThread = new (std::nothrow) tthread::thread(threadWorker, NULL);

    std::function<void(uint8_t ** data, int width, int height)> callback = uploadData;
    gCapture->setVideoDecoderCallback(callback);

    //create plane
	createPlane();

    //create dome
    dome = new sgct_utils::SGCTDome(7.4f, 180.0f, 256, 128);

    sgct::ShaderManager::instance()->addShaderProgram( "xform",
            "xform.vert",
            "xform.frag" );

    sgct::ShaderManager::instance()->bindShaderProgram( "xform" );

    Matrix_Loc = sgct::ShaderManager::instance()->getShaderProgram( "xform").getUniformLocation( "MVP" );
    ScaleUV_Loc = sgct::ShaderManager::instance()->getShaderProgram( "xform").getUniformLocation("scaleUV");
    OffsetUV_Loc = sgct::ShaderManager::instance()->getShaderProgram( "xform").getUniformLocation("offsetUV");
    GLint Tex_Loc = sgct::ShaderManager::instance()->getShaderProgram( "xform").getUniformLocation( "Tex" );
    glUniform1i( Tex_Loc, 0 );

    sgct::ShaderManager::instance()->unBindShaderProgram();

    sgct::ShaderManager::instance()->addShaderProgram("chromakey",
        "xform.vert",
        "chromakey.frag");

    sgct::ShaderManager::instance()->bindShaderProgram("chromakey");

    Matrix_Loc_CK = sgct::ShaderManager::instance()->getShaderProgram("chromakey").getUniformLocation("MVP");
    ScaleUV_Loc_CK = sgct::ShaderManager::instance()->getShaderProgram("chromakey").getUniformLocation("scaleUV");
    OffsetUV_Loc_CK = sgct::ShaderManager::instance()->getShaderProgram("chromakey").getUniformLocation("offsetUV");
    ChromaKeyColor_Loc_CK = sgct::ShaderManager::instance()->getShaderProgram("chromakey").getUniformLocation("chromaKeyColor");
	ChromaKeyCutOff_Loc_CK = sgct::ShaderManager::instance()->getShaderProgram("chromakey").getUniformLocation("chromaKeyCutOff");
	ChromaKeySensitivity_Loc_CK = sgct::ShaderManager::instance()->getShaderProgram("chromakey").getUniformLocation("chromaKeyThresholdSensitivity");
	ChromaKeySmoothing_Loc_CK = sgct::ShaderManager::instance()->getShaderProgram("chromakey").getUniformLocation("chromaKeySmoothing");
    GLint Tex_Loc_CK = sgct::ShaderManager::instance()->getShaderProgram("chromakey").getUniformLocation("Tex");
    glUniform1i(Tex_Loc_CK, 0);

    sgct::ShaderManager::instance()->unBindShaderProgram();

	// Setup ImGui binding
	if (gEngine->isMaster())
		ImGui_ImplGlfwGL3_Init(gEngine->getCurrentWindowPtr()->getWindowHandle());

    sgct::Engine::checkForOGLErrors();
}

void myEncodeFun()
{
    sgct::SharedData::instance()->writeDouble(&curr_time);
    sgct::SharedData::instance()->writeBool(&info);
    sgct::SharedData::instance()->writeBool(&stats);
    sgct::SharedData::instance()->writeBool(&wireframe);
    sgct::SharedData::instance()->writeInt32(&texIndex);
    sgct::SharedData::instance()->writeInt32(&incrIndex);
    sgct::SharedData::instance()->writeBool(&takeScreenshot);
    sgct::SharedData::instance()->writeBool(&renderDome);
    sgct::SharedData::instance()->writeInt32(&domeCut);

	if (planeScreenAspect.getVal() != imPlaneScreenAspect) recreatePlane = true;
	planeScreenAspect.setVal(imPlaneScreenAspect);
	sgct::SharedData::instance()->writeInt32(&planeScreenAspect);
	if (planeMaterialAspect.getVal() != imPlaneMaterialAspect) recreatePlane = true;
	planeMaterialAspect.setVal(imPlaneMaterialAspect);
	sgct::SharedData::instance()->writeInt32(&planeMaterialAspect);
	if (planeUseCaptureSize.getVal() != imPlaneUseCaptureSize) recreatePlane = true;
	planeUseCaptureSize.setVal(imPlaneUseCaptureSize);
	sgct::SharedData::instance()->writeBool(&planeUseCaptureSize);
	if (planeWidth.getVal() != imPlaneWidth) recreatePlane = true;
	planeWidth.setVal(imPlaneWidth);
	sgct::SharedData::instance()->writeFloat(&planeWidth);

	planeAzimuth.setVal(imPlaneAzimuth);
	sgct::SharedData::instance()->writeFloat(&planeAzimuth);
	planeElevation.setVal(imPlaneElevation);
	sgct::SharedData::instance()->writeFloat(&planeElevation);
	planeRoll.setVal(imPlaneRoll);
	sgct::SharedData::instance()->writeFloat(&planeRoll);
	planeDistance.setVal(imPlaneDistance);
	sgct::SharedData::instance()->writeFloat(&planeDistance);

	chromaKey.setVal(imChromaKey);
	sgct::SharedData::instance()->writeBool(&chromaKey);
	chromaKeyColor.setVal(glm::vec3(imChromaKeyColor.x, imChromaKeyColor.y, imChromaKeyColor.z));
	sgct::SharedData::instance()->writeObj(&chromaKeyColor);
	chromaKeyCutOff.setVal(imChromaKeyCutOff);
	sgct::SharedData::instance()->writeFloat(&chromaKeyCutOff);
	chromaKeySensitivity.setVal(imChromaKeySensitivity);
	sgct::SharedData::instance()->writeFloat(&chromaKeySensitivity);
	chromaKeySmoothing.setVal(imChromaKeySmoothing);
	sgct::SharedData::instance()->writeFloat(&chromaKeySmoothing);
}

void myDecodeFun()
{
    sgct::SharedData::instance()->readDouble(&curr_time);
    sgct::SharedData::instance()->readBool(&info);
    sgct::SharedData::instance()->readBool(&stats);
    sgct::SharedData::instance()->readBool(&wireframe);
    sgct::SharedData::instance()->readInt32(&texIndex);
    sgct::SharedData::instance()->readInt32(&incrIndex);
    sgct::SharedData::instance()->readBool(&takeScreenshot);
    sgct::SharedData::instance()->readBool(&renderDome);
    sgct::SharedData::instance()->readInt32(&domeCut);

	sgct::SharedData::instance()->readInt32(&planeScreenAspect);
	if (planeScreenAspect.getVal() != imPlaneScreenAspect) recreatePlane = true;
	imPlaneScreenAspect = planeScreenAspect.getVal();
	sgct::SharedData::instance()->readInt32(&planeMaterialAspect);
	if (planeMaterialAspect.getVal() != imPlaneMaterialAspect) recreatePlane = true;
	imPlaneMaterialAspect = planeMaterialAspect.getVal();
	sgct::SharedData::instance()->readBool(&planeUseCaptureSize);
	if (planeUseCaptureSize.getVal() != imPlaneUseCaptureSize) recreatePlane = true;
	imPlaneUseCaptureSize = planeUseCaptureSize.getVal();
	sgct::SharedData::instance()->readFloat(&planeWidth);
	if (planeWidth.getVal() != imPlaneWidth) recreatePlane = true;
	imPlaneWidth = planeWidth.getVal();

	sgct::SharedData::instance()->readFloat(&planeAzimuth);
	imPlaneAzimuth = planeAzimuth.getVal();
	sgct::SharedData::instance()->readFloat(&planeElevation);
	imPlaneElevation = planeElevation.getVal();
	sgct::SharedData::instance()->readFloat(&planeRoll);
	imPlaneRoll = planeRoll.getVal();
	sgct::SharedData::instance()->readFloat(&planeDistance);
	imPlaneDistance = planeDistance.getVal();

	sgct::SharedData::instance()->readBool(&chromaKey);
	imChromaKey = chromaKey.getVal();
	sgct::SharedData::instance()->readObj(&chromaKeyColor);
	imChromaKeyColor.x = chromaKeyColor.getVal().x;
	imChromaKeyColor.y = chromaKeyColor.getVal().y;
	imChromaKeyColor.z = chromaKeyColor.getVal().z;
	sgct::SharedData::instance()->readFloat(&chromaKeyCutOff);
	imChromaKeyCutOff = chromaKeyCutOff.getVal();
	sgct::SharedData::instance()->readFloat(&chromaKeySensitivity);
	imChromaKeySensitivity = chromaKeySensitivity.getVal();
	sgct::SharedData::instance()->readFloat(&chromaKeySmoothing);
	imChromaKeySmoothing = chromaKeySmoothing.getVal();
	
}

void myCleanUpFun()
{
    if (dome != NULL)
        delete dome;

    if (plane != NULL)
        delete plane;

    if (texId)
    {
        glDeleteTextures(1, &texId);
        texId = GL_FALSE;
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
    
    
    if(hiddenWindow)
        glfwDestroyWindow(hiddenWindow);
}

void myKeyCallback(int key, int action)
{
    if (gEngine->isMaster())
    {
		switch (key)
		{
		case SGCT_KEY_D:
			if (action == SGCT_PRESS)
				renderDome.setVal(true);
			break;
		case SGCT_KEY_S:
			if (action == SGCT_PRESS)
				stats.toggle();
			break;

		case SGCT_KEY_I:
			if (action == SGCT_PRESS)
				info.toggle();
			break;

		case SGCT_KEY_W:
			if (action == SGCT_PRESS)
				wireframe.toggle();
			break;

		case SGCT_KEY_F:
			if (action == SGCT_PRESS)
				wireframe.toggle();
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
				renderDome.setVal(false);
			break;

		case SGCT_KEY_LEFT:
			if (action == SGCT_PRESS && numSyncedTex.getVal() > 0)
			{
				texIndex.getVal() > incrIndex.getVal() - 1 ? texIndex -= incrIndex.getVal() : texIndex.setVal(numSyncedTex.getVal() - 1);
				//fprintf(stderr, "Index set to: %d\n", texIndex.getVal());
			}
			break;

		case SGCT_KEY_RIGHT:
			if (action == SGCT_PRESS && numSyncedTex.getVal() > 0)
			{
				texIndex.setVal((texIndex.getVal() + incrIndex.getVal()) % numSyncedTex.getVal());
				//fprintf(stderr, "Index set to: %d\n", texIndex.getVal());
			}
			break;
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
    hiddenWindow = glfwCreateWindow( 1, 1, "Thread Window", NULL, sharedWindow );
     
    if( !hiddenWindow )
    {
        sgct::MessageHandler::instance()->print("Failed to create loader context!\n");
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
	if (captureStarted)
		stopCapture();
    
    //read the image on slave
    readImage( reinterpret_cast<unsigned char*>(receivedData), receivedlength);
    uploadTexture();

	//start capture
	if (captureStarted)
		startCapture();
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
            startDataTransfer();
            transfer.setVal(false);
            
            //load textures on master
            uploadTexture();
            serverUploadDone = true;
            
            if(sgct_core::ClusterManager::instance()->getNumberOfNodes() == 1) //no cluster
            {
                clientsUploadDone = true;
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
    if(static_cast<int>(imagePaths.getSize()) > id)
    {
        sendTimer = sgct::Engine::getTime();

        int imageCounter = static_cast<int32_t>(imagePaths.getSize());
        lastPackage.setVal(imageCounter - 1);

        for (int i = id; i < imageCounter; i++)
        {
            //load from file
            std::pair<std::string, int> tmpPair = imagePaths.getValAt(static_cast<std::size_t>(i));

            std::ifstream file(tmpPair.first.c_str(), std::ios::binary);
            file.seekg(0, std::ios::end);
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<char> buffer(size + headerSize);
            char type = tmpPair.second;

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
    else
        transImages.push_back(img);

    mutex.unlock();
}

void uploadTexture()
{
    mutex.lock();

    if (!transImages.empty())
    {
        glfwMakeContextCurrent(hiddenWindow);

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

                delete transImages[i];
                transImages[i] = NULL;
            }
            else //if invalid load
            {
                texIds.addVal(GL_FALSE);
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
            std::size_t found;

            std::string tmpStr = pathStrings[i];

            //find file type
            found = tmpStr.find(".jpg");
            if (found != std::string::npos)
            {
                imagePaths.addVal(std::pair<std::string, int>(pathStrings[i], IM_JPEG));
                transfer.setVal(true); //tell transfer thread to start processing data
                serverUploadCount++;
            }
            else
            {
                found = tmpStr.find(".jpeg");
                if (found != std::string::npos)
                {
                    imagePaths.addVal(std::pair<std::string, int>(pathStrings[i], IM_JPEG));
                    transfer.setVal(true); //tell transfer thread to start processing data
                    serverUploadCount++;
                }
                else
                {
                    found = tmpStr.find(".png");
                    if (found != std::string::npos)
                    {
                        imagePaths.addVal(std::pair<std::string, int>(pathStrings[i], IM_PNG));
                        transfer.setVal(true); //tell transfer thread to start processing data
                        serverUploadCount++;
                    }
                }
            }
        }
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
        }
        else if (strcmp(argv[i], "-video") == 0 && argc > (i + 1))
        {
            gCapture->setVideoDevice(std::string(argv[i + 1]));
        }
        else if (strcmp(argv[i], "-option") == 0 && argc > (i + 2))
        {
            gCapture->addOption(
                std::make_pair(std::string(argv[i + 1]), std::string(argv[i + 2])));
        }
        else if (strcmp(argv[i], "-flip") == 0)
        {
            flipFrame = true;
        }
        else if (strcmp(argv[i], "-plane") == 0 && argc > (i + 3))
        {
            planeAzimuth = static_cast<float>(atof(argv[i + 1]));
            planeElevation = static_cast<float>(atof(argv[i + 2]));
            planeRoll = static_cast<float>(atof(argv[i + 3]));
        }

        i++; //iterate
    }
}

void allocateTexture()
{
    int w = gCapture->getWidth();
    int h = gCapture->getHeight();

    if (w * h <= 0)
    {
        sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Invalid texture size (%dx%d)!\n", w, h);
        return;
    }

    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, w, h);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void uploadData(uint8_t ** data, int width, int height)
{
    // At least two textures and GLSync objects
    // should be used to control that the uploaded texture is the same
    // for all viewports to prevent any tearing and maintain frame sync

    if (texId)
    {
        unsigned char * GPU_ptr = reinterpret_cast<unsigned char*>(glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));
        if (GPU_ptr)
        {
            int dataOffset = 0;
            int stride = width * 3;

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
            glBindTexture(GL_TEXTURE_2D, texId);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, 0);
        }

        calculateStats();
    }
}

void captureLoop(void *arg)
{
    glfwMakeContextCurrent(hiddenWindow);

    int dataSize = gCapture->getWidth() * gCapture->getHeight() * 3;
    GLuint PBO;
    glGenBuffers(1, &PBO);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, 0, GL_DYNAMIC_DRAW);

    while (captureRunning.getVal())
    {
		gCapture->poll();
		sgct::Engine::sleep(0.02); //take a short break to offload the cpu

		// We are not sending the capture to the nodes.
		/*if (gEngine->isMaster() && transfer.getVal() && !serverUploadDone.getVal() && !clientsUploadDone.getVal())
		{
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

			startDataTransfer();
			transfer.setVal(false);

			//load textures on master
			uploadTexture();
			serverUploadDone = true;

			if (sgct_core::ClusterManager::instance()->getNumberOfNodes() == 1) //no cluster
			{
			clientsUploadDone = true;
			}

			sgct::Engine::sleep(0.1); //ten iteration per second

			//restore for capture
			glfwMakeContextCurrent(hiddenWindow);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
		}*/
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
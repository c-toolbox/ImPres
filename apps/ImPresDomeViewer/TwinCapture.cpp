#include "TwinCapture.hpp"
#include <sgct.h>

extern "C"
{
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
}

#define USE_REF_COUNTER 1
//if any known visual studio platform
#if (_MSC_VER >= 1100) //visual studio 5 or later
#undef av_err2str
#define av_err2str(errnum) \
	av_make_error_string(reinterpret_cast<char*>(_alloca(AV_ERROR_MAX_STRING_SIZE)), \
	AV_ERROR_MAX_STRING_SIZE, errnum)
#endif

TwinCapture::TwinCapture()
{
	for (int i = 0; i < 2; ++i) {
		mFMTContext[i] = nullptr;
		mVideoDevice[i] = "";
		mVideoStream[i] = nullptr;
		mVideoCodecContext[i] = nullptr;
	}
	mOptions = nullptr;
	mVideoScaleContext = nullptr;
	mFrame = nullptr;
	mTempFrame = nullptr;
	mVideoDecoderCallback = nullptr;
	mVideoHost = "";

	mWidth = 0;
	mHeight = 0;

	mWidth = 0;
	mHeight = 0;

	mVideo_stream_idx = -1;
	mDecodedVideoFrames = 0;

	mDstPixFmt = AV_PIX_FMT_BGR24;
	mInited = false;
}

TwinCapture::~TwinCapture()
{
	cleanup();
}

std::string TwinCapture::getVideoHost() const
{
    return mVideoHost;
}

int TwinCapture::getWidth() const
{
	return mWidth;
}

int TwinCapture::getHeight() const
{
	return mHeight;
}

const char * TwinCapture::getFormat() const
{
	return mVideoStrFormat.c_str();
}

std::size_t TwinCapture::getNumberOfDecodedFrames() const
{
	return mDecodedVideoFrames;
}

bool TwinCapture::init()
{
	for (int i = 0; i < 2; ++i) {
		if (mVideoDevice[i].empty())
		{
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "TwinCapture : No video devices specified!\n");
			cleanup();
			return false;
		}
	}

	initFFmpeg();
	setupOptions();

	// --------------------------------------------------
	//check out https://ffmpeg.org/ffmpeg-devices.html
	// --------------------------------------------------
	AVInputFormat * iformat;
	std::string inputName;

	for (int i = 0; i < 2; ++i) {
#ifdef __WIN32__
		iformat = av_find_input_format("dshow");
		inputName = "video=" + mVideoDevice[i];
#elif defined __APPLE__
		iformat = av_find_input_format("avfoundation");
		inputName = mVideoDevice;
#else  //linux NOT-Tested
		iformat = av_find_input_format("video4linux2");
		inputName = mVideoDevice;
#endif

		if (avformat_open_input(&mFMTContext[i], inputName.c_str(), iformat, &mOptions) < 0)
		{
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "TwinCapture : Could not open capture input!\n");
			cleanup();
			return false;
		}

		/* retrieve stream information */
		if (avformat_find_stream_info(mFMTContext[i], nullptr) < 0)
		{
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "TwinCapture : Could not find stream information!\n");
			cleanup();
			return false;
		}

		if (!initVideoStream(i))
		{
			cleanup();
			return false;
		}

		//dump format info to console
		av_dump_format(mFMTContext[i], 0, inputName.c_str(), 0);

		if (mVideoCodecContext[i])
		{
			if (!allocateVideoDecoderData(mVideoCodecContext[i]->pix_fmt))
			{
				cleanup();
				return false;
			}
		}
	}

	/* initialize packet, set data to nullptr, let the demuxer fill it */
	av_init_packet(&mPkt);
	mPkt.data = nullptr;
	mPkt.size = 0;

	//success
	mInited = true;

    sgct::MessageHandler::instance()->print("TwinCapture init complete\n");

	return true;
}

void TwinCapture::setVideoHost(std::string hostAdress)
{
    mVideoHost = hostAdress;
}

void TwinCapture::setVideoDevices(std::string videoDeviceName1, std::string videoDeviceName2)
{
	mVideoDevice[0] = videoDeviceName1;
	mVideoDevice[1] = videoDeviceName2;
}

void TwinCapture::setVideoDecoderCallback(std::function<void(uint8_t ** data, int width, int height, int idx)> cb)
{
	mVideoDecoderCallback = cb;
}

void TwinCapture::addOption(std::pair<std::string, std::string> option)
{
	mUserOptions.push_back(option);
}

bool TwinCapture::poll()
{
	if (!mInited)
		return false;
	
	int ret;
	int gotVideoFrame;

	bool all_ok = true;

	for (int i = 0; i < 2; ++i) {
		if (av_read_frame(mFMTContext[i], &mPkt) >= 0)
		{
			AVPacket orig_pkt = mPkt;

			do
			{
				ret = decodePacket(&gotVideoFrame, i);
				if (ret < 0)
				{
					sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to decode package!\n");
					all_ok = false;
					break;
				}

				mPkt.data += ret;
				mPkt.size -= ret;

			} while (mPkt.size > 0);

			av_free_packet(&orig_pkt);
		}
	}

	return all_ok;
}

void TwinCapture::setupOptions()
{
	av_dict_set(&mOptions, "fflags", "nobuffer", 0); //reduce latency
	av_dict_set(&mOptions, "fflags", "flush_packets", 0); //reduce the latency by flushing out packets immediately

	//set user options
	for (std::size_t i = 0; i < mUserOptions.size(); i++)
	{
		if (!mUserOptions.at(i).first.empty() && !mUserOptions.at(i).second.empty())
		{
			av_dict_set(&mOptions, mUserOptions.at(i).first.c_str(), mUserOptions.at(i).second.c_str(), 0);
		}
	}
}

void TwinCapture::initFFmpeg()
{
	//set log level
	//av_log_set_level(AV_LOG_INFO);
	av_log_set_level(AV_LOG_QUIET);

	/* register all formats and mCodecs */
	av_register_all();
	avdevice_register_all();
	//avformat_network_init();
}

bool TwinCapture::initVideoStream(int idx)
{
	//open video stream
	if (openCodeContext(mFMTContext[idx], AVMEDIA_TYPE_VIDEO, mVideo_stream_idx) >= 0)
	{
		mVideoStream[idx] = mFMTContext[idx]->streams[mVideo_stream_idx];
	}
	else
		return false;

	mVideoCodecContext[idx] = mVideoStream[idx]->codec;

	if (idx == 0) {
		AVCodecID codecId = mVideoCodecContext[idx]->codec_id;

		mWidth = mVideoCodecContext[idx]->width;
		mHeight = mVideoCodecContext[idx]->height;
	}

	return true;
}

int TwinCapture::openCodeContext(AVFormatContext *fmt_ctx, enum AVMediaType type, int & streamIndex)
{
	int ret;
	AVStream *st;
	AVCodecContext *dec_ctx = nullptr;
	AVCodec *dec = nullptr;
	AVDictionary *opts = nullptr;

	ret = av_find_best_stream(fmt_ctx, type, -1, -1, nullptr, 0);
	if (ret < 0)
	{
		sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Could not find %s stream!\n", av_get_media_type_string(type));
		return ret;
	}
	else
	{
		streamIndex = ret;
		st = fmt_ctx->streams[streamIndex];

		/* find decoder for the stream */
		dec_ctx = st->codec;
		dec_ctx->thread_count = 0; //auto number of threads
		dec_ctx->flags |= CODEC_FLAG_LOW_DELAY;
		dec_ctx->flags2 |= CODEC_FLAG2_FAST;

		dec = avcodec_find_decoder(dec_ctx->codec_id);

		if (!dec)
		{
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Could not find %s codec!\n", av_get_media_type_string(type));
			return AVERROR(EINVAL);
		}

		/* Init the decoders, with or without reference counting */
#if USE_REF_COUNTER
		av_dict_set(&opts, "refcounted_frames", "1", 0);
#else
		av_dict_set(&opts, "refcounted_frames", "0", 0);
#endif
		if ((ret = avcodec_open2(dec_ctx, dec, &opts)) < 0)
		{
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Could not open %s codec!\n", av_get_media_type_string(type));
			return ret;
		}
	}

	return 0;
}

bool TwinCapture::allocateVideoDecoderData(AVPixelFormat pix_fmt)
{
	int ret = 0;

	if (pix_fmt != mDstPixFmt)
	{
		char buf[256];
		std::size_t found;

		mVideoStrFormat = std::string(av_get_pix_fmt_string(buf, 256, pix_fmt));
		found = mVideoStrFormat.find(' ');
		if (found != std::string::npos)
			mVideoStrFormat = mVideoStrFormat.substr(0, found); //delate inrelevant data

		mVideoDstFormat.assign(av_get_pix_fmt_string(buf, 256, mDstPixFmt));
		found = mVideoDstFormat.find(' ');
		if (found != std::string::npos)
			mVideoDstFormat = mVideoDstFormat.substr(0, found); //delate inrelevant data

		sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_INFO, "Creating video scaling context (%s->%s)\n", mVideoStrFormat.c_str(), mVideoDstFormat.c_str());

		//create context for frame convertion
		mVideoScaleContext = sws_getContext(mWidth, mHeight, pix_fmt,
			mWidth, mHeight, mDstPixFmt,
			SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
		if (!mVideoScaleContext)
		{
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Could not allocate frame convertion context!\n");
			return false;
		}
	}

	mTempFrame = av_frame_alloc();
	if (!mTempFrame)
	{
		sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Could not allocate temp frame data!\n");
		return false;
	}

	mTempFrame->width = mWidth;
	mTempFrame->height = mHeight;
	mTempFrame->format = mDstPixFmt;

	ret = av_image_alloc(mTempFrame->data, mTempFrame->linesize, mWidth, mHeight, mDstPixFmt, 1);
	if (ret < 0)
	{
		sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Could not allocate temp frame buffer!\n");
		return false;
	}

	if (!mFrame)
		mFrame = av_frame_alloc();

	if (!mFrame)
	{
		sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Could not allocate frame data!\n");
		return false;
	}

	return true;
}

int TwinCapture::decodePacket(int * gotVideoPtr, int idx)
{
	int ret = 0;
	int decoded = mPkt.size;
	bool packageOk = false;

	*gotVideoPtr = 0;
	
	if (mPkt.stream_index == mVideo_stream_idx && mVideoCodecContext[idx])
	{
		/* decode video frame */
		ret = avcodec_decode_video2(mVideoCodecContext[idx], mFrame, gotVideoPtr, &mPkt);

		if (ret < 0)
		{
			sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Video decoding error: %s!\n", av_err2str(ret));
			return ret;
		}

		decoded = FFMIN(ret, mPkt.size);
		if (*gotVideoPtr)
		{
			packageOk = true;
			
			if (mVideoCodecContext[idx]->pix_fmt != mDstPixFmt)
			{
				//convert to destination pixel format
				ret = sws_scale(mVideoScaleContext, mFrame->data, mFrame->linesize, 0, mHeight, mTempFrame->data, mTempFrame->linesize);
				if (ret < 0)
				{
					sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to convert decoded frame to %s!\n", mVideoDstFormat.c_str());
					return ret;
				}
			}
			else
			{
				ret = av_frame_copy(mTempFrame, mFrame);
				if (ret < 0)
				{
					sgct::MessageHandler::instance()->print(sgct::MessageHandler::NOTIFY_ERROR, "Failed to copy frame!\n");
					return ret;
				}
			}

			//store mTempFrame;
			if (mVideoDecoderCallback != nullptr)
				mVideoDecoderCallback(mTempFrame->data, mWidth, mHeight, idx);

			mDecodedVideoFrames++;
			//std::cout << "Decoded frames: " << mDecodedVideoFrames << std::endl;

			if (ret < 0)
			{
				return ret;
			}
		}
	}

#if USE_REF_COUNTER
	/* If we use the new API with reference counting, we own the data and need
	* to de-reference it when we don't use it anymore */
	if (packageOk)
	{
		av_frame_unref(mFrame);
	}
#endif

	return decoded;
}

void TwinCapture::cleanup()
{
	mInited = false;
	mVideoDecoderCallback = nullptr;

	for (int i = 0; i < 2; ++i) {
		if (mVideoCodecContext[i])
		{
			avcodec_close(mVideoCodecContext[i]);
			mVideoCodecContext[i] = nullptr;
		}

		if (mFMTContext[i])
		{
			avformat_close_input(&mFMTContext[i]);
			mFMTContext[i] = nullptr;
		}

		mVideoStream[i] = nullptr;
	}

	if (mFrame)
	{
		av_frame_free(&mFrame);
		mFrame = nullptr;
	}

	if (mTempFrame)
	{
		av_frame_free(&mTempFrame);
		mTempFrame = nullptr;
	}

	if (mVideoScaleContext)
	{
		sws_freeContext(mVideoScaleContext);
		mVideoScaleContext = nullptr;
	}

	mWidth = 0;
	mHeight = 0;
}
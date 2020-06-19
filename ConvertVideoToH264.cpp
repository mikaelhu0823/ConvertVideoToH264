// ConvertVideoToH264.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <memory>
#include <string>
#include <fstream>
#include <thread>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>  
#include "ffencoder.h"
#include "ffdecoder.h"

using namespace std;


std::shared_ptr<FFEncoder>	spFFEncoder_;
std::shared_ptr<FFDecoder>	spFFDecoder_;

#define MAX_AUDIO_PACKET_SIZE 1024
#define MAX_VIDEO_PACKET_SIZE (1024 * 32)

void ReadVideoFrame();
void OnVideoDataCaptured(const cv::Mat& img, const FFVideoFormat& video_format);

std::shared_ptr<ofstream> sp_h264 = nullptr;

uint8_t *out_data = nullptr;
uint8_t* pYuvBuf = nullptr;
uint8_t *out_data_t = nullptr;

enum class FramePixelFormat {
	F_PIX_FMT_INVALID = 0,
	F_PIX_FMT_I420,
	F_PIX_FMT_RGB24,
	F_PIX_FMT_BGR24,
	F_PIX_FMT_NV21,

	F_PIX_FMT_NB
};

struct FrameInfo {
	uint8_t* data_;
	int		 width_;
	int		 height_;
	FramePixelFormat	framePixFormat_;
};

int main()
{
	string str_h264 = "G:/Video/Gangrongboji.h264";
	sp_h264 = std::make_shared<ofstream>(str_h264, ios::out | ios::binary);
	if (!sp_h264) {
		cout<<"The specified file:" + str_h264 + "does not exist!"<<std::endl;

		return -1;
	}

	spFFEncoder_ = std::make_shared<FFEncoder>();
	spFFDecoder_ = std::make_shared<FFDecoder>();

	FFVideoFormat format_v;
	format_v.set(720, 402, FF_PIX_FMT_I420, 400000, 25); //hnc??? camera size can not be changed automaticly?
	if (spFFEncoder_->openVideo(FF_CODEC_ID_H264, format_v) < 0) {
		return -1;
	}
	if (spFFDecoder_->openVideo(FF_CODEC_ID_H264) < 0) {
		return -1;
	}

	ReadVideoFrame();

	sp_h264->close();
	sp_h264.reset();

	spFFEncoder_->closeVideo();
	spFFEncoder_.reset();
	spFFDecoder_->closeVideo();
	spFFDecoder_.reset();

    return 0;
}

void ReadVideoFrame() {
	bool b_first = true;

	FFVideoFormat video_format;
	video_format.height = 720;
	video_format.width = 960;
	video_format.fps = /*frameRate*/25; //hnc the original is 0.
	video_format.bitrate = 400000;  //hnc???
	video_format.pix_fmt = FF_PIX_FMT_BGR24;

	//ffmpeg解码播放mp4,h264文件。
	cv::VideoCapture capture("G:/Video/Gangrongboji.mp4");
	while (true)
	{
		cv::Mat frame;			//定义一个Mat变量，用于存储每一帧的图像  
		capture >> frame;		//读取当前帧

		int type = frame.type();
		int ch = frame.channels();
		int width = frame.size().width;
		int height = frame.size().height;

		if (b_first) {
			video_format.height = frame.size().height;
			video_format.width = frame.size().width;
			b_first = false;
		}

		//若视频播放完成，退出循环  
		if (frame.empty())
		{
			break;
		}
		OnVideoDataCaptured(frame, video_format);

		////hnc for test
		//cv::imshow("opencv_image", frame);  //显示摄像头的数据
		cv::waitKey(30);
	}

	cv::waitKey(10);
	capture.release();

	delete[] out_data_t;
	out_data_t = nullptr;

	delete[] pYuvBuf;
	pYuvBuf = nullptr;

	delete[] out_data;
	out_data = nullptr;
}

void OnVideoDataCaptured(const cv::Mat& img, const FFVideoFormat& video_format)
{
	string str_err = "";
	if (spFFEncoder_) {
		
		const int BUFFSIZE = MAX_VIDEO_PACKET_SIZE;
		
		if (out_data == nullptr) {
			out_data = new uint8_t[BUFFSIZE];
		}			
		memset(out_data, 0, BUFFSIZE);

		int out_size = -1;

		//hnc
		//convert BGR24 to YUV420
		cv::Mat yuvImg/*(video_format.height, video_format.width, CV_8UC3)*/;
		cvtColor(img, yuvImg, CV_RGB2YUV_I420);
		int bufLen = video_format.width * video_format.height * 3 >> 1;

		if (pYuvBuf == nullptr){
			pYuvBuf = new unsigned char[bufLen];
		}
		
		memset(pYuvBuf, 0, bufLen);
		memcpy(pYuvBuf, yuvImg.data, bufLen * sizeof(unsigned char));//pYuvBuf即为所获取的YUV420数据 

		const_cast<FFVideoFormat&>(video_format).pix_fmt = FF_PIX_FMT_I420;

		//int bufLen = video_format.width * video_format.height * 3;

		//hnc for test ps
		//clock_t start_time = clock();
		long bret = spFFEncoder_->encodeVideo(/*img.data*/pYuvBuf, bufLen, video_format, out_data, out_size);
		/*clock_t end_time = clock();
		double sec = static_cast<double> (end_time - start_time) / CLOCKS_PER_SEC;*/

		if (bret == 0) {
			sp_h264->write((char*)out_data, out_size);
			/*if (!spVideoAudioJrtpSessionMng_->SendH264(out_data, out_size, str_err)) {

				cout << "JRTP send AAC frame failed!";
			}*/


			//hnc for test
			int width = img.size().width;
			int height = img.size().height;

			const int BUFFSIZE = width * height * 3;
			if (out_data_t == nullptr) {
				out_data_t = new uint8_t[BUFFSIZE];
			}

			memset(out_data_t, 0, BUFFSIZE);

			//int out_size = -1;
			int out_size_t = BUFFSIZE;
			FFVideoFormat out_fmt;

			out_fmt.set(width, height, FF_PIX_FMT_RGB24, 400000, 25);

			//to decode
			if (!spFFDecoder_->decodeVideo(out_data, out_size, out_data_t, out_size_t, out_fmt)) {
				//call ui comp to show the frame.
				FrameInfo frame_info;
				frame_info.data_ = out_data_t;
				frame_info.width_ = width;
				frame_info.height_ = height;
				frame_info.framePixFormat_ = FramePixelFormat::F_PIX_FMT_RGB24;

				cv::Mat frame = cv::Mat(cv::Size(width, height), CV_8UC3, out_data_t);
				//若视频播放完成，退出循环  
				if (frame.empty())
				{
					return;
				}
				cv::imshow("显示视频", frame);  //显示当前帧  
				//cv::waitKey(30);  //延时30ms  
			}
		}
	}
}


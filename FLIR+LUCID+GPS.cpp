#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include "stdafx.h"
#include "ArenaApi.h"
#include "SaveApi.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <ctime>
#include <fstream>
#include <assert.h>
#include <time.h>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <string>
#include <stdio.h>
#include <Windows.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include "opencv2/imgproc/imgproc.hpp"
#include "SerialPort.h" //USB 통신

#ifndef _WIN32
#include <pthread.h>
#endif

using namespace std::chrono;
//using namespace Spinnaker;
//using namespace Spinnaker::GenApi;
//using namespace Spinnaker::GenICam;
using namespace std;

// Initialize config parameters, will be updated by config file
std::string path;
std::string triggerCam = "20323052"; // serial number of primary camera
double exposureTime = 5000.0; // exposure time in microseconds (i.e., 1/ max FPS)
double FPS = 200.0; // frames per second in Hz
double compression = 1.0; // compression factor
int widthToSet;
int heightToSet;
double NewFrameRate;
int numBuffers = 200; // depending on RAM

// Initialize placeholders
vector<ofstream> cameraFiles;
ofstream csvFile;
ofstream metadataFile;
string metadataFilename;
int cameraCnt;
string serialNumberF;
string serialNumberL;
// mutex lock for parallel threads
HANDLE ghMutex;

// Camera trigger type for primary and secondary cameras
enum triggerType
{
	SOFTWARE, // Primary camera
	HARDWARE // Secondary camera
};

// Initialization overwritten in InitializeMultipleCameras()
triggerType chosenTrigger = HARDWARE;

/*
================
This function reads the config file to update camera parameters such as working directory, serial of primary camera and Framerate
================
*/


int readconfig()
{
	int result = 0;

	std::cout << "Reading config file... ";
	std::ifstream cFile("myConfig.txt");
	if (cFile.is_open())
	{

		std::string line;
		while (getline(cFile, line))
		{
			line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
			if (line[0] == '#' || line.empty()) continue;

			auto delimiterPos = line.find("=");
			auto name = line.substr(0, delimiterPos);
			auto value = line.substr(delimiterPos + 1);

			//Custom coding
			if (name == "triggerCam") triggerCam = value;
			else if (name == "FPS") FPS = std::stod(value);
			else if (name == "compression") compression = std::stod(value);
			else if (name == "exposureTime") exposureTime = std::stod(value);
			else if (name == "numBuffers") numBuffers = std::stod(value);
			else if (name == "path") path = value;
		}
	}
	else
	{
		std::cerr << " Couldn't open config file for reading.";
	}

	std::cout << " Done!" << endl;
	/*
	std::cout << "\ntriggerCam=" << triggerCam;
	std::std::cout << "\nFPS=" << FPS;
	std::std::cout << "\ncompression=" << compression;
	std::std::cout << "\nexposureTime=" << exposureTime;
	std::std::cout << "\nnumBuffers=" << numBuffers;
	std::std::cout << "\nPath=" << path << endl << endl;
	*/
	return result, triggerCam, exposureTime, path, FPS, compression, numBuffers;
}

/*
=================
These helper functions getCurrentDateTime and removeSpaces get the system time and transform it to readable format. The output string is used as timestamp for new filenames. The function getTimeStamp prints system writing time in nanoseconds to the csv logfile.
=================
*/
string removeSpaces(string word)
{
	string newWord;
	for (int i = 0; i < word.length(); i++) {
		if (word[i] != ' ') {
			newWord += word[i];
		}
	}
	return newWord;
}

string getCurrentDateTime()
{
	stringstream currentDateTime;
	// current date/time based on system
	time_t ttNow = time(0);
	tm* ptmNow;
	// year
	ptmNow = localtime(&ttNow);
	currentDateTime << 1900 + ptmNow->tm_year;
	//month
	if (ptmNow->tm_mon < 10) //Fill in the leading 0 if less than 10
		currentDateTime << "0" << 1 + ptmNow->tm_mon;
	else
		currentDateTime << (1 + ptmNow->tm_mon);
	//day
	if (ptmNow->tm_mday < 10) //Fill in the leading 0 if less than 10
		currentDateTime << "0" << ptmNow->tm_mday << " ";
	else
		currentDateTime << ptmNow->tm_mday << " ";
	//spacer
	currentDateTime << "_";
	//hour
	if (ptmNow->tm_hour < 10) //Fill in the leading 0 if less than 10
		currentDateTime << "0" << ptmNow->tm_hour;
	else
		currentDateTime << ptmNow->tm_hour;
	//min
	if (ptmNow->tm_min < 10) //Fill in the leading 0 if less than 10
		currentDateTime << "0" << ptmNow->tm_min;
	else
		currentDateTime << ptmNow->tm_min;
	//sec
	if (ptmNow->tm_sec < 10) //Fill in the leading 0 if less than 10
		currentDateTime << "0" << ptmNow->tm_sec;
	else
		currentDateTime << ptmNow->tm_sec;
	string test = currentDateTime.str();

	test = removeSpaces(test);

	return test;
}

auto getTimeStamp()
{
	std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();

	typedef std::chrono::duration<int, std::ratio_multiply<std::chrono::hours::period, std::ratio<8>>::type> Days;

	Days days = std::chrono::duration_cast<Days>(duration);
	duration -= days;

	auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);

	auto timestamp = nanoseconds.count();
	return timestamp;
}

/*
=================
The function CreateFiles works within a for loop and creates .tmp binary files for each camera, as well as a single .csv logging sheet csvFile. The files are saved in working irectory.
=================
*/
int CreateFiles(string serialNumber, int cameraCnt, string CurrentDateTime_)
{
	int result = 0;
	//std::cout << endl << "CREATING FILES:..." << endl;

	stringstream sstream_tmpFilename;
	stringstream sstream_csvFile;
	stringstream sstream_metadataFile;
	string tmpFilename;
	string csvFilename;
	ofstream filename; // TODO: still needed?
	const string csDestinationDirectory = path;

	// Create temporary file from serialnum assigned to cameraCnt
	//sstream_tmpFilename << csDestinationDirectory << CurrentDateTime_ + "_" + serialNumber << "_file" << cameraCnt << ".tmp";
	sstream_tmpFilename << csDestinationDirectory << CurrentDateTime_ + "_" + serialNumber << ".tmp";
	sstream_tmpFilename >> tmpFilename;

	//std::cout << "File " << tmpFilename << " initialized" << endl;

	cameraFiles.push_back(std::move(filename)); // TODO: still needed?
	cameraFiles[cameraCnt].open(tmpFilename.c_str(), ios_base::out | ios_base::binary);

	// Create .csv logfile and .txt metadata only once for all cameras during first loop
	if (cameraCnt == 0)
	{
		// create csv logfile with headers
		sstream_csvFile << csDestinationDirectory << "logfile_" << CurrentDateTime_ << ".csv";
		sstream_csvFile >> csvFilename;

		//std::cout << "CSV file: " << csvFilename << " initialized" << endl;

		csvFile.open(csvFilename);
		csvFile << "FrameID" << "," << "Timestamp" << "," << "SerialNumber" << "," << "FileNumber" << "," << "SystemTimeInNanoseconds" << endl;

		// create txt metadata
		sstream_metadataFile << csDestinationDirectory << "metadata_" << CurrentDateTime_ << ".txt";
		sstream_metadataFile >> metadataFilename;

	}
	return result;
}


/*
=================
The function CleanBuffer runs on all cameras in hardware trigger mode before the actual recording. If the buffer contains images, these are released in the for loop. When the buffer is empty, GetNextImage will get timeout error and end the loop.
=================
*/
int CleanBuffer(Spinnaker::CameraPtr pCam)
{
	int result = 0;

	// Clean Buffer acquiring idle images
	pCam->BeginAcquisition();

	try
	{
		for (unsigned int imagesInBuffer = 0; imagesInBuffer < numBuffers; imagesInBuffer++)
		{
			// first numBuffer images are descarted
			Spinnaker::ImagePtr pResultImage = pCam->GetNextImage(100);
			char* imageData = static_cast<char*>(pResultImage->GetData());
			pResultImage->Release();
		}
	}
	catch (Spinnaker::Exception& e)
	{
		//std::cout << "Buffer clean. " << endl;
	}

	pCam->EndAcquisition();

	return result;
}

/*
=================
The function AcquireImages runs in parallel threads and grabs images from each camera and saves them in the corresponding binary file. Each image also records the image status to the logging csvFile.
=================
*/
char* monitoringImgL = NULL;
char* monitoringImgF = NULL;
bool AcquireOK = FALSE;
DWORD WINAPI AcquireImagesFLIR(LPVOID lpParam)
{
	cout << "FLIR thread start" << endl;
	AcquireOK = TRUE;
	// START function in UN-locked thread

	// Initialize camera
	Spinnaker::CameraPtr pCam = *((Spinnaker::CameraPtr*)lpParam);
	pCam->Init();

	// Start actual image acquisition
	pCam->BeginAcquisition();

	// Initialize empty parameters outside of locked case
	Spinnaker::ImagePtr pResultImage;
	char* imageData; // 모니터링을 위해 밖으로 빼고 싶은데, thread에 필요한 거라서 안될듯..
	string deviceUser_ID;
	int firstFrame = 1;
	int stopwait = 0;

	// Retrieve and save images in while loop until manual ESC press

	printf("Running...\n");
	while (GetAsyncKeyState(VK_ESCAPE) == 0)
	{

		// Start mutex_lock
		DWORD dwCount = 0, dwWaitResult;
		dwWaitResult = WaitForSingleObject(
			ghMutex,    // handle to mutex
			INFINITE);  // no time-out interval
		switch (dwWaitResult)
		{

			// The thread got ownership of the mutex
		case WAIT_OBJECT_0:
			try
			{
				// Identify specific camera in locked thread
				serialNumberF = "22623682";
				cameraCnt = 0;

				// For first loop in while ...
				if (firstFrame == 1)
				{
					// first image is descarted
					Spinnaker::ImagePtr pResultImage = pCam->GetNextImage();
					char* imageData = static_cast<char*>(pResultImage->GetData());
					pResultImage->Release();

					// anounce start recording
					std::cout << "Camera [" << serialNumberF << "] " << "Started recording with ID [" << cameraCnt << " ]..." << endl;
				}
				firstFrame = 0;

				// Retrieve image and ensure image completion
				try
				{
					pResultImage = pCam->GetNextImage(); // waiting time for NextImage in miliseconds, never time out.
				}
				catch (Spinnaker::Exception& e)
				{
					// Break recording loop after waiting 1000ms without trigger
					stopwait = 1;
				}

				// Break recording loop when stopwait is activated
				if (stopwait == 1)
				{
					std::cout << "Trigger signal disconnected. Stop recording for camera [" << serialNumberF << "]" << endl;
					// End acquisition
					pCam->EndAcquisition();
					cameraFiles[cameraCnt].close();
					csvFile.close();
					// Deinitialize camera
					pCam->DeInit();

					break;
				}

				// Acquire the image buffer to write to a file
				// GetData()가 두번 연속 나오는건 문제가 있어보인다.
				imageData = static_cast<char*>(pResultImage->GetData());
				monitoringImgF = imageData;

				// Do the writing to assigned cameraFile
				cameraFiles[cameraCnt].write(imageData, pResultImage->GetImageSize());
				//cout << pResultImage->GetImageSize() << endl;
				csvFile << pResultImage->GetFrameID() << "," << pResultImage->GetTimeStamp() << "," << serialNumberF << "," << cameraCnt << "," << getTimeStamp() << endl;

				// Check if the writing is successful
				if (!cameraFiles[cameraCnt].good())
				{
					std::cout << "Error writing to file for camera " << cameraCnt << " !" << endl;
					return -1;
				}

				// Release image
				pResultImage->Release();
			}

			catch (Spinnaker::Exception& e)
			{
				std::cout << "Error: " << e.what() << endl;
				return -1;
			}

			// normal break
			ReleaseMutex(ghMutex);
			break;

			// The thread got ownership of an abandoned mutex
		case WAIT_ABANDONED:
			std::cout << "wait abandoned" << endl; //TODO what is this for?
		}
	}
	// End acquisition
	pCam->EndAcquisition();
	cameraFiles[cameraCnt].close();
	csvFile.close();

	// Deinitialize camera
	pCam->DeInit();
	std::cout << "End FLIR camera!!" << endl;
	AcquireOK = FALSE;
	return 1;
}

DWORD WINAPI AcquireImagesLUCID(LPVOID lpParam)
{
	cout << "LUCID thread start!" << endl;
	AcquireOK = TRUE;
	// START function in UN-locked thread
	// Initialize camera
	Arena::IDevice* pDevice = (Arena::IDevice*)lpParam;
	// Start actual image acquisition
	pDevice->StartStream();
	// Initialize empty parameters outside of locked case
	Arena::IImage* pResultImage;
	//char* imageData; // 모니터링을 위해 밖으로 빼고 싶은데, thread에 필요한 거라서 안될듯..
	char* imageData0;
	//string deviceUser_ID;
	int firstFrame = 1;
	int stopwait = 0;

	// Retrieve and save images in while loop until manual ESC press
	printf("Running...\n");
	// Start mutex_lock
	while (GetAsyncKeyState(VK_ESCAPE) == 0)
	{
		DWORD dwCount = 0, dwWaitResult;
		dwWaitResult = WaitForSingleObject(
			ghMutex,    // handle to mutex
			INFINITE);  // no time-out interval
		switch (dwWaitResult)
		{

			// The thread got ownership of the mutex
		case WAIT_OBJECT_0:
			try
			{
				// Identify specific camera in locked thread
				cameraCnt = 1;
				serialNumberL = "232000061";
				// For first loop in while ...
				if (firstFrame == 1)
				{
					// first image is descarted

					Arena::IImage* pResultImage0 = pDevice->GetImage(2000);

					//const uint8_t* pInput = pResultImage0->GetData();
					//const uint8_t* pIn = pInput;
					char* imageData0 = reinterpret_cast<char*>(const_cast<uint8_t*>(pResultImage0->GetData()));

					//Save::ImageParams params(2448, 2048, Arena::GetBitsPerPixel(Mono8));
					//Save::ImageWriter writer(params, "testL.jpg");
					//int imageSize = 2448 * 2048;
					//Arena::IImage* pimage = Arena::ImageFactory::Create(reinterpret_cast<uint8_t*>(imageData0), imageSize, 2448, 2048, Mono8);
					//writer << pimage->GetData();

					delete[] pResultImage0;

					// anounce start recording
					std::cout << "Camera [" << serialNumberL << "] " << "Started recording with ID [" << cameraCnt << " ]..." << endl;
				}
				firstFrame = 0;

				// Retrieve image and ensure image completion
				try
				{
					pResultImage = pDevice->GetImage(2000);// waiting time for NextImage in 2000 miliseconds.
				}
				catch (Spinnaker::Exception& e)
				{
					// Break recording loop after waiting 1000ms without trigger
					stopwait = 1;
				}

				// Break recording loop when stopwait is activated
				if (stopwait == 1)
				{
					std::cout << "Trigger signal disconnected. Stop recording for camera [" << serialNumberL << "]" << endl;
					// End acquisition
					pDevice->StopStream();
					cameraFiles[cameraCnt].close();
					csvFile.close();
					// Deinitialize camera
					// LUCID는 RunMultipleCameras 에서 DestroyDevice가 이루어짐

					break;
				}

				//imageData = static_cast<char*>(pResultImage->GetData());
				imageData0 = reinterpret_cast<char*>(const_cast<uint8_t*>(pResultImage->GetData()));
				// Acquire the image buffer to write to a file
				//monitoringImgL = pResultImage->GetData();
				monitoringImgL = imageData0;

				// Do the writing to assigned cameraFile
				//cout << pResultImage->GetSizeFilled() << endl; == 2448*2048 = 5013504
				cameraFiles[cameraCnt].write(imageData0, pResultImage->GetSizeFilled());
				csvFile << pResultImage->GetFrameId() << "," << pResultImage->GetTimestamp() << "," << serialNumberL << "," << cameraCnt << "," << getTimeStamp() << endl;

				// Check if the writing is successful
				if (!cameraFiles[cameraCnt].good())
				{
					std::cout << "Error writing to file for camera " << cameraCnt << " !" << endl;
					return -1;
				}

				// Release image
				pDevice->RequeueBuffer(pResultImage);
			}

			catch (Spinnaker::Exception& e)
			{
				std::cout << "Error: " << e.what() << endl;
				return -1;
			}

			// normal break
			ReleaseMutex(ghMutex);
			break;

			// The thread got ownership of an abandoned mutex
		case WAIT_ABANDONED:
			std::cout << "wait abandoned" << endl; //TODO what is this for?
		}
	}
	// End acquisition
	pDevice->StopStream();
	//cameraFiles[cameraCnt].close();
	//csvFile.close();

	// Deinitialize camera
	// LUCID는 RunMultipleCameras 에서 DestroyDevice가 이루어짐

	std::cout << "End LUCID camera!!" << endl;
	AcquireOK = FALSE;
	return 1;
}

DWORD WINAPI MonitoringThread(LPVOID lpParam)
{
	std::cout << "MonitoringThread start!" << endl;
	// 리사이즈할 크기 지정
	// 이미지 리사이즈
	cv::Mat resized_image = cv::Mat::zeros(cv::Size(2248 / 2, 2048 / 4), CV_8UC1);
	cv::Mat imgF = cv::Mat::zeros(2248, 2048, CV_8UC1);
	cv::Mat imgL = cv::Mat::zeros(2248, 2048, CV_8UC1);
	Spinnaker::ImagePtr pImageF;
	Arena::IImage* pImageL;
	cv::Mat concatImg = cv::Mat::zeros(cv::Size(2248 * 2, 2048), CV_8UC1);;
	cv::Scalar_<int> blue(255, 0, 0);
	string ExposureTimeMonitor = "exposure time : " + to_string(exposureTime);
	string FrameMonitor = "Frame rate : " + to_string(FPS);

	Save::ImageParams params(2448, 2048, Arena::GetBitsPerPixel(Mono8));
	Save::ImageWriter writer(params, "testL.jpg");

	while (AcquireOK)
	{
		if (monitoringImgF != NULL && monitoringImgL != NULL)
		{
			try
			{
				//pImage1 = Spinnaker::Image::Create(2448, 2048, 0, 0, Spinnaker::PixelFormat_BayerRGPolarized8, monitoringImgL);
				pImageF = Spinnaker::Image::Create(2448, 2048, 0, 0, Spinnaker::PixelFormat_BayerRG8, monitoringImgF);
				pImageF->Save("testF.jpg");
				//reinterpret_cast<char*>(const_cast<uint8_t*>(pResultImage->GetData()))
				//cout << sizeof(*&monitoringImgL) << endl;
				//cout << sizeof(*reinterpret_cast<uint8_t*>(monitoringImgL)) << endl;
				//cout << 2448 * 2048 * Arena::GetBitsPerPixel(Mono8) / 8 << endl;

				pImageL = Arena::ImageFactory::Create(reinterpret_cast<uint8_t*>(monitoringImgL), 2448 * 2048 * Arena::GetBitsPerPixel(Mono8) / 8, 2448, 2048, Mono8);
				writer << pImageL->GetData();


				imgF = cv::imread("testF.jpg", 1);
				imgL = cv::imread("testL.jpg", 1);
				if (imgF.empty()) printf("imgF.empty");
				if (imgL.empty()) printf("imgL.empty");

				cv::hconcat(imgL, imgF, concatImg);
				if (concatImg.empty()) printf("concatImg.empty");
				if (resized_image.empty()) printf("resized_image.empty");
				cv::resize(concatImg, resized_image, cv::Size(2248 / 2, 2048 / 4));
				cv::putText(resized_image, ExposureTimeMonitor, cv::Point(0, 30), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, 8);
				cv::putText(resized_image, FrameMonitor, cv::Point(0, 60), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, 8);
				cv::imshow("img", resized_image);
				
				monitoringImgF = NULL;
				monitoringImgL = NULL;
				//Mat img = imread("test.jpg", 1);
				//imshow("img", img);
			}
			catch (string error)
			{
				cout << error << endl;
			}
		}
		int keycode = cv::waitKey(5);

	}

	cv::destroyAllWindows();
	std::cout << "MonitoringThread done!" << endl;
	return 0;
}


//실제 데이터가 들어오는것을 모니터링
DWORD WINAPI CommWatchProc(LPVOID lpData)
{
	cout << "GPS Thread start!!" << endl;
	DWORD       dwEvtMask;
	OVERLAPPED  os;
	CSerialPort* npComm = (CSerialPort*)lpData;
	char InData[COM_MAXBLOCK + 1];
	int        nLength;
	memset(&os, 0, sizeof(OVERLAPPED));


	os.hEvent = CreateEvent(
		NULL,    // no security
		TRUE,    // explicit reset req
		FALSE,   // initial event reset
		NULL); // no name

	if (!SetCommMask(npComm->m_hComm, EV_RXCHAR)) //EV_RXCHAR만 검출. 이래야 오류가 안뜨더라.
		return (FALSE);

	int i = 0;
	DWORD lpModernstat;
	BOOL fCTS, fDSR, fRING, fRLSD;
	while (npComm->fConnected && GetAsyncKeyState(VK_ESCAPE) == 0 && AcquireOK)
	{
		dwEvtMask = 0;
		WaitCommEvent(npComm->m_hComm, &dwEvtMask, &os);
		if ((dwEvtMask & EV_BREAK) == EV_BREAK)
		{
			cout << "EV_BREAK" << endl;
		}
		if ((dwEvtMask & EV_CTS) == EV_CTS)
		{
			cout << "EV_CTS" << endl;
		}
		if ((dwEvtMask & EV_ERR) == EV_ERR)
		{
			cout << "EV_ERR" << endl;
		}
		if ((dwEvtMask & EV_RING) == EV_RING)
		{
			cout << "EV_RING" << endl;
		}
		if ((dwEvtMask & EV_RLSD) == EV_RLSD)
		{
			cout << "EV_RLSD" << endl;
		}
		if ((dwEvtMask & EV_RXCHAR) == EV_RXCHAR)
		{
			do
			{
				memset(InData, 0, COM_MAXBLOCK);
				if (nLength = npComm->ReadCommBlock((LPSTR)InData, COM_MAXBLOCK))
				{
					cout << InData;

					char buffer[128];
					snprintf(buffer, sizeof(buffer), "%lld", getTimeStamp());
					char* num_string = buffer;

					cameraFiles[2].write("timestamp", 9);
					cameraFiles[2].write(num_string, strlen(num_string));
					cameraFiles[2].write("\n", 1);
					cameraFiles[2].write(InData, strlen(InData));
					//이곳에서 데이타를 받는다.
				}
			} while (nLength > 0);

		}
		if ((dwEvtMask & EV_RXFLAG) == EV_RXFLAG)
		{
			cout << "EV_RXFLAG" << endl;
		}
		if ((dwEvtMask & EV_TXEMPTY) == EV_TXEMPTY)
		{
			//npComm->bTxEmpty = TRUE;
		}
	}

	CloseHandle(os.hEvent);
	cameraFiles[2].close();
	npComm->dwThreadID = 0;
	//npComm->hWatchThread = NULL;
	cout << "GPS thread Done!!!" << endl;
	return(TRUE);
}

/*
=================
The function RunMultipleCameras acts as the body of the program. It calls the InitializeMultipleCameras function to configure all cameras, then creates parallel threads and starts AcquireImages on each thred. With the setting configured above, threads are waiting for synchronized trigger as a central clock.
=================
*/
int RunMultipleCameras(Spinnaker::CameraList camList, std::vector<Arena::DeviceInfo> deviceInfos, Arena::IDevice* pDevice)
{
	int result = 0;
	unsigned int camListSizeF = 0;
	unsigned int camListSizeL = 0;
	unsigned int TotalThreadNum = 0;
	try
	{
		// Retrieve camera list size
		camListSizeF = camList.GetSize();
		camListSizeL = deviceInfos.size();

		// Create an array of handles
		Spinnaker::CameraPtr* pCamList = new Spinnaker::CameraPtr[camListSizeF];
		pCamList[0] = camList.GetByIndex(0);
		// Initialize cameras in camList 
		//result = InitializeMultipleCameras(camList, pCamList, pDevice, deviceInfos, camListSizeF, camListSizeL);
		std::cout << "Initializing camera LUCID " << serialNumberL << " and FLIR " << serialNumberF << "... ";
		serialNumberL = "232000061";
		serialNumberF = "22623682";
		string CurrentDateTime = getCurrentDateTime();
		CreateFiles(serialNumberF, 0, CurrentDateTime);
		CreateFiles(serialNumberL, 1, CurrentDateTime);
		CreateFiles("GPS", 2, CurrentDateTime);
		//seting gps logging
		CSerialPort _serial;
		if (_serial.OpenPort(L"COM7"))   // 실제 사용될 COM Port 를 넣어야합니다.  
		{
			// BaudRate, ByteSize, fParity, Parity, StopBit 정보를 설정해줍니다.  
			_serial.ConfigurePort(CBR_115200, 8, FALSE, NOPARITY, ONESTOPBIT);
			// Timeout 설정입니다. 별다른거 없으면 전부 zero 설정해도 되구요.  
			_serial.SetCommunicationTimeouts(0xFFFFFFFF, 0, 1000, 2, 0);

		}

		// START RECORDING
		std::cout << endl << "*** START RECORDING ***" << endl;
		TotalThreadNum = camListSizeF + camListSizeL + 2;
		HANDLE* grabThreads = new HANDLE[TotalThreadNum];


		grabThreads[0] = CreateThread(nullptr, 0, AcquireImagesFLIR, &pCamList[0], 0, nullptr); // call AcquireImages in parallel threads
		assert(grabThreads[0] != nullptr);

		grabThreads[1] = CreateThread(nullptr, 0, AcquireImagesLUCID, pDevice, 0, nullptr); // call AcquireImages in parallel threads
		assert(grabThreads[1] != nullptr);

		grabThreads[2] = CreateThread(nullptr, 0, MonitoringThread, nullptr, 0, nullptr);
		assert(grabThreads[2] != nullptr);

		grabThreads[3] = CreateThread((LPSECURITY_ATTRIBUTES)NULL,0,(LPTHREAD_START_ROUTINE)CommWatchProc,(LPVOID)&_serial,0, nullptr); // call AcquireImages in parallel threads
		assert(grabThreads[3] != nullptr);

		//HANDLE monitoringThread = CreateThread(nullptr, 0, MonitoringThread, nullptr, 0, nullptr);
		// Wait for all threads to finish
		WaitForMultipleObjects(
			TotalThreadNum, // number of threads to wait for
			grabThreads, // handles for threads to wait for
			TRUE,        // wait for all of the threads
			INFINITE     // wait forever
		);

		std::cout << endl << "*** CloseHandle  ***" << endl;
		CloseHandle(ghMutex);

		// Check thread return code for each camera
		for (unsigned int i = 0; i < TotalThreadNum; i++)
		{
			DWORD exitcode;
			BOOL rc = GetExitCodeThread(grabThreads[i], &exitcode);
			if (!rc)
			{
				std::cout << "Handle error from GetExitCodeThread() returned for camera at index " << i << endl;
				result = -1;
			}
			else if (!exitcode)
			{
				std::cout << "Grab thread for camera at index " << i << " exited with errors." << endl;
				result = -1;
			}
		}

		// Clear CameraPtr array and close all handles
		pCamList[0] = 0;
		for (unsigned int i = 0; i < TotalThreadNum; i++)
		{
			
			CloseHandle(grabThreads[i]);
		}
		//CloseHandle(monitoringThread);
		// Delete array pointer
		delete[] grabThreads;
		//delete monitoringThread;

		// End recording
		std::cout << endl << "*** STOP RECORDING ***" << endl;

		// Reset Cameras
		//result = ResetSettings(camList, pCamList, camListSizeF);

		// Clear camera List
		camList.Clear();
		// Clear GPS
		_serial.ClosePort();
		// Delete array pointer
		delete[] pCamList;

	}
	catch (Spinnaker::Exception& e)
	{
		std::cout << "Error: " << e.what() << endl;
		result = -1;
	}
	return result;
}


/*
=================
This is the Entry point for the program. After testing writing priviledges it gets all camera instances and calls RunMultipleCameras.
=================
*/
int main(int /*argc*/, char** /*argv*/)
{
	// get terminal window width and adapt string length?
	// Print application build information
	std::cout << "***********************************************************************" << endl;
	std::cout << "		Application build date: " << __DATE__ << " " << __TIME__ << endl;
	std::cout << "	MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com" << endl;
	std::cout << "***********************************************************************" << endl;

	// Testing ascii logo print
	// source: https://patorjk.com/software/taag/#p=display&h=3&v=3&f=Big%20Money-sw&t=syncFLIR%0A
	std::cout << R"(
                                     ________ __       ______ _______  
                                    /        /  |     /      /       \ 
  _______ __    __ _______   _______$$$$$$$$/$$ |     $$$$$$/$$$$$$$  |
 /       /  |  /  /       \ /       $$ |__   $$ |       $$ | $$ |__$$ |
$$$$$$$/$$ |  $$ $$$$$$$  /$$$$$$$/$$    |  $$ |       $$ | $$    $$< 
$$      \$$ |  $$ $$ |  $$ $$ |     $$$$$/   $$ |       $$ | $$$$$$$  |
 $$$$$$  $$ \__$$ $$ |  $$ $$ \_____$$ |     $$ |_____ _$$ |_$$ |  $$ |
     $$/$$    $$ $$ |  $$ $$       $$ |     $$       / $$   $$ |  $$ |
$$$$$$$/  $$$$$$$ $$/   $$/ $$$$$$$/$$/      $$$$$$$$/$$$$$$/$$/   $$/ 
         /  \__$$ |                                                    
         $$    $$/                                                     
          $$$$$$/  

)";

	int result = 0;

	// Test permission to write to directory
	stringstream testfilestream;
	string testfilestring;
	testfilestream << "test.txt";
	testfilestream >> testfilestring;
	const char* testfile = testfilestring.c_str();

	FILE* tempFile = fopen(testfile, "w+");
	if (tempFile == nullptr)
	{
		std::cout << "Failed to create file in current folder.  Please check permissions." << endl;
		std::cout << "Press Enter to exit..." << endl;
		getchar();
		return -1;
	}

	fclose(tempFile);
	remove(testfile);


	// Retrieve singleton reference to system object
	Spinnaker::SystemPtr system = Spinnaker::System::GetInstance();
	Arena::ISystem* pSystemL = Arena::OpenSystem();

	// Retrieve list of cameras from the system
	Spinnaker::CameraList camList = system->GetCameras();
	unsigned int numCamerasF = camList.GetSize();

	pSystemL->UpdateDevices(100);
	std::vector<Arena::DeviceInfo> deviceInfos = pSystemL->GetDevices();
	unsigned int numCamerasL = deviceInfos.size();


	std::cout << "	Number of cameras detected: " << numCamerasF + numCamerasL << endl << endl;

	// Finish if there are no cameras
	if (numCamerasL + numCamerasF != 2)
	{
		// Clear camera list before releasing system
		deviceInfos.clear();
		camList.Clear();

		// Release system
		system->ReleaseInstance();
		if (numCamerasF == 0)std::cout << "No FLIR cameras connected!" << endl;
		if (numCamerasF == 0)std::cout << "No LUCID cameras connected!" << endl;
		std::cout << "Done! CAMERA(S) DIDN'T CONNETED...." << endl;
		getchar();

		return -1;
	}
	Arena::IDevice* pDevice = pSystemL->CreateDevice(deviceInfos[0]);
	// Read config file and update parameters
	readconfig();

	// Create mutex to lock parallel threads
	ghMutex = CreateMutex(
		NULL,              // default security attributes
		FALSE,             // initially not owned
		NULL);             // unnamed mutex

	if (ghMutex == NULL)
	{
		printf("CreateMutex error: %d\n", GetLastError());
		return -1;
	}



	// Run all cameras
	result = RunMultipleCameras(camList, deviceInfos, pDevice);

	// Save metadata with recording settings and placeholders for BINtoAVI
	metadataFile.open(metadataFilename);
	metadataFile << "# This is the metadata summarizing recording parameters:" << endl;
	metadataFile << "# 1) Change ColorVideo = 1/0, chosenVideoType =UNCOMPRESSED/MJPG/H264 and VideoPath" << endl;
	metadataFile << "# 2) For MJPG use quality index from 1 to 100, for H264 use bitrates from 1e6 to 100e6 bps" << endl;
	metadataFile << "# 3) Set maximum video size and max available RAM in GB" << endl;
	metadataFile << "# 4) Set directory for converted videos" << endl;
	metadataFile << "Filename=" << metadataFilename << endl;
	metadataFile << "Framerate=" << NewFrameRate << endl;
	metadataFile << "ImageHeight=" << heightToSet << endl;
	metadataFile << "ImageWidth=" << widthToSet << endl;
	metadataFile << "ColorVideo=1" << endl;
	metadataFile << "chosenVideoType=H264" << endl;
	metadataFile << "h264bitrate=30000000" << endl;
	metadataFile << "mjpgquality=90" << endl;
	metadataFile << "maxVideoSize=4" << endl;
	metadataFile << "maxRAM=10" << endl;
	metadataFile << "VideoPath=" << path << endl;

	std::cout << "Metadata file: " << metadataFilename << " saved!" << endl;

	// Clear camera list before releasing system
	camList.Clear();



	// Release system
	system->ReleaseInstance();
	pSystemL->DestroyDevice(pDevice);
	// Testing ascii logo print
	// source: https://patorjk.com/software/taag/#p=display&h=3&v=3&f=Big%20Money-sw&t=syncFLIR%0A
	std::cout << "						Press Enter to exit..." << endl;
	// Print application build information
	std::cout << "***********************************************************************" << endl;
	std::cout << "		Application build date: " << __DATE__ << " " << __TIME__ << endl;
	std::cout << "	MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com" << endl;
	std::cout << "***********************************************************************" << endl;
	getchar();

	return result;
}

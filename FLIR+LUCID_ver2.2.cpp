// 본 코드는 LUCID, FLIR 카메라 동시 취득을위한 코드이다
// 16bit 데이터 취득 버전 코드이다

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
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

#ifndef _WIN32
#include <pthread.h>
#endif

using namespace std::chrono;
using namespace std;

// Initialize config parameters, will be updated by config file
std::string path;

// Initialize placeholders
vector<ofstream> cameraFiles;
ofstream csvFile;
int cameraCnt;
string serialNumberF;
string serialNumberL;
// mutex lock for parallel threads
HANDLE ghMutex;


// myconfig.txt 폴더를 읽어 저장 경로를 설정
int readconfig()
{
	int result = 0;
	std::cout << "Reading config file... ";
	std::ifstream cFile("myconfig.txt");
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
			if (name == "path") path = value;
		}
	}
	else
	{
		std::cerr << " Couldn't open config file for reading.";
	}
	std::cout << " Done!" << endl;
	return result;
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

// 취득 시점의 시간을 받아와서, 파일명에 사용한다
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
	string tmpFilename;
	string csvFilename;
	ofstream filename; // TODO: still needed?
	const string csDestinationDirectory = path;

	// 카메라 로깅 데이터 파일을 만든다
	sstream_tmpFilename << csDestinationDirectory << CurrentDateTime_ + "_" + serialNumber << ".bin";
	sstream_tmpFilename >> tmpFilename;

	//std::cout << "File " << tmpFilename << " initialized" << endl;

	cameraFiles.push_back(std::move(filename)); // TODO: still needed?
	cameraFiles[cameraCnt].open(tmpFilename.c_str(), ios_base::out | ios_base::binary);

	// Create .csv logfile and .txt  only once for all cameras during first loop
	if (cameraCnt == 0)
	{
		// create csv logfile with headers
		sstream_csvFile << csDestinationDirectory << "logfile_" << CurrentDateTime_ << ".csv";
		sstream_csvFile >> csvFilename;

		//std::cout << "CSV file: " << csvFilename << " initialized" << endl;

		csvFile.open(csvFilename);
		csvFile << "FrameID" << "," << "Timestamp" << "," << "SerialNumber" << "," << "FileNumber" << "," << "SystemTimeInNanoseconds" << endl;
	}
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
	Arena::IImage* pConvert;
	char* imageData0;
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

					char* imageData0 = reinterpret_cast<char*>(const_cast<uint8_t*>(pResultImage0->GetData()));

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
				// monitoring용 이미지는 16bit 그대로 놔둘경우 읽을 때 이상하게 읽힘. 8bit로 교체해서 전달
				pConvert = Arena::ImageFactory::Convert(pResultImage, Mono8);
				monitoringImgL = reinterpret_cast<char*>(const_cast<uint8_t*>(pConvert->GetData()));

				// Acquire the image buffer to write to a file
				imageData0 = reinterpret_cast<char*>(const_cast<uint8_t*>(pResultImage->GetData()));

				// Do the writing to assigned cameraFile
				//cout << pResultImage->GetSizeFilled() << endl;// == 2448*2048 = 5013504
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

cv::Mat ImageRotateInner(const cv::Mat src, double degree
	, cv::Point2f base = cv::Point2f(std::numeric_limits<float>::infinity())) {
	if (base.x == std::numeric_limits<float>::infinity()) {
		base.x = src.cols / 2;
		base.y = src.rows / 2;
	}
	cv::Mat dst = src.clone();
	cv::Mat rot = cv::getRotationMatrix2D(base, degree, 1.0);
	cv::warpAffine(src, dst, rot, src.size());
	return std::move(dst);
}



DWORD WINAPI MonitoringThread(LPVOID lpParam)
{
	std::cout << "MonitoringThread start!" << endl;
	// 리사이즈할 크기 지정
	// 이미지 리사이즈
	cv::Mat resized_image = cv::Mat::zeros(cv::Size(2248 / 2, 2048 / 4), CV_8UC1);
	cv::Mat rotated_image = cv::Mat::zeros(cv::Size(2248 / 2, 2048 / 4), CV_8UC1);
	cv::Mat imgF = cv::Mat::zeros(2248, 2048, CV_8UC1);
	cv::Mat imgL = cv::Mat::zeros(2248, 2048, CV_8UC1);
	Spinnaker::ImagePtr pImageF;
	Arena::IImage* pImageL;
	cv::Mat concatImg = cv::Mat::zeros(cv::Size(2248 * 2, 2048), CV_8UC1);;
	cv::Scalar_<int> blue(255, 0, 0);


	Save::ImageParams params(2448, 2048, Arena::GetBitsPerPixel(Mono8));
	Save::ImageWriter writer(params, "testL.jpg");

	while (AcquireOK)
	{
		if (monitoringImgF != NULL && monitoringImgL != NULL)
		{
			try
			{
				pImageF = Spinnaker::Image::Create(2448, 2048, 0, 0, Spinnaker::PixelFormat_BayerRG16, monitoringImgF);
				pImageF->Save("testF.jpg");

				pImageL = Arena::ImageFactory::Create(reinterpret_cast<uint8_t*>(monitoringImgL), 2448*2048, 2448, 2048, Mono8);

				writer << pImageL->GetData();


				imgF = cv::imread("testF.jpg", 1);
				imgL = cv::imread("testL.jpg", 1);
				if (imgF.empty()) printf("imgF.empty");
				if (imgL.empty()) printf("imgL.empty");

				cv::hconcat(imgL, imgF, concatImg);
				if (concatImg.empty()) printf("concatImg.empty");
				if (resized_image.empty()) printf("resized_image.empty");
				cv::resize(concatImg, resized_image, cv::Size(2248 / 2, 2048 / 4));
				rotated_image = ImageRotateInner(resized_image, 180);
				cv::imshow("img", rotated_image);
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

	try
	{
		// Retrieve camera list size
		camListSizeF = camList.GetSize();
		camListSizeL = deviceInfos.size();

		// Create an array of handles
		Spinnaker::CameraPtr* pCamList = new Spinnaker::CameraPtr[camListSizeF];
		pCamList[0] = camList.GetByIndex(0);
		// Initialize cameras in camList 
		std::cout << "Initializing camera LUCID " << serialNumberL << " and FLIR " << serialNumberF << "... ";
		serialNumberL = "232000061";
		serialNumberF = "22623682";
		string CurrentDateTime = getCurrentDateTime();
		CreateFiles(serialNumberF, 0, CurrentDateTime);
		CreateFiles(serialNumberL, 1, CurrentDateTime);

		// START RECORDING
		std::cout << endl << "*** START RECORDING ***" << endl;

		HANDLE* grabThreads = new HANDLE[camListSizeF + camListSizeL + 1];


		grabThreads[0] = CreateThread(nullptr, 0, AcquireImagesFLIR, &pCamList[0], 0, nullptr); // call AcquireImages in parallel threads
		assert(grabThreads[0] != nullptr);

		grabThreads[1] = CreateThread(nullptr, 0, AcquireImagesLUCID, pDevice, 0, nullptr); // call AcquireImages in parallel threads
		assert(grabThreads[1] != nullptr);

		grabThreads[2] = CreateThread(nullptr, 0, MonitoringThread, nullptr, 0, nullptr);
		assert(grabThreads[2] != nullptr);


		//HANDLE monitoringThread = CreateThread(nullptr, 0, MonitoringThread, nullptr, 0, nullptr);
		// Wait for all threads to finish
		WaitForMultipleObjects(
			camListSizeF + camListSizeL + 1, // number of threads to wait for
			grabThreads, // handles for threads to wait for
			TRUE,        // wait for all of the threads
			INFINITE     // wait forever
		);

		std::cout << endl << "*** CloseHandle  ***" << endl;
		CloseHandle(ghMutex);

		// Check thread return code for each camera
		for (unsigned int i = 0; i < camListSizeF; i++)
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
				cout << i << endl;
				std::cout << "Grab thread for camera at index " << i << " exited with errors." << endl;
				result = -1;
			}
		}


		// Clear CameraPtr array and close all handles
		pCamList[0] = 0;
		for (unsigned int i = 0; i < camListSizeF; i++)
		{
			cout << i << endl;
			CloseHandle(grabThreads[i]);
		}
		//CloseHandle(monitoringThread);
		// Delete array pointer
		delete[] grabThreads;

		// End recording
		std::cout << endl << "*** STOP RECORDING ***" << endl;

		// Clear camera List
		camList.Clear();

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
	/*
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

*/

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
		std::cout << "Done! Press Enter to exit..." << endl;
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

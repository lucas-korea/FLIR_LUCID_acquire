//#include "Spinnaker.h"
//#include "SpinGenApi/SpinnakerGenApi.h"
//#include "stdafx.h"
//#include "ArenaApi.h"
//#include "SaveApi.h"
//#include <iostream>
//#include <sstream>
//#include <fstream>
//#include <vector>
//#include <ctime>
//#include <fstream>
//#include <assert.h>
//#include <time.h>
//#include <cmath>
//#include <chrono>
//#include <algorithm>
//#include <string>
//#include <stdio.h>
//#include <Windows.h>
//#include <opencv2/imgcodecs.hpp>
//#include <opencv2/videoio.hpp>
//#include <opencv2/highgui.hpp>
//#include "opencv2/imgproc/imgproc.hpp"
//
//#ifndef _WIN32
//#include <pthread.h>
//#endif
//
//using namespace std::chrono;
////using namespace Spinnaker;
////using namespace Spinnaker::GenApi;
////using namespace Spinnaker::GenICam;
//using namespace std;
//
//// Initialize config parameters, will be updated by config file
//std::string path;
//std::string triggerCam = "20323052"; // serial number of primary camera
//double exposureTime = 5000.0; // exposure time in microseconds (i.e., 1/ max FPS)
//double FPS = 200.0; // frames per second in Hz
//double compression = 1.0; // compression factor
//int widthToSet;
//int heightToSet;
//double NewFrameRate;
//int numBuffers = 200; // depending on RAM
//
//// Initialize placeholders
//vector<ofstream> cameraFiles;
//ofstream csvFile;
//ofstream metadataFile;
//string metadataFilename;
//int cameraCnt;
//string serialNumberF;
//string serialNumberL;
//// mutex lock for parallel threads
//HANDLE ghMutex;
//
//// Camera trigger type for primary and secondary cameras
//enum triggerType
//{
//	SOFTWARE, // Primary camera
//	HARDWARE // Secondary camera
//};
//
//// Initialization overwritten in InitializeMultipleCameras()
//triggerType chosenTrigger = HARDWARE;
//
///*
//================
//This function reads the config file to update camera parameters such as working directory, serial of primary camera and Framerate
//================
//*/
//
//
//int readconfig()
//{
//	int result = 0;
//
//	std::cout << "Reading config file... ";
//	std::ifstream cFile("myConfig.txt");
//	if (cFile.is_open())
//	{
//
//		std::string line;
//		while (getline(cFile, line))
//		{
//			line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
//			if (line[0] == '#' || line.empty()) continue;
//
//			auto delimiterPos = line.find("=");
//			auto name = line.substr(0, delimiterPos);
//			auto value = line.substr(delimiterPos + 1);
//
//			//Custom coding
//			if (name == "triggerCam") triggerCam = value;
//			else if (name == "FPS") FPS = std::stod(value);
//			else if (name == "compression") compression = std::stod(value);
//			else if (name == "exposureTime") exposureTime = std::stod(value);
//			else if (name == "numBuffers") numBuffers = std::stod(value);
//			else if (name == "path") path = value;
//		}
//	}
//	else
//	{
//		std::cerr << " Couldn't open config file for reading.";
//	}
//
//	std::cout << " Done!" << endl;
//	/*
//	std::cout << "\ntriggerCam=" << triggerCam;
//	std::std::cout << "\nFPS=" << FPS;
//	std::std::cout << "\ncompression=" << compression;
//	std::std::cout << "\nexposureTime=" << exposureTime;
//	std::std::cout << "\nnumBuffers=" << numBuffers;
//	std::std::cout << "\nPath=" << path << endl << endl;
//	*/
//	return result, triggerCam, exposureTime, path, FPS, compression, numBuffers;
//}
//
///*
//=================
//These helper functions getCurrentDateTime and removeSpaces get the system time and transform it to readable format. The output string is used as timestamp for new filenames. The function getTimeStamp prints system writing time in nanoseconds to the csv logfile.
//=================
//*/
//string removeSpaces(string word)
//{
//	string newWord;
//	for (int i = 0; i < word.length(); i++) {
//		if (word[i] != ' ') {
//			newWord += word[i];
//		}
//	}
//	return newWord;
//}
//
//string getCurrentDateTime()
//{
//	stringstream currentDateTime;
//	// current date/time based on system
//	time_t ttNow = time(0);
//	tm* ptmNow;
//	// year
//	ptmNow = localtime(&ttNow);
//	currentDateTime << 1900 + ptmNow->tm_year;
//	//month
//	if (ptmNow->tm_mon < 10) //Fill in the leading 0 if less than 10
//		currentDateTime << "0" << 1 + ptmNow->tm_mon;
//	else
//		currentDateTime << (1 + ptmNow->tm_mon);
//	//day
//	if (ptmNow->tm_mday < 10) //Fill in the leading 0 if less than 10
//		currentDateTime << "0" << ptmNow->tm_mday << " ";
//	else
//		currentDateTime << ptmNow->tm_mday << " ";
//	//spacer
//	currentDateTime << "_";
//	//hour
//	if (ptmNow->tm_hour < 10) //Fill in the leading 0 if less than 10
//		currentDateTime << "0" << ptmNow->tm_hour;
//	else
//		currentDateTime << ptmNow->tm_hour;
//	//min
//	if (ptmNow->tm_min < 10) //Fill in the leading 0 if less than 10
//		currentDateTime << "0" << ptmNow->tm_min;
//	else
//		currentDateTime << ptmNow->tm_min;
//	//sec
//	if (ptmNow->tm_sec < 10) //Fill in the leading 0 if less than 10
//		currentDateTime << "0" << ptmNow->tm_sec;
//	else
//		currentDateTime << ptmNow->tm_sec;
//	string test = currentDateTime.str();
//
//	test = removeSpaces(test);
//
//	return test;
//}
//
//auto getTimeStamp()
//{
//	std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
//	auto duration = now.time_since_epoch();
//
//	typedef std::chrono::duration<int, std::ratio_multiply<std::chrono::hours::period, std::ratio<8>>::type> Days;
//
//	Days days = std::chrono::duration_cast<Days>(duration);
//	duration -= days;
//
//	auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
//
//	auto timestamp = nanoseconds.count();
//	return timestamp;
//}
//
///*
//=================
//The function CreateFiles works within a for loop and creates .tmp binary files for each camera, as well as a single .csv logging sheet csvFile. The files are saved in working irectory.
//=================
//*/
//int CreateFiles(string serialNumber, int cameraCnt, string CurrentDateTime_)
//{
//	int result = 0;
//	//std::cout << endl << "CREATING FILES:..." << endl;
//
//	stringstream sstream_tmpFilename;
//	stringstream sstream_csvFile;
//	stringstream sstream_metadataFile;
//	string tmpFilename;
//	string csvFilename;
//	ofstream filename; // TODO: still needed?
//	const string csDestinationDirectory = path;
//
//	// Create temporary file from serialnum assigned to cameraCnt
//    //sstream_tmpFilename << csDestinationDirectory << CurrentDateTime_ + "_" + serialNumber << "_file" << cameraCnt << ".tmp";
//	sstream_tmpFilename << csDestinationDirectory << CurrentDateTime_ + "_" + serialNumber << ".tmp";
//	sstream_tmpFilename >> tmpFilename;
//
//	//std::cout << "File " << tmpFilename << " initialized" << endl;
//
//	cameraFiles.push_back(std::move(filename)); // TODO: still needed?
//	cameraFiles[cameraCnt].open(tmpFilename.c_str(), ios_base::out | ios_base::binary);
//
//	// Create .csv logfile and .txt metadata only once for all cameras during first loop
//	if (cameraCnt == 0)
//	{
//		// create csv logfile with headers
//		sstream_csvFile << csDestinationDirectory << "logfile_" << CurrentDateTime_ << ".csv";
//		sstream_csvFile >> csvFilename;
//
//		//std::cout << "CSV file: " << csvFilename << " initialized" << endl;
//
//		csvFile.open(csvFilename);
//		csvFile << "FrameID" << "," << "Timestamp" << "," << "SerialNumber" << "," << "FileNumber" << "," << "SystemTimeInNanoseconds" << endl;
//
//		// create txt metadata
//		sstream_metadataFile << csDestinationDirectory << "metadata_" << CurrentDateTime_ << ".txt";
//		sstream_metadataFile >> metadataFilename;
//
//	}
//	return result;
//}
//
//// 
///*
//=================
//The function ResetSettings configures the trigger and exposure settings off to bring the cameras back to its original settings.
//=================
//*/
//
//int ResetSettings(Spinnaker::CameraList camList, Spinnaker::CameraPtr* pCamList, unsigned int camListSize)
//{
//	int result = 0;
//	// TODO: see device reset and factory reset 
//	try
//	{
//		for (unsigned int i = 0; i < camListSize; i++)
//		{
//			// Select camera in loop
//			pCamList[i] = camList.GetByIndex(i);
//			pCamList[i]->Init();
//
//			Spinnaker::GenApi::INodeMap& nodeMap = pCamList[i]->GetNodeMap();
//			Spinnaker::GenApi::CStringPtr ptrStringSerial = pCamList[i]->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");
//
//			if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
//			{
//				serialNumberF = ptrStringSerial->GetValue();
//			}
//
//			std::cout << "Resetting camera " << serialNumberF << "... ";
//
//			// Turn trigger mode back off
//			try
//			{
//				Spinnaker::GenApi::CEnumerationPtr ptrTriggerMode = nodeMap.GetNode("TriggerMode");
//				if (!IsAvailable(ptrTriggerMode) || !IsReadable(ptrTriggerMode))
//				{
//					std::cout << "Unable to disable trigger mode (node retrieval). Non-fatal error..." << endl;
//					return -1;
//				}
//				Spinnaker::GenApi::CEnumEntryPtr ptrTriggerModeOff = ptrTriggerMode->GetEntryByName("Off");
//				if (!IsAvailable(ptrTriggerModeOff) || !IsReadable(ptrTriggerModeOff))
//				{
//					std::cout << "Unable to disable trigger mode (enum entry retrieval). Non-fatal error..." << endl;
//					return -1;
//				}
//				ptrTriggerMode->SetIntValue(ptrTriggerModeOff->GetValue());
//			}
//			catch (Spinnaker::Exception& e)
//			{
//				std::cout << "Unable to reset trigger mode." << endl;
//				result = -1;
//			}
//
//			// Turn automatic exposure back on
//			try
//			{
//				Spinnaker::GenApi::CEnumerationPtr ptrExposureAuto = nodeMap.GetNode("ExposureAuto");
//				if (!IsAvailable(ptrExposureAuto) || !IsWritable(ptrExposureAuto))
//				{
//					std::cout << "Unable to enable automatic exposure (node retrieval). Non-fatal error..." << endl << endl;
//					return -1;
//				}
//				Spinnaker::GenApi::CEnumEntryPtr ptrExposureAutoContinuous = ptrExposureAuto->GetEntryByName("Continuous");
//				if (!IsAvailable(ptrExposureAutoContinuous) || !IsReadable(ptrExposureAutoContinuous))
//				{
//					std::cout << "Unable to enable automatic exposure (enum entry retrieval). Non-fatal error..." << endl << endl;
//					return -1;
//				}
//				ptrExposureAuto->SetIntValue(ptrExposureAutoContinuous->GetValue());
//			}
//			catch (Spinnaker::Exception& e)
//			{
//				std::cout << "Unable to reset automatic exposure." << endl;
//				result = -1;
//			}
//
//			// Disable Aquisition Frame Rate 
//			try
//			{
//				Spinnaker::GenApi::CBooleanPtr ptrAcquisitionFrameRateEnable = nodeMap.GetNode("AcquisitionFrameRateEnable");
//				if (!IsAvailable(ptrAcquisitionFrameRateEnable) || !IsWritable(ptrAcquisitionFrameRateEnable))
//				{
//					// Trying Chameleon3 settings, Chameleon3 node is called Acquisition Frame Rate Control Enabled
//					Spinnaker::GenApi::CBooleanPtr ptrAcquisitionFrameRateControlEnabled = nodeMap.GetNode("AcquisitionFrameRateEnabled");
//					if (!IsAvailable(ptrAcquisitionFrameRateControlEnabled) || !IsWritable(ptrAcquisitionFrameRateControlEnabled))
//					{
//						std::cout << "Unable to set AcquisitionFrameRateControlEnabled. Aborting..." << endl << endl;
//						return -1;
//					}
//					ptrAcquisitionFrameRateControlEnabled->SetValue(false);
//					//std::cout << "Acquisition Frame Rate Control Disabled" << endl;
//				}
//				else
//				{
//					ptrAcquisitionFrameRateEnable->SetValue(false);
//					//std::cout << "Acquisition Frame Rate Disabled" << endl;
//				}
//			}
//			catch (Spinnaker::Exception& e)
//			{
//				std::cout << "Unable to reset acquisition frame rate." << endl;
//				result = -1;
//			}
//
//
//			// TODO: Strobe? Are strobe settings already locked by disabeling the trigger?
//			// TODO: Aquisition mode continuous ? 
//			// TODO: Buffer ? 
//
//			pCamList[i]->DeInit();
//			std::cout << "Done!" << endl;
//		}
//	}
//	catch (Spinnaker::Exception& e)
//	{
//		std::cout << "Unable to reset camera settings. Error: " << e.what() << endl;
//		result = -1;
//	}
//	return result;
//}
//
//
///*
//=================
//width ,height 설정. 불필요한것으로 보임
//The function ImageSettings configures the image compression i.e., the actual image width and height.
//=================
//*/
//int ImageSettings(Spinnaker::GenApi::INodeMap& nodeMap)
//{
//	int result = 0;
//	//std::cout << endl << "CONFIGURING IMAGE SETTINGS:..." << endl;
//	// TODO: update with decimation and or binning instead of compression
//
//	try
//	{
//		// Set image width
//		Spinnaker::GenApi::CIntegerPtr ptrWidth = nodeMap.GetNode("Width");
//		if (IsAvailable(ptrWidth) && IsWritable(ptrWidth))
//		{
//			int width = ptrWidth->GetMax();
//			widthToSet = width / compression;
//			ptrWidth->SetValue(widthToSet);
//		}
//		// Set maximum height
//		Spinnaker::GenApi::CIntegerPtr ptrHeight = nodeMap.GetNode("Height");
//		if (IsAvailable(ptrHeight) && IsWritable(ptrHeight))
//		{
//			int height = ptrHeight->GetMax();
//			heightToSet = height / compression;
//			ptrHeight->SetValue(heightToSet);
//		}
//		//std::cout << "Image setting set to " << ptrWidth->GetValue() << " x " << ptrHeight->GetValue() << "..." << endl;
//	}
//	catch (Spinnaker::Exception& e)
//	{
//		std::cout << "Error: " << e.what() << endl;
//		result = -1;
//	}
//	return result, widthToSet, heightToSet;
//}
//
///*
//=================
//The function ConfigureTrigger turns off trigger mode and then configures the trigger source for each camera. 
//Once the trigger source has been selected, trigger mode is enabled. Note that chosenTrigger=SOFTWARE/HARDWARE is set globally from outside the function. 
//See resources here: https://www.flir.com/support-center/iis/machine-vision/application-note/configuring-synchronized-capture-with-multiple-cameras/
//=================
//*/
//int ConfigureTrigger(Spinnaker::GenApi::INodeMap& nodeMap)
//{
//	int result = 0;
//	//std::cout << endl << "CONFIGURING TRIGGER:..." << endl;
//
//	try
//	{
//		// Ensure trigger mode off
//		Spinnaker::GenApi::CEnumerationPtr ptrTriggerMode = nodeMap.GetNode("TriggerMode");
//		if (!IsAvailable(ptrTriggerMode) || !IsReadable(ptrTriggerMode))
//		{
//			std::cout << "Unable to disable trigger mode (node retrieval). Aborting..." << endl;
//			return -1;
//		}
//		Spinnaker::GenApi::CEnumEntryPtr ptrTriggerModeOff = ptrTriggerMode->GetEntryByName("Off");
//		if (!IsAvailable(ptrTriggerModeOff) || !IsReadable(ptrTriggerModeOff))
//		{
//			std::cout << "Unable to disable trigger mode (enum entry retrieval). Aborting..." << endl;
//			return -1;
//		}
//		ptrTriggerMode->SetIntValue(ptrTriggerModeOff->GetValue());
//		//std::cout << "1. Trigger mode disabled" << endl;
//
//		// Select trigger source
//		Spinnaker::GenApi::CEnumerationPtr ptrTriggerSource = nodeMap.GetNode("TriggerSource");
//		if (!IsAvailable(ptrTriggerSource) || !IsWritable(ptrTriggerSource))
//		{
//			std::cout << "Unable to set trigger mode (node retrieval). Aborting..." << endl;
//			return -1;
//		}
//
//		if (chosenTrigger == SOFTWARE) // this is a primary camera 
//		{
//			// Set trigger mode to software
//			Spinnaker::GenApi::CEnumEntryPtr ptrTriggerSourceSoftware = ptrTriggerSource->GetEntryByName("Software");
//			if (!IsAvailable(ptrTriggerSourceSoftware) || !IsReadable(ptrTriggerSourceSoftware))
//			{
//				std::cout << "Unable to set trigger mode (enum entry retrieval). Aborting..." << endl;
//				return -1;
//			}
//			ptrTriggerSource->SetIntValue(ptrTriggerSourceSoftware->GetValue());
//			//std::cout << "2. Trigger source set to software" << endl;
//
//			// See strobe output lines (as in 4. https://www.flir.com/support-center/iis/machine-vision/application-note/configuring-synchronized-capture-with-multiple-cameras/)
//			// TODO: CM3 settings
//			// Select Line1 from the Line Selection (for BFS camera)
//			Spinnaker::GenApi::CEnumerationPtr ptrLine1Selector = nodeMap.GetNode("LineSelector");
//			if (!IsAvailable(ptrLine1Selector) || !IsWritable(ptrLine1Selector))
//			{
//				std::cout << "Unable to set Lineselector  (node retrieval). Aborting..." << endl << endl;
//				return -1;
//			}
//			Spinnaker::GenApi::CEnumEntryPtr ptrLine1 = ptrLine1Selector->GetEntryByName("Line1");
//			if (!IsAvailable(ptrLine1) || !IsReadable(ptrLine1))
//			{
//				std::cout << "Unable to get Line1  (node retrieval). Aborting..." << endl << endl;
//				return -1;
//			}
//			ptrLine1Selector->SetIntValue(ptrLine1->GetValue());
//			//std::cout << "Selected LineSelector is Line:  " << ptrLineSelector->GetCurrentEntry()->GetSymbolic() << endl;
//
//			// Set Line Mode to output
//			Spinnaker::GenApi::CEnumerationPtr ptrLineMode = nodeMap.GetNode("LineMode");
//			if (!IsAvailable(ptrLineMode) || !IsWritable(ptrLineMode))
//			{
//				std::cout << "Unable to get Line Mode(node retrieval). Aborting..." << endl << endl;
//				return -1;
//			}
//			Spinnaker::GenApi::CEnumEntryPtr ptrOutput = ptrLineMode->GetEntryByName("Output");
//			if (!IsAvailable(ptrOutput) || !IsReadable(ptrOutput))
//			{
//				std::cout << "Unable to get Output  (node retrieval). Aborting..." << endl << endl;
//				return -1;
//			}
//			ptrLineMode->SetIntValue(ptrOutput->GetValue());
//
//			// TODO: Select Line2 and enable 3.3V (for BFS)
//			Spinnaker::GenApi::CEnumerationPtr ptrLine2Selector = nodeMap.GetNode("LineSelector");
//			if (!IsAvailable(ptrLine2Selector) || !IsWritable(ptrLine2Selector))
//			{
//				std::cout << "Unable to set Lineselector  (node retrieval). Aborting..." << endl << endl;
//				return -1;
//			}
//			Spinnaker::GenApi::CEnumEntryPtr ptrLine2 = ptrLine2Selector->GetEntryByName("Line2");
//			if (!IsAvailable(ptrLine2) || !IsReadable(ptrLine2))
//			{
//				std::cout << "Unable to get Line2  (node retrieval). Aborting..." << endl << endl;
//				return -1;
//			}
//			ptrLine2Selector->SetIntValue(ptrLine2->GetValue());
//
//			// Enable 3.3V in Line2
//			Spinnaker::GenApi::CBooleanPtr ptrV3_3Enable = nodeMap.GetNode("V3_3Enable");
//			if (!IsAvailable(ptrV3_3Enable) || !IsWritable(ptrV3_3Enable))
//			{
//				std::cout << "Unable to set AcquisitionFrameRateControlEnabled. Aborting..." << endl << endl;
//				return -1;
//			}
//			ptrV3_3Enable->SetValue(true);
//
//			// Trigger mode not yet activated, triggering initialized with BeginAcquisition
//		}
//		else if (chosenTrigger == HARDWARE)
//		{
//			// Select Line3 as trigger input
//			Spinnaker::GenApi::CEnumEntryPtr ptrTriggerSourceHardware = ptrTriggerSource->GetEntryByName("Line3");
//			if (!IsAvailable(ptrTriggerSourceHardware) || !IsReadable(ptrTriggerSourceHardware))
//			{
//				std::cout << "Unable to set trigger mode (enum entry retrieval). Aborting..." << endl;
//				return -1;
//			}
//			ptrTriggerSource->SetIntValue(ptrTriggerSourceHardware->GetValue());
//			//std::cout << "2. Trigger source set to hardware" << endl;
//
//			// Set TriggerOverlap (mode 0 or mode 14) (see p 24 https://gitlab.ruhr-uni-bochum.de/ikn/syncflir/uploads/b6f7d722be8529b22156a95b160d5ffb/BFS-U3-16S2-Technical-Reference.pdf)
//			Spinnaker::GenApi::CEnumerationPtr ptrTriggerOverlap = nodeMap.GetNode("TriggerOverlap");
//			if (!IsAvailable(ptrTriggerOverlap) || !IsWritable(ptrTriggerOverlap))
//			{
//				std::cout << "Unable to set trigger overlap (node retrieval). Aborting..." << endl;
//				return -1;
//			}
//
//			// Set trigger to read out
//			Spinnaker::GenApi::CEnumEntryPtr ptrTriggerOverlapReadOut = ptrTriggerOverlap->GetEntryByName("ReadOut");
//			if (!IsAvailable(ptrTriggerOverlapReadOut) || !IsReadable(ptrTriggerOverlapReadOut))
//			{
//				std::cout << "Unable to get ReadOut value (node retrieval). Aborting..." << endl << endl;
//				return -1;
//			}
//			ptrTriggerOverlap->SetIntValue(ptrTriggerOverlapReadOut->GetValue());
//
//			// Set TriggerActivation Falling Edge TODO: better Rising Edge? ...
//			Spinnaker::GenApi::CEnumerationPtr ptrTriggerActivation = nodeMap.GetNode("TriggerActivation");
//			if (!IsAvailable(ptrTriggerActivation) || !IsWritable(ptrTriggerActivation))
//			{
//				std::cout << "Unable to set trigger activation (node retrieval). Aborting..." << endl;
//				return -1;
//			}
//			Spinnaker::GenApi::CEnumEntryPtr ptrTriggerActivationFallingEdge = ptrTriggerActivation->GetEntryByName("FallingEdge");
//			if (!IsAvailable(ptrTriggerActivationFallingEdge) || !IsReadable(ptrTriggerActivationFallingEdge))
//			{
//				std::cout << "Unable to get ReadOut value (node retrieval). Aborting..." << endl << endl;
//				return -1;
//			}
//			ptrTriggerActivation->SetIntValue(ptrTriggerActivationFallingEdge->GetValue());
//
//			// Turn trigger on and wait for incoming trigger
//			Spinnaker::GenApi::CEnumEntryPtr ptrTriggerModeOn = ptrTriggerMode->GetEntryByName("On");
//			if (!IsAvailable(ptrTriggerModeOn) || !IsReadable(ptrTriggerModeOn))
//			{
//				std::cout << "Unable to enable trigger mode (enum entry retrieval). Aborting..." << endl;
//				return -1;
//			}
//
//			ptrTriggerMode->SetIntValue(ptrTriggerModeOn->GetValue());
//			//std::cout << "3. Trigger mode activated" << endl;
//		}
//	}
//	catch (Spinnaker::Exception& e)
//	{
//		std::cout << "Error: " << e.what() << endl;
//		result = -1;
//	}
//	return result;
//}
//
///*
//=================
//FLIR 카메라는 ExposureAuto로 설정
//The function ConfigureExposure sets the Exposure setting for the cameras. The Exposure time will affect the overall brightness of the image, but also the framerate. Note that *exposureTime* is set from outside the function. To reach a Frame Rate of 200fps each frame cannot take longer than 1/200 = 5ms or 5000 microseconds.
//=================
//*/
//int ConfigureExposure(Spinnaker::GenApi::INodeMap& nodeMap)
//{
//	int result = 0;
//	//std::cout << endl << "CONFIGURING EXPOSURE:..." << endl;
//
//	try
//	{
//		// Turn off automatic exposure mode
//		Spinnaker::GenApi::CEnumerationPtr ptrExposureAuto = nodeMap.GetNode("ExposureAuto");
//		if (!IsAvailable(ptrExposureAuto) || !IsWritable(ptrExposureAuto))
//		{
//			std::cout << "Unable to disable automatic exposure. Aborting..." << endl << endl;
//			return -1;
//		}
//		Spinnaker::GenApi::CEnumEntryPtr ptrExposureAutoOff = ptrExposureAuto->GetEntryByName("Continuous");
//		if (!IsAvailable(ptrExposureAutoOff) || !IsReadable(ptrExposureAutoOff))
//		{
//			std::cout << "Unable to disable automatic exposure. Aborting..." << endl << endl;
//			return -1;
//		}
//		ptrExposureAuto->SetIntValue(ptrExposureAutoOff->GetValue());
//
//		// Set exposure time manually; exposure time recorded in microseconds
//		Spinnaker::GenApi::CFloatPtr ptrExposureTime = nodeMap.GetNode("ExposureTime");
//		if (!IsAvailable(ptrExposureTime) || !IsWritable(ptrExposureTime))
//		{
//			std::cout << "Unable to set exposure time. Aborting..." << endl << endl;
//			return -1;
//		}
//	}
//	catch (Spinnaker::Exception& e)
//	{
//		std::cout << "Error: " << e.what() << endl;
//		result = -1;
//	}
//	return result;
//}
//
///*
//=================
//The function ConfigureFramerate sets the desired Framerate for the trigger camera.
//=================
//*/
//int ConfigureFramerate(Spinnaker::GenApi::INodeMap& nodeMap)
//{
//	int result = 0;
//	//std::cout << endl << "CONFIGURING FRAMERATE:..." << endl;
//
//	try
//	{
//		//Enable Aquisition Frame Rate 
//		Spinnaker::GenApi::CBooleanPtr ptrAcquisitionFrameRateEnable = nodeMap.GetNode("AcquisitionFrameRateEnable");
//		if (!IsAvailable(ptrAcquisitionFrameRateEnable) || !IsWritable(ptrAcquisitionFrameRateEnable))
//		{
//			// Trying Chameleon3 settings
//			// Set Frame Rate Auto to Off 
//			Spinnaker::GenApi::CEnumerationPtr ptrFrameRateAuto = nodeMap.GetNode("AcquisitionFrameRateAuto");
//			if (!IsAvailable(ptrFrameRateAuto) || !IsWritable(ptrFrameRateAuto))
//			{
//				std::cout << "Unable to set FrameRateAuto. Aborting..." << endl << endl;
//				return -1;
//			}
//			// Retrieve entry node from enumeration node
//			Spinnaker::GenApi::CEnumEntryPtr ptrFrameRateAutoOff = ptrFrameRateAuto->GetEntryByName("Off");
//			if (!IsAvailable(ptrFrameRateAutoOff) || !IsReadable(ptrFrameRateAutoOff))
//			{
//				std::cout << "Unable to set Frame Rate to Off. Aborting..." << endl << endl;
//				return -1;
//			}
//			int64_t framerateAutoOff = ptrFrameRateAutoOff->GetValue();
//
//			ptrFrameRateAuto->SetIntValue(framerateAutoOff);
//			//std::cout << "Frame Rate Auto set to Off..." << endl;
//
//			// Chameleon3 node is called Acquisition Frame Rate Control Enabled
//			Spinnaker::GenApi::CBooleanPtr ptrAcquisitionFrameRateControlEnabled = nodeMap.GetNode("AcquisitionFrameRateEnabled");
//			if (!IsAvailable(ptrAcquisitionFrameRateControlEnabled) || !IsWritable(ptrAcquisitionFrameRateControlEnabled))
//			{
//				std::cout << "Unable to set AcquisitionFrameRateControlEnabled. Aborting..." << endl << endl;
//				return -1;
//			}
//			ptrAcquisitionFrameRateControlEnabled->SetValue(true);
//			//std::cout << "Acquisition Frame Rate Control Enabled" << endl;
//		}
//		else
//		{
//			ptrAcquisitionFrameRateEnable->SetValue(true);
//			//std::cout << "Acquisition Frame Rate Enabled" << endl;
//		}
//
//		// Set Maximum Acquisition FrameRate
//		Spinnaker::GenApi::CFloatPtr ptrAcquisitionFrameRate = nodeMap.GetNode("AcquisitionFrameRate");
//		if (!IsAvailable(ptrAcquisitionFrameRate) || !IsReadable(ptrAcquisitionFrameRate))
//		{
//			std::cout << "Unable to get node AcquisitionFrameRate. Aborting..." << endl << endl;
//			return -1;
//		}
//		ptrAcquisitionFrameRate->SetValue(ptrAcquisitionFrameRate->GetMax());
//
//		// Desired frame rate from config 
//		double FPSToSet = FPS;
//
//		// Maximum Acquisition Frame Rate
//		double testAcqFrameRate = ptrAcquisitionFrameRate->GetValue();
//		//std::cout << "Maximum Acquisition Frame Rate is  : " << ptrAcquisitionFrameRate->GetMax() << endl;
//
//		// Maximum Resulting Frame Rate bounded by 1/Exposure
//		double testResultingAcqFrameRate = 1000000 / exposureTime; // exposure in microseconds (10^-6)
//
//
//		// If desired FPS too high, find next best available
//		if (FPSToSet > testResultingAcqFrameRate || FPSToSet > testAcqFrameRate)
//		{
//			// take the lowest bounded frame rate
//			if (testResultingAcqFrameRate > testAcqFrameRate)
//			{
//				FPSToSet = testAcqFrameRate;
//			}
//			else
//			{
//				FPSToSet = testResultingAcqFrameRate;
//			}
//		}
//		// if FPS not too high, go ahead
//		ptrAcquisitionFrameRate->SetValue(FPSToSet);
//	}
//	catch (Spinnaker::Exception& e)
//	{
//		std::cout << "Error: " << e.what() << endl;
//		result = -1;
//	}
//	return result;
//}
//
//
///*
//=================
//FLIR 카메라는 가시광이니까 BayerRG8로 세팅
//=================
//*/
//int ConfigurePixelFormat(Spinnaker::GenApi::INodeMap& nodeMap)
//{
//	Spinnaker::GenApi::CEnumerationPtr ptrPixelFormat = nodeMap.GetNode("PixelFormat");
//	if (IsAvailable(ptrPixelFormat) && IsWritable(ptrPixelFormat))
//	{
//		// Retrieve the desired entry node from the enumeration node
//		Spinnaker::GenApi::CEnumEntryPtr ptrPixelFormatBayerRG8 = ptrPixelFormat->GetEntryByName("BayerRG8");
//		if (IsAvailable(ptrPixelFormatBayerRG8) && IsReadable(ptrPixelFormatBayerRG8))
//		{
//			// Retrieve the integer value from the entry node
//			const int64_t pixelFormatBayerRG8 = ptrPixelFormatBayerRG8->GetValue();
//
//			// Set integer as new value for enumeration node
//			ptrPixelFormat->SetIntValue(pixelFormatBayerRG8);
//			//isPixelFormatColor = true;
//		}
//		else
//		{
//			// Methods in the ImageUtilityPolarization class are supported for images of
//			// polarized pixel formats only.
//			std::cout << "Pixel format  " << ptrPixelFormatBayerRG8->GetSymbolic() << "  can not available(entry retrieval).Aborting..." << endl;
//			return -1;
//		}
//	}
//}
///*
//=================
//The function BufferHandlingSettings sets manual buffer handling mode to numBuffers set above.
//=================
//*/
//int BufferHandlingSettings(Spinnaker::CameraPtr pCam)
//{
//	int result = 0;
//	//cout << endl << "CONFIGURING BUFFER..." << endl;
//
//	// Retrieve Stream Parameters device nodemap
//	Spinnaker::GenApi::INodeMap& sNodeMap = pCam->GetTLStreamNodeMap();
//
//	// Retrieve Buffer Handling Mode Information
//	Spinnaker::GenApi::CEnumerationPtr ptrHandlingMode = sNodeMap.GetNode("StreamBufferHandlingMode");
//	if (!IsAvailable(ptrHandlingMode) || !IsWritable(ptrHandlingMode))
//	{
//		std::cout << "Unable to set Buffer Handling mode (node retrieval). Aborting..." << endl << endl;
//		return -1;
//	}
//
//	Spinnaker::GenApi::CEnumEntryPtr ptrHandlingModeEntry = ptrHandlingMode->GetCurrentEntry();
//	if (!IsAvailable(ptrHandlingModeEntry) || !IsReadable(ptrHandlingModeEntry))
//	{
//		std::cout << "Unable to set Buffer Handling mode (Entry retrieval). Aborting..." << endl << endl;
//		return -1;
//	}
//
//	// Set stream buffer Count Mode to manual
//	Spinnaker::GenApi::CEnumerationPtr ptrStreamBufferCountMode = sNodeMap.GetNode("StreamBufferCountMode");
//	if (!IsAvailable(ptrStreamBufferCountMode) || !IsWritable(ptrStreamBufferCountMode))
//	{
//		std::cout << "Unable to set Buffer Count Mode (node retrieval). Aborting..." << endl << endl;
//		return -1;
//	}
//
//	Spinnaker::GenApi::CEnumEntryPtr ptrStreamBufferCountModeManual = ptrStreamBufferCountMode->GetEntryByName("Manual");
//	if (!IsAvailable(ptrStreamBufferCountModeManual) || !IsReadable(ptrStreamBufferCountModeManual))
//	{
//		std::cout << "Unable to set Buffer Count Mode entry (Entry retrieval). Aborting..." << endl << endl;
//		return -1;
//	}
//	ptrStreamBufferCountMode->SetIntValue(ptrStreamBufferCountModeManual->GetValue());
//
//	// Retrieve and modify Stream Buffer Count
//	Spinnaker::GenApi::CIntegerPtr ptrBufferCount = sNodeMap.GetNode("StreamBufferCountManual");
//	if (!IsAvailable(ptrBufferCount) || !IsWritable(ptrBufferCount))
//	{
//		std::cout << "Unable to set Buffer Count (Integer node retrieval). Aborting..." << endl << endl;
//		return -1;
//	}
//
//	// Display Buffer Info
//	//std::cout << "Stream Buffer Count Mode set to manual" << endl;
//	//std::cout << "Default Buffer Count: " << ptrBufferCount->GetValue() << endl;
//	//std::cout << "Maximum Buffer Count: " << ptrBufferCount->GetMax() << endl;
//	if (ptrBufferCount->GetMax() < numBuffers)
//	{
//		ptrBufferCount->SetValue(ptrBufferCount->GetMax());
//	}
//	else
//	{
//		ptrBufferCount->SetValue(numBuffers);
//	}
//
//	//std::cout << "Manual Buffer Count: " << ptrBufferCount->GetValue() << endl;
//	ptrHandlingModeEntry = ptrHandlingMode->GetEntryByName("OldestFirst");
//	ptrHandlingMode->SetIntValue(ptrHandlingModeEntry->GetValue());
//	//std::cout << "Buffer Handling Mode has been set to " << ptrHandlingModeEntry->GetDisplayName() << endl;
//
//	return result;
//}
//
///*
//=================
//The function CleanBuffer runs on all cameras in hardware trigger mode before the actual recording. If the buffer contains images, these are released in the for loop. When the buffer is empty, GetNextImage will get timeout error and end the loop.
//=================
//*/
//int CleanBuffer(Spinnaker::CameraPtr pCam)
//{
//	int result = 0;
//
//	// Clean Buffer acquiring idle images
//	pCam->BeginAcquisition();
//
//	try
//	{
//		for (unsigned int imagesInBuffer = 0; imagesInBuffer < numBuffers; imagesInBuffer++)
//		{
//			// first numBuffer images are descarted
//			Spinnaker::ImagePtr pResultImage = pCam->GetNextImage(100);
//			char* imageData = static_cast<char*>(pResultImage->GetData());
//			pResultImage->Release();
//		}
//	}
//	catch (Spinnaker::Exception& e)
//	{
//		//std::cout << "Buffer clean. " << endl;
//	}
//
//	pCam->EndAcquisition();
//
//	return result;
//}
//
///*
//=================
//The function AcquireImages runs in parallel threads and grabs images from each camera and saves them in the corresponding binary file. Each image also records the image status to the logging csvFile.
//=================
//*/
//char* monitoringImgL = NULL;
//char* monitoringImgF = NULL;
//bool AcquireOK = FALSE;
//DWORD WINAPI AcquireImagesFLIR(LPVOID lpParam)
//{
//	cout << "FLIR thread start" << endl;
//	AcquireOK = TRUE;
//	// START function in UN-locked thread
//
//	// Initialize camera
//	Spinnaker::CameraPtr pCam = *((Spinnaker::CameraPtr*)lpParam);
//	pCam->Init();
//
//	// Start actual image acquisition
//	pCam->BeginAcquisition();
//
//	// Initialize empty parameters outside of locked case
//	Spinnaker::ImagePtr pResultImage;
//	char* imageData; // 모니터링을 위해 밖으로 빼고 싶은데, thread에 필요한 거라서 안될듯..
//	string deviceUser_ID;
//	int firstFrame = 1;
//	int stopwait = 0;
//
//	// Retrieve and save images in while loop until manual ESC press
//
//	printf("Running...\n");
//	while (GetAsyncKeyState(VK_ESCAPE) == 0)
//	{
//
//		// Start mutex_lock
//		DWORD dwCount = 0, dwWaitResult;
//		dwWaitResult = WaitForSingleObject(
//			ghMutex,    // handle to mutex
//			INFINITE);  // no time-out interval
//		switch (dwWaitResult)
//		{
//
//			// The thread got ownership of the mutex
//		case WAIT_OBJECT_0:
//			try
//			{
//				// Identify specific camera in locked thread
//				serialNumberF = "22623682";
//				cameraCnt = 0;
//
//				// For first loop in while ...
//				if (firstFrame == 1)
//				{
//					// first image is descarted
//					Spinnaker::ImagePtr pResultImage = pCam->GetNextImage();
//					char* imageData = static_cast<char*>(pResultImage->GetData());
//					pResultImage->Release();
//
//					// anounce start recording
//					std::cout << "Camera [" << serialNumberF << "] " << "Started recording with ID [" << cameraCnt << " ]..." << endl;
//				}
//				firstFrame = 0;
//
//				// Retrieve image and ensure image completion
//				try
//				{
//					pResultImage = pCam->GetNextImage(); // waiting time for NextImage in miliseconds, never time out.
//				}
//				catch (Spinnaker::Exception& e)
//				{
//					// Break recording loop after waiting 1000ms without trigger
//					stopwait = 1;
//				}
//
//				// Break recording loop when stopwait is activated
//				if (stopwait == 1)
//				{
//					std::cout << "Trigger signal disconnected. Stop recording for camera [" << serialNumberF << "]" << endl;
//					// End acquisition
//					pCam->EndAcquisition();
//					cameraFiles[cameraCnt].close();
//					csvFile.close();
//					// Deinitialize camera
//					pCam->DeInit();
//
//					break;
//				}
//
//				// Acquire the image buffer to write to a file
//				// GetData()가 두번 연속 나오는건 문제가 있어보인다.
//				imageData = static_cast<char*>(pResultImage->GetData());
//				monitoringImgF = imageData;
//
//				// Do the writing to assigned cameraFile
//				cameraFiles[cameraCnt].write(imageData, pResultImage->GetImageSize());
//				//cout << pResultImage->GetImageSize() << endl;
//				csvFile << pResultImage->GetFrameID() << "," << pResultImage->GetTimeStamp() << "," << serialNumberF << "," << cameraCnt << "," << getTimeStamp() << endl;
//
//				// Check if the writing is successful
//				if (!cameraFiles[cameraCnt].good())
//				{
//					std::cout << "Error writing to file for camera " << cameraCnt << " !" << endl;
//					return -1;
//				}
//
//				// Release image
//				pResultImage->Release();
//			}
//
//			catch (Spinnaker::Exception& e)
//			{
//				std::cout << "Error: " << e.what() << endl;
//				return -1;
//			}
//
//			// normal break
//			ReleaseMutex(ghMutex);
//			break;
//
//			// The thread got ownership of an abandoned mutex
//		case WAIT_ABANDONED:
//			std::cout << "wait abandoned" << endl; //TODO what is this for?
//		}
//	}
//	// End acquisition
//	pCam->EndAcquisition();
//	cameraFiles[cameraCnt].close();
//	csvFile.close();
//
//	// Deinitialize camera
//	pCam->DeInit();
//	std::cout << "End FLIR camera!!" << endl;
//	AcquireOK = FALSE;
//	return 1;
//}
//
//DWORD WINAPI AcquireImagesLUCID(LPVOID lpParam)
//{
//	cout << "LUCID thread start!" << endl;
//	AcquireOK = TRUE;
//	// START function in UN-locked thread
//	// Initialize camera
//	Arena::IDevice* pDevice = (Arena::IDevice*)lpParam;
//	// Start actual image acquisition
//	pDevice->StartStream();
//	// Initialize empty parameters outside of locked case
//	Arena::IImage* pResultImage;
//	//char* imageData; // 모니터링을 위해 밖으로 빼고 싶은데, thread에 필요한 거라서 안될듯..
//	char* imageData0;
//	//string deviceUser_ID;
//	int firstFrame = 1;
//	int stopwait = 0;
//
//	// Retrieve and save images in while loop until manual ESC press
//	printf("Running...\n");
//	// Start mutex_lock
//	while (GetAsyncKeyState(VK_ESCAPE) == 0)
//	{
//		DWORD dwCount = 0, dwWaitResult;
//		dwWaitResult = WaitForSingleObject(
//			ghMutex,    // handle to mutex
//			INFINITE);  // no time-out interval
//		switch (dwWaitResult)
//		{
//
//			// The thread got ownership of the mutex
//		case WAIT_OBJECT_0:
//			try
//			{
//				// Identify specific camera in locked thread
//				cameraCnt = 1;
//				serialNumberL = "232000061";
//				// For first loop in while ...
//				if (firstFrame == 1)
//				{
//					// first image is descarted
//
//					Arena::IImage* pResultImage0 = pDevice->GetImage(2000);
//
//					//const uint8_t* pInput = pResultImage0->GetData();
//					//const uint8_t* pIn = pInput;
//					char* imageData0 = reinterpret_cast<char*>(const_cast<uint8_t*>(pResultImage0->GetData()));
//
//					//Save::ImageParams params(2448, 2048, Arena::GetBitsPerPixel(Mono8));
//					//Save::ImageWriter writer(params, "testL.jpg");
//					//int imageSize = 2448 * 2048;
//					//Arena::IImage* pimage = Arena::ImageFactory::Create(reinterpret_cast<uint8_t*>(imageData0), imageSize, 2448, 2048, Mono8);
//					//writer << pimage->GetData();
//
//					delete[] pResultImage0;
//
//					// anounce start recording
//					std::cout << "Camera [" << serialNumberL << "] " << "Started recording with ID [" << cameraCnt << " ]..." << endl;
//				}
//				firstFrame = 0;
//
//				// Retrieve image and ensure image completion
//				try
//				{
//					pResultImage = pDevice->GetImage(2000);// waiting time for NextImage in 2000 miliseconds.
//				}
//				catch (Spinnaker::Exception& e)
//				{
//					// Break recording loop after waiting 1000ms without trigger
//					stopwait = 1;
//				}
//
//				// Break recording loop when stopwait is activated
//				if (stopwait == 1)
//				{
//					std::cout << "Trigger signal disconnected. Stop recording for camera [" << serialNumberL << "]" << endl;
//					// End acquisition
//					pDevice->StopStream();
//					cameraFiles[cameraCnt].close();
//					csvFile.close();
//					// Deinitialize camera
//					// LUCID는 RunMultipleCameras 에서 DestroyDevice가 이루어짐
//
//					break;
//				}
//
//				//imageData = static_cast<char*>(pResultImage->GetData());
//				imageData0 = reinterpret_cast<char*>(const_cast<uint8_t*>(pResultImage->GetData()));
//				// Acquire the image buffer to write to a file
//				//monitoringImgL = pResultImage->GetData();
//				monitoringImgL = imageData0;
//
//				// Do the writing to assigned cameraFile
//				//cout << pResultImage->GetSizeFilled() << endl; == 2448*2048 = 5013504
//				cameraFiles[cameraCnt].write(imageData0, pResultImage->GetSizeFilled());
//				csvFile << pResultImage->GetFrameId() << "," << pResultImage->GetTimestamp() << "," << serialNumberL << "," << cameraCnt << "," << getTimeStamp() << endl;
//
//				// Check if the writing is successful
//				if (!cameraFiles[cameraCnt].good())
//				{
//					std::cout << "Error writing to file for camera " << cameraCnt << " !" << endl;
//					return -1;
//				}
//
//				// Release image
//				pDevice->RequeueBuffer(pResultImage);
//			}
//
//			catch (Spinnaker::Exception& e)
//			{
//				std::cout << "Error: " << e.what() << endl;
//				return -1;
//			}
//
//			// normal break
//			ReleaseMutex(ghMutex);
//			break;
//
//			// The thread got ownership of an abandoned mutex
//		case WAIT_ABANDONED:
//			std::cout << "wait abandoned" << endl; //TODO what is this for?
//		}
//	}
//	// End acquisition
//	pDevice->StopStream();
//	//cameraFiles[cameraCnt].close();
//	//csvFile.close();
//
//	// Deinitialize camera
//	// LUCID는 RunMultipleCameras 에서 DestroyDevice가 이루어짐
//
//	std::cout << "End LUCID camera!!" << endl;
//	//AcquireOK = FALSE;
//	return 1;
//}
//
//DWORD WINAPI MonitoringThread(LPVOID lpParam)
//{
//	std::cout << "MonitoringThread start!" << endl;
//	// 리사이즈할 크기 지정
//	// 이미지 리사이즈
//	cv::Mat resized_image = cv::Mat::zeros(cv::Size(2248 / 2, 2048 / 4), CV_8UC1);
//	cv::Mat imgF = cv::Mat::zeros(2248, 2048, CV_8UC1);
//	cv::Mat imgL = cv::Mat::zeros(2248, 2048, CV_8UC1);
//	Spinnaker::ImagePtr pImageF;
//	Arena::IImage* pImageL;
//	cv::Mat concatImg = cv::Mat::zeros(cv::Size(2248 * 2, 2048), CV_8UC1);;
//	cv::Scalar_<int> blue(255, 0, 0);
//	string ExposureTimeMonitor = "exposure time : " + to_string(exposureTime);
//	string FrameMonitor = "Frame rate : " + to_string(FPS);
//
//	Save::ImageParams params(2448,2048, Arena::GetBitsPerPixel(Mono8));
//	Save::ImageWriter writer(params, "testL.jpg");
//	while (AcquireOK)
//	{
//		if ( monitoringImgF != NULL && monitoringImgL != NULL)
//		{
//			//pImage1 = Spinnaker::Image::Create(2448, 2048, 0, 0, Spinnaker::PixelFormat_BayerRGPolarized8, monitoringImgL);
//			pImageF = Spinnaker::Image::Create(2448, 2048, 0, 0, Spinnaker::PixelFormat_BayerRG8, monitoringImgF);
//			pImageF->Save("testF.jpg");
//			//reinterpret_cast<char*>(const_cast<uint8_t*>(pResultImage->GetData()))
//			//cout << sizeof(*&monitoringImgL) << endl;
//			//cout << sizeof(*reinterpret_cast<uint8_t*>(monitoringImgL)) << endl;
//			//cout << 2448 * 2048 * Arena::GetBitsPerPixel(Mono8) / 8 << endl;
//
//			pImageL = Arena::ImageFactory::Create(reinterpret_cast<uint8_t*>(monitoringImgL), 2448 * 2048 * Arena::GetBitsPerPixel(Mono8) / 8, 2448, 2048, Mono8);
//			writer << pImageL->GetData();
//
//
//			imgF = cv::imread("testF.jpg", 1);
//			imgL = cv::imread("testL.jpg", 1);
//			if (imgF.empty()) printf("imgF.empty");
//			if (imgL.empty()) printf("imgL.empty");
//
//			cv::hconcat( imgL, imgF, concatImg);
//			if (concatImg.empty()) printf("concatImg.empty");
//			if (resized_image.empty()) printf("resized_image.empty");
//			cv::resize(concatImg, resized_image, cv::Size(2248 / 2, 2048 / 4));
//			cv::putText(resized_image, ExposureTimeMonitor, cv::Point(0,30), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, 8);
//			cv::putText(resized_image, FrameMonitor, cv::Point(0, 60), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, 8);
//			cv::imshow("img", resized_image);
//			//Mat img = imread("test.jpg", 1);
//			//imshow("img", img);
//			
//		}
//		int keycode = cv::waitKey(5);
//		
//	}
//	cv::destroyAllWindows();
//	std::cout << "MonitoringThread done!" << endl;
//	return 0;
//}
//
///*
//=================
//The function InitializeMultipleCameras bundles the initialization process for all cameras on the system. It is called in RunMultipleCameras and starts a loop to set Buffer, Strobe, Exposure and Trigger, as well as to create binary files for each camera.
//=================
//*/
//int InitializeMultipleCameras(Spinnaker::CameraList camList, Spinnaker::CameraPtr* pCamList, Arena::IDevice* pDevice, std::vector<Arena::DeviceInfo> deviceInfos, unsigned int camListSize, unsigned int camListSize2)
//{
//	int result = 0;
//	string CurrentDateTime = getCurrentDateTime();
//	try
//	{
//		for (unsigned int i = 0; i < camListSize; i++)
//		{
//			// Select camera in loop
//			pCamList[i] = camList.GetByIndex(i); // TODO: try to get order of USB Interface/primary vs secondary // get by serial?
//			pCamList[i]->Init();
//			//Arena::IDevice* pDevice = pSystem->CreateDevice(deviceInfos[0]);
//
//			Spinnaker::GenApi::INodeMap& nodeMapF = pCamList[i]->GetNodeMap();
//			Spinnaker::GenApi::CStringPtr ptrStringSerialF = pCamList[i]->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");
//			GenApi_3_3_LUCID::INodeMap* nodeMapL = pDevice->GetNodeMap();
//			GenApi_3_3_LUCID::CStringPtr ptrStringSerialL = pDevice->GetTLDeviceNodeMap()->GetNode("DeviceSerialNumber");
//
//			serialNumberL = ptrStringSerialL->GetValue();
//			serialNumberF = ptrStringSerialF->GetValue();
//
//			cout << serialNumberL << '\t' << serialNumberF << endl;
//
//			// Set DeviceUserID to loop counter to assign camera order to cameraFiles in parallel threads
//			Spinnaker::GenApi::CStringPtr ptrDeviceUserId = nodeMapF.GetNode("DeviceUserID");
//
//			std::cout << "Initializing camera LUCID " << serialNumberL << " and FLIR " << serialNumberF << "... ";
//
//			Arena::SetNodeValue<GenICam::gcstring>(
//				pDevice->GetNodeMap(),
//				"AcquisitionMode",
//				"Continuous");
//
//			// getting the buffer handling mode to 'NewestOnly' ensures the most recent
//			// image is delivered, even if it means skipping frames.
//			Arena::SetNodeValue<GenICam::gcstring>(
//				pDevice->GetTLStreamNodeMap(),
//				"StreamBufferHandlingMode",
//				"NewestOnly");
//
//			// enable stream auto negotiate packet size
//			Arena::SetNodeValue<bool>(
//				pDevice->GetTLStreamNodeMap(),
//				"StreamAutoNegotiatePacketSize",
//				true);
//
//			// enable stream packet resend
//			Arena::SetNodeValue<bool>(
//				pDevice->GetTLStreamNodeMap(),
//				"StreamPacketResendEnable",
//				true);
//
//
//			// Set Buffer
//			BufferHandlingSettings(pCamList[i]);
//
//			// Set Gain
//			Spinnaker::GenApi::CEnumerationPtr ptrGainAuto = nodeMapF.GetNode("GainAuto");
//			Spinnaker::GenApi::CEnumEntryPtr ptrGainAutoContinuous = ptrGainAuto->GetEntryByName("Continuous");
//			ptrGainAuto->SetIntValue(ptrGainAutoContinuous->GetValue());
//			
//			Arena::SetNodeValue<GenICam::gcstring>(
//				pDevice->GetNodeMap(),
//				"GainAuto",
//				"Continuous");
//
//			// Set white balance
//			Spinnaker::GenApi::CEnumerationPtr ptrBalanceWhiteAuto = nodeMapF.GetNode("BalanceWhiteAuto");
//			Spinnaker::GenApi::CEnumEntryPtr ptrBalanceWhiteAutoContinuous = ptrBalanceWhiteAuto->GetEntryByName("Continuous");
//			ptrBalanceWhiteAuto->SetIntValue(ptrBalanceWhiteAutoContinuous->GetValue());
//			
//			Arena::SetNodeValue<GenICam::gcstring>(
//				pDevice->GetNodeMap(),
//				"BalanceWhiteAuto",
//				"Continuous");
//			Arena::SetNodeValue< bool >(
//				pDevice->GetNodeMap(),
//				"BalanceWhiteEnable",
//				TRUE);
//
//			// Set Exposure
//			ConfigureExposure(nodeMapF);
//
//			Arena::SetNodeValue<GenICam::gcstring>(
//				pDevice->GetNodeMap(),
//				"ExposureAuto",
//				"Continuous");
//			// Set Image Settings
//			ImageSettings(nodeMapF);
//
//			// Start all cameras in hardware trigger mode 
//			chosenTrigger = HARDWARE;
//			//ConfigureTrigger(nodeMap);
//
//			// Set pixelForamt
//			ConfigurePixelFormat(nodeMapF);
//
//			Arena::SetNodeValue<GenICam::gcstring>(
//				pDevice->GetNodeMap(),
//				"PixelFormat",
//				"PolarizeMono8");
//
//			// Clean buffer
//			CleanBuffer(pCamList[i]);
//
//			// Create binary files for each camera and overall .csv logfile
//			CreateFiles(serialNumberF, 0, CurrentDateTime);
//			CreateFiles(serialNumberL, 1, CurrentDateTime);
//
//			// For trigger camera:
//			if (serialNumberF == triggerCam)
//			{
//				// Configure Trigger
//				chosenTrigger = SOFTWARE;
//				ConfigureTrigger(nodeMapF);
//
//				// Set Framerate
//				ConfigureFramerate(nodeMapF);
//
//				// read framerate for output
//				Spinnaker::GenApi::CFloatPtr ptrAcquisitionFrameRate = nodeMapF.GetNode("AcquisitionFrameRate");
//				if (!IsAvailable(ptrAcquisitionFrameRate) || !IsReadable(ptrAcquisitionFrameRate))
//				{
//					std::cout << "Unable to get node AcquisitionFrameRate. Aborting..." << endl << endl;
//					return -1;
//				}
//				NewFrameRate = ptrAcquisitionFrameRate->GetValue();
//				std::cout << "set as trigger with " << NewFrameRate << "Hz... ";
//			}
//			pCamList[i]->DeInit();
//			std::cout << "Done!" << endl;
//		}
//	}
//	catch (Spinnaker::Exception& e)
//	{
//		std::cout << "Error: " << e.what() << endl;
//		result = -1;
//	}
//	return result;
//}
//
///*
//=================
//The function RunMultipleCameras acts as the body of the program. It calls the InitializeMultipleCameras function to configure all cameras, then creates parallel threads and starts AcquireImages on each thred. With the setting configured above, threads are waiting for synchronized trigger as a central clock.
//=================
//*/
//int RunMultipleCameras(Spinnaker::CameraList camList, std::vector<Arena::DeviceInfo> deviceInfos, Arena::IDevice* pDevice)
//{
//	int result = 0;
//	unsigned int camListSizeF = 0;
//	unsigned int camListSizeL = 0;
//
//	try
//	{
//		// Retrieve camera list size
//		camListSizeF = camList.GetSize();
//		camListSizeL = deviceInfos.size();
//
//		// Create an array of handles
//		Spinnaker::CameraPtr* pCamList = new Spinnaker::CameraPtr[camListSizeF];
//
//		// Initialize cameras in camList 
//		result = InitializeMultipleCameras(camList, pCamList, pDevice, deviceInfos, camListSizeF, camListSizeL);
//
//		// START RECORDING
//		std::cout << endl << "*** START RECORDING ***" << endl;
//
//		HANDLE* grabThreads = new HANDLE[camListSizeF + camListSizeL + 1];
//
//
//		grabThreads[0] = CreateThread(nullptr, 0, AcquireImagesFLIR, &pCamList[0], 0, nullptr); // call AcquireImages in parallel threads
//		assert(grabThreads[0] != nullptr);
//
//		grabThreads[1] = CreateThread(nullptr, 0, AcquireImagesLUCID, pDevice, 0, nullptr); // call AcquireImages in parallel threads
//		assert(grabThreads[1] != nullptr);
//
//		grabThreads[2] = CreateThread(nullptr, 0, MonitoringThread, nullptr, 0, nullptr);
//		assert(grabThreads[2] != nullptr);
//
//
//		//HANDLE monitoringThread = CreateThread(nullptr, 0, MonitoringThread, nullptr, 0, nullptr);
//		// Wait for all threads to finish
//		WaitForMultipleObjects(
//			camListSizeF + camListSizeL + 1, // number of threads to wait for
//			grabThreads, // handles for threads to wait for
//			TRUE,        // wait for all of the threads
//			INFINITE     // wait forever
//		);
//
//		std::cout << endl << "*** CloseHandle  ***" << endl;
//		CloseHandle(ghMutex);
//
//		// Check thread return code for each camera
//		for (unsigned int i = 0; i < camListSizeF; i++)
//		{
//			DWORD exitcode;
//			BOOL rc = GetExitCodeThread(grabThreads[i], &exitcode);
//			if (!rc)
//			{
//				cout << i << endl;
//				std::cout << "Handle error from GetExitCodeThread() returned for camera at index " << i << endl;
//				result = -1;
//			}
//			else if (!exitcode)
//			{
//				cout << i << endl;
//				std::cout << "Grab thread for camera at index " << i << " exited with errors." << endl;
//				result = -1;
//			}
//		}
//
//		// Clear CameraPtr array and close all handles
//		for (unsigned int i = 0; i < camListSizeF; i++)
//		{
//			pCamList[i] = 0;
//			CloseHandle(grabThreads[i]);
//		}
//		//CloseHandle(monitoringThread);
//		// Delete array pointer
//		delete[] grabThreads;
//		//delete monitoringThread;
//
//		// End recording
//		std::cout << endl << "*** STOP RECORDING ***" << endl;
//
//		// Reset Cameras
//		result = ResetSettings(camList, pCamList, camListSizeF);
//
//		// Clear camera List
//		camList.Clear();
//
//		// Delete array pointer
//		delete[] pCamList;
//
//	}
//	catch (Spinnaker::Exception& e)
//	{
//		std::cout << "Error: " << e.what() << endl;
//		result = -1;
//	}
//	return result;
//}
//
//
///*
//=================
//This is the Entry point for the program. After testing writing priviledges it gets all camera instances and calls RunMultipleCameras.
//=================
//*/
//int main(int /*argc*/, char** /*argv*/)
//{
//	// get terminal window width and adapt string length?
//	// Print application build information
//	std::cout << "***********************************************************************" << endl;
//	std::cout << "		Application build date: " << __DATE__ << " " << __TIME__ << endl;
//	std::cout << "	MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com" << endl;
//	std::cout << "***********************************************************************" << endl;
//
//	// Testing ascii logo print
//	// source: https://patorjk.com/software/taag/#p=display&h=3&v=3&f=Big%20Money-sw&t=syncFLIR%0A
//	std::cout << R"(
//                                     ________ __       ______ _______  
//                                    /        /  |     /      /       \ 
//  _______ __    __ _______   _______$$$$$$$$/$$ |     $$$$$$/$$$$$$$  |
// /       /  |  /  /       \ /       $$ |__   $$ |       $$ | $$ |__$$ |
///$$$$$$$/$$ |  $$ $$$$$$$  /$$$$$$$/$$    |  $$ |       $$ | $$    $$< 
//$$      \$$ |  $$ $$ |  $$ $$ |     $$$$$/   $$ |       $$ | $$$$$$$  |
// $$$$$$  $$ \__$$ $$ |  $$ $$ \_____$$ |     $$ |_____ _$$ |_$$ |  $$ |
///     $$/$$    $$ $$ |  $$ $$       $$ |     $$       / $$   $$ |  $$ |
//$$$$$$$/  $$$$$$$ $$/   $$/ $$$$$$$/$$/      $$$$$$$$/$$$$$$/$$/   $$/ 
//         /  \__$$ |                                                    
//         $$    $$/                                                     
//          $$$$$$/  
//
//)";
//
//	int result = 0;
//
//	// Test permission to write to directory
//	stringstream testfilestream;
//	string testfilestring;
//	testfilestream << "test.txt";
//	testfilestream >> testfilestring;
//	const char* testfile = testfilestring.c_str();
//
//	FILE* tempFile = fopen(testfile, "w+");
//	if (tempFile == nullptr)
//	{
//		std::cout << "Failed to create file in current folder.  Please check permissions." << endl;
//		std::cout << "Press Enter to exit..." << endl;
//		getchar();
//		return -1;
//	}
//
//	fclose(tempFile);
//	remove(testfile);
//
//
//	// Retrieve singleton reference to system object
//	Spinnaker::SystemPtr system = Spinnaker::System::GetInstance();
//	Arena::ISystem* pSystemL = Arena::OpenSystem();
//
//	// Retrieve list of cameras from the system
//	Spinnaker::CameraList camList = system->GetCameras();
//	unsigned int numCamerasF = camList.GetSize();
//
//	pSystemL->UpdateDevices(100);
//	std::vector<Arena::DeviceInfo> deviceInfos = pSystemL->GetDevices();
//	unsigned int numCamerasL = deviceInfos.size();
//	Arena::IDevice* pDevice = pSystemL->CreateDevice(deviceInfos[0]);
//
//	std::cout << "	Number of cameras detected: " << numCamerasF+ numCamerasL << endl << endl;
//
//	// Finish if there are no cameras
//	if (numCamerasL + numCamerasF != 2)
//	{
//		// Clear camera list before releasing system
//		deviceInfos.clear();
//		camList.Clear();
//
//		// Release system
//		system->ReleaseInstance();
//		if (numCamerasF == 0)std::cout << "No FLIR cameras connected!" << endl;
//		if (numCamerasF == 0)std::cout << "No LUCID cameras connected!" << endl;
//		std::cout << "Done! Press Enter to exit..." << endl;
//		getchar();
//
//		return -1;
//	}
//
//	// Read config file and update parameters
//	readconfig();
//
//	// Create mutex to lock parallel threads
//	ghMutex = CreateMutex(
//		NULL,              // default security attributes
//		FALSE,             // initially not owned
//		NULL);             // unnamed mutex
//
//	if (ghMutex == NULL)
//	{
//		printf("CreateMutex error: %d\n", GetLastError());
//		return -1;
//	}
//
//	// Run all cameras
//	result = RunMultipleCameras(camList, deviceInfos, pDevice);
//
//	// Save metadata with recording settings and placeholders for BINtoAVI
//	metadataFile.open(metadataFilename);
//	metadataFile << "# This is the metadata summarizing recording parameters:" << endl;
//	metadataFile << "# 1) Change ColorVideo = 1/0, chosenVideoType =UNCOMPRESSED/MJPG/H264 and VideoPath" << endl;
//	metadataFile << "# 2) For MJPG use quality index from 1 to 100, for H264 use bitrates from 1e6 to 100e6 bps" << endl;
//	metadataFile << "# 3) Set maximum video size and max available RAM in GB" << endl;
//	metadataFile << "# 4) Set directory for converted videos" << endl;
//	metadataFile << "Filename=" << metadataFilename << endl;
//	metadataFile << "Framerate=" << NewFrameRate << endl;
//	metadataFile << "ImageHeight=" << heightToSet << endl;
//	metadataFile << "ImageWidth=" << widthToSet << endl;
//	metadataFile << "ColorVideo=1" << endl;
//	metadataFile << "chosenVideoType=H264" << endl;
//	metadataFile << "h264bitrate=30000000" << endl;
//	metadataFile << "mjpgquality=90" << endl;
//	metadataFile << "maxVideoSize=4" << endl;
//	metadataFile << "maxRAM=10" << endl;
//	metadataFile << "VideoPath=" << path << endl;
//
//	std::cout << "Metadata file: " << metadataFilename << " saved!" << endl;
//
//	// Clear camera list before releasing system
//	camList.Clear();
//
//	// Release system
//	system->ReleaseInstance();
//	pSystemL->DestroyDevice(pDevice);
//	// Testing ascii logo print
//	// source: https://patorjk.com/software/taag/#p=display&h=3&v=3&f=Big%20Money-sw&t=syncFLIR%0A
//	std::cout << "						Press Enter to exit..." << endl;
//	// Print application build information
//	std::cout << "***********************************************************************" << endl;
//	std::cout << "		Application build date: " << __DATE__ << " " << __TIME__ << endl;
//	std::cout << "	MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com" << endl;
//	std::cout << "***********************************************************************" << endl;
//	getchar();
//
//	return result;
//}
//
//int main2()
//{
//	const string filename = "C:\\Users\\jcy37\\source\\repos\\Project2\\data\\20230615_145114_232000061.tmp";
//	ifstream rawFile(filename, ios_base::in | ios_base::binary);
//	Save::ImageParams params(2448, 2048, Arena::GetBitsPerPixel(Mono8));
//	Save::ImageWriter writer(params, "testL.jpg");
//	if (!rawFile)
//	{
//		cout << "Error opening file: " << filename << " Aborting..." << endl;
//
//		return -1;
//	}
//	
//	if (!rawFile)
//	{
//		cout << "Error opening file: " << filename << " Aborting..." << endl;
//
//		return -1;
//	}
//
//	int imageSize = 2448 * 2048;
//	char* imageBuffer = new char[imageSize];
//	char* imageBuffer_dump = new char[imageSize];
//	Arena::IImage* pimage;
//	rawFile.read(imageBuffer, imageSize);
//	memcpy(imageBuffer_dump, imageBuffer, imageSize);
//	if (imageBuffer  != NULL)
//	{
//		 pimage = Arena::ImageFactory::Create(reinterpret_cast<const uint8_t*>(imageBuffer_dump), imageSize, 2448, 2048, Mono8);
//		 getchar();
//		 return 0;
//	}
//
//	writer << pimage->GetData();
//	//delete[] imageBuffer;
//	//pimage->GetData();
//	//rawFile.close();
//	std::cout << "						Press Enter to exit..." << endl;
//	getchar();
//	return 0;
//}
//
//void main_cv()
//{
//	int i, j;
//	unsigned char OrgImg[2448][2048];
//
//	FILE* infile = fopen("C:\\Users\\jcy37\\source\\repos\\Project2\\data\\20230615_145114_232000061.tmp", "rb");
//	if (infile == NULL) { printf("File open error!!"); return; }
//	fread(OrgImg, sizeof(char), 2448 * 2048, infile);
//	fclose(infile);
//
//	//for (i = 0; i < 2448; i++)
//	//{
//	//	for (j = 0; j < 2048; j++)
//	//	{
//	//		OrgImg[i][j] = 255 - OrgImg[i][j];
//	//	}
//	//}
//
//	//FILE* outfile = fopen("coin_inv.raw", "wb");
//	//fwrite(OrgImg, sizeof(char), 2448 * 2048, outfile);
//	//fclose(outfile);
//	std::cout << "						Press Enter to exit..." << endl;
//	// Print application build information
//	std::cout << "***********************************************************************" << endl;
//	std::cout << "		Application build date: " << __DATE__ << " " << __TIME__ << endl;
//	std::cout << "	MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com" << endl;
//	std::cout << "***********************************************************************" << endl;
//	getchar();
//}
//
//
//int main_FLIRtest(int /*argc*/, char** /*argv*/)
//{
//	string filenames = "C:\\Users\\jcy37\\source\\repos\\Project2\\data\\20230615_144521_22623682.tmp";
//
//	int numFiles = 1;
//	cout << numFiles << endl;
//	string num = to_string(numFiles);
//	cout << endl << "Converting BINtoAVI for 1 files..." << endl;
//	cout << filenames << endl;
//
//
//	cout << endl << "Press Enter to convert files" << endl << endl;
//	getchar(); // pass filenames vector to RetrieveImagesFromFiles
//
//	int result = 0;
//	int imageSize = 2448*2048;
//
//	ifstream rawFile(filenames, ios_base::in | ios_base::binary);
//	if (!rawFile)
//	{
//		cout << "Error opening file: " << filenames << " Aborting..." << endl;
//
//		return -1;
//	}
//
//	Spinnaker::ImagePtr myImage;
//	int frameCnt = 0;
//	string videoFilename = "FLIR_test_";
//	char* imageBuffer = new char[imageSize];
//	rawFile.read(imageBuffer, imageSize);
//	// Import binary image into Mono8 Image structure
//	Spinnaker::ImagePtr pImage = Spinnaker::Image::Create(2448, 2048, 0, 0, Spinnaker::PixelFormat_BayerRG8, imageBuffer);
//
//	myImage = pImage->Convert(Spinnaker::PixelFormat_BayerRG8, Spinnaker::HQ_LINEAR);
//	myImage->Save((videoFilename + to_string(frameCnt) + "_new.jpg").c_str());
//	cout << "Image " << videoFilename << to_string(frameCnt) << "_new.jpg" << " saved!" << endl;
//
//	// Delete the acquired buffer
//	delete[] imageBuffer;
//
//	rawFile.close();
//
//	getchar();
//
//	return 0;
//}
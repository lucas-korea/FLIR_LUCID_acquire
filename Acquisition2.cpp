/*
====================================================================================================
TODO:
This script was developed with close assistance by the FLIR Systems Support Team, and is adapted
from examples in FLIR Systems/Spinnaker/src. Copyright from FLIR Integrated Imaging Solutions, Inc. applies.
This program initializes and configures FLIR cameras in a hardware trigger setup in wich the primary camera
triggers all other secondary cameras. Image acquisition runs in parallel threads, locked during grabbing
and writing, and saves data to binary files to save time in the between frames intervals.
The .tmp output file has to be converted to AVI with a different code TMPtoAVI.
Note that exposureTime and triggerCam serial number are configures from the myconfig.txt and need to be adapted
before start recording. The binary files get very large, make sure to change save path to SSD directory with
> 1TB (1TB = +- 500000 frames) and high writing speed (1.6MB * 100fps * 3cameras = 480bps).
Install Spinnaker SDK before using this script.

MIT License Copyright (c) 2021 GuillermoHidalgoGadea.com
Sourcecode: https://github.com/Guillermo-Hidalgo-Gadea/syncFLIR

version: v1.1 29.November 2021
====================================================================================================
*/

/*
####################################################################################################
TODO: next releases:
- update header, gitlab migration and copyright
- check for unnecessary variables and libraries
- check ResetSettings function and factory reset
- check trigger strobe settings for CM3
- try to get order of USB Interface/primary vs secondary / get by serial?
- use decimation or binning instead of size compression (http://softwareservices.flir.com/BFS-U3-89S6/latest/Model/public/ImageFormatControl.html)
- ...
####################################################################################################
*/

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
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

#include <Windows.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include "opencv2/imgproc/imgproc.hpp"

#ifndef _WIN32
#include <pthread.h>
#endif

using namespace std::chrono;
using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
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
string serialNumber;

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

// 
/*
=================
The function ResetSettings configures the trigger and exposure settings off to bring the cameras back to its original settings.
=================
*/

int ResetSettings(CameraList camList, CameraPtr* pCamList, unsigned int camListSize)
{
	int result = 0;
	// TODO: see device reset and factory reset 
	try
	{
		for (unsigned int i = 0; i < camListSize; i++)
		{
			// Select camera in loop
			pCamList[i] = camList.GetByIndex(i);
			pCamList[i]->Init();

			INodeMap& nodeMap = pCamList[i]->GetNodeMap();
			CStringPtr ptrStringSerial = pCamList[i]->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");

			if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
			{
				serialNumber = ptrStringSerial->GetValue();
			}

			std::cout << "Resetting camera " << serialNumber << "... ";

			// Turn trigger mode back off
			try
			{
				CEnumerationPtr ptrTriggerMode = nodeMap.GetNode("TriggerMode");
				if (!IsAvailable(ptrTriggerMode) || !IsReadable(ptrTriggerMode))
				{
					std::cout << "Unable to disable trigger mode (node retrieval). Non-fatal error..." << endl;
					return -1;
				}
				CEnumEntryPtr ptrTriggerModeOff = ptrTriggerMode->GetEntryByName("Off");
				if (!IsAvailable(ptrTriggerModeOff) || !IsReadable(ptrTriggerModeOff))
				{
					std::cout << "Unable to disable trigger mode (enum entry retrieval). Non-fatal error..." << endl;
					return -1;
				}
				ptrTriggerMode->SetIntValue(ptrTriggerModeOff->GetValue());
			}
			catch (Spinnaker::Exception& e)
			{
				std::cout << "Unable to reset trigger mode." << endl;
				result = -1;
			}

			// Turn automatic exposure back on
			try
			{
				CEnumerationPtr ptrExposureAuto = nodeMap.GetNode("ExposureAuto");
				if (!IsAvailable(ptrExposureAuto) || !IsWritable(ptrExposureAuto))
				{
					std::cout << "Unable to enable automatic exposure (node retrieval). Non-fatal error..." << endl << endl;
					return -1;
				}
				CEnumEntryPtr ptrExposureAutoContinuous = ptrExposureAuto->GetEntryByName("Continuous");
				if (!IsAvailable(ptrExposureAutoContinuous) || !IsReadable(ptrExposureAutoContinuous))
				{
					std::cout << "Unable to enable automatic exposure (enum entry retrieval). Non-fatal error..." << endl << endl;
					return -1;
				}
				ptrExposureAuto->SetIntValue(ptrExposureAutoContinuous->GetValue());
			}
			catch (Spinnaker::Exception& e)
			{
				std::cout << "Unable to reset automatic exposure." << endl;
				result = -1;
			}

			// Disable Aquisition Frame Rate 
			try
			{
				CBooleanPtr ptrAcquisitionFrameRateEnable = nodeMap.GetNode("AcquisitionFrameRateEnable");
				if (!IsAvailable(ptrAcquisitionFrameRateEnable) || !IsWritable(ptrAcquisitionFrameRateEnable))
				{
					// Trying Chameleon3 settings, Chameleon3 node is called Acquisition Frame Rate Control Enabled
					CBooleanPtr ptrAcquisitionFrameRateControlEnabled = nodeMap.GetNode("AcquisitionFrameRateEnabled");
					if (!IsAvailable(ptrAcquisitionFrameRateControlEnabled) || !IsWritable(ptrAcquisitionFrameRateControlEnabled))
					{
						std::cout << "Unable to set AcquisitionFrameRateControlEnabled. Aborting..." << endl << endl;
						return -1;
					}
					ptrAcquisitionFrameRateControlEnabled->SetValue(false);
					//std::cout << "Acquisition Frame Rate Control Disabled" << endl;
				}
				else
				{
					ptrAcquisitionFrameRateEnable->SetValue(false);
					//std::cout << "Acquisition Frame Rate Disabled" << endl;
				}
			}
			catch (Spinnaker::Exception& e)
			{
				std::cout << "Unable to reset acquisition frame rate." << endl;
				result = -1;
			}


			// TODO: Strobe? Are strobe settings already locked by disabeling the trigger?
			// TODO: Aquisition mode continuous ? 
			// TODO: Buffer ? 

			pCamList[i]->DeInit();
			std::cout << "Done!" << endl;
		}
	}
	catch (Spinnaker::Exception& e)
	{
		std::cout << "Unable to reset camera settings. Error: " << e.what() << endl;
		result = -1;
	}
	return result;
}


/*
=================
The function ImageSettings configures the image compression i.e., the actual image width and height.
=================
*/
int ImageSettings(INodeMap& nodeMap)
{
	int result = 0;
	//std::cout << endl << "CONFIGURING IMAGE SETTINGS:..." << endl;
	// TODO: update with decimation and or binning instead of compression

	try
	{
		// Set image width
		CIntegerPtr ptrWidth = nodeMap.GetNode("Width");
		if (IsAvailable(ptrWidth) && IsWritable(ptrWidth))
		{
			int width = ptrWidth->GetMax();
			widthToSet = width / compression;
			ptrWidth->SetValue(widthToSet);
		}
		// Set maximum height
		CIntegerPtr ptrHeight = nodeMap.GetNode("Height");
		if (IsAvailable(ptrHeight) && IsWritable(ptrHeight))
		{
			int height = ptrHeight->GetMax();
			heightToSet = height / compression;
			ptrHeight->SetValue(heightToSet);
		}
		//std::cout << "Image setting set to " << ptrWidth->GetValue() << " x " << ptrHeight->GetValue() << "..." << endl;
	}
	catch (Spinnaker::Exception& e)
	{
		std::cout << "Error: " << e.what() << endl;
		result = -1;
	}
	return result, widthToSet, heightToSet;
}

/*
=================
The function ConfigureTrigger turns off trigger mode and then configures the trigger source for each camera. 
Once the trigger source has been selected, trigger mode is enabled. Note that chosenTrigger=SOFTWARE/HARDWARE is set globally from outside the function. 
See resources here: https://www.flir.com/support-center/iis/machine-vision/application-note/configuring-synchronized-capture-with-multiple-cameras/
=================
*/
int ConfigureTrigger(INodeMap& nodeMap)
{
	int result = 0;
	//std::cout << endl << "CONFIGURING TRIGGER:..." << endl;

	try
	{
		// Ensure trigger mode off
		CEnumerationPtr ptrTriggerMode = nodeMap.GetNode("TriggerMode");
		if (!IsAvailable(ptrTriggerMode) || !IsReadable(ptrTriggerMode))
		{
			std::cout << "Unable to disable trigger mode (node retrieval). Aborting..." << endl;
			return -1;
		}
		CEnumEntryPtr ptrTriggerModeOff = ptrTriggerMode->GetEntryByName("Off");
		if (!IsAvailable(ptrTriggerModeOff) || !IsReadable(ptrTriggerModeOff))
		{
			std::cout << "Unable to disable trigger mode (enum entry retrieval). Aborting..." << endl;
			return -1;
		}
		ptrTriggerMode->SetIntValue(ptrTriggerModeOff->GetValue());
		//std::cout << "1. Trigger mode disabled" << endl;

		// Select trigger source
		CEnumerationPtr ptrTriggerSource = nodeMap.GetNode("TriggerSource");
		if (!IsAvailable(ptrTriggerSource) || !IsWritable(ptrTriggerSource))
		{
			std::cout << "Unable to set trigger mode (node retrieval). Aborting..." << endl;
			return -1;
		}

		if (chosenTrigger == SOFTWARE) // this is a primary camera 
		{
			// Set trigger mode to software
			CEnumEntryPtr ptrTriggerSourceSoftware = ptrTriggerSource->GetEntryByName("Software");
			if (!IsAvailable(ptrTriggerSourceSoftware) || !IsReadable(ptrTriggerSourceSoftware))
			{
				std::cout << "Unable to set trigger mode (enum entry retrieval). Aborting..." << endl;
				return -1;
			}
			ptrTriggerSource->SetIntValue(ptrTriggerSourceSoftware->GetValue());
			//std::cout << "2. Trigger source set to software" << endl;

			// See strobe output lines (as in 4. https://www.flir.com/support-center/iis/machine-vision/application-note/configuring-synchronized-capture-with-multiple-cameras/)
			// TODO: CM3 settings
			// Select Line1 from the Line Selection (for BFS camera)
			CEnumerationPtr ptrLine1Selector = nodeMap.GetNode("LineSelector");
			if (!IsAvailable(ptrLine1Selector) || !IsWritable(ptrLine1Selector))
			{
				std::cout << "Unable to set Lineselector  (node retrieval). Aborting..." << endl << endl;
				return -1;
			}
			CEnumEntryPtr ptrLine1 = ptrLine1Selector->GetEntryByName("Line1");
			if (!IsAvailable(ptrLine1) || !IsReadable(ptrLine1))
			{
				std::cout << "Unable to get Line1  (node retrieval). Aborting..." << endl << endl;
				return -1;
			}
			ptrLine1Selector->SetIntValue(ptrLine1->GetValue());
			//std::cout << "Selected LineSelector is Line:  " << ptrLineSelector->GetCurrentEntry()->GetSymbolic() << endl;

			// Set Line Mode to output
			CEnumerationPtr ptrLineMode = nodeMap.GetNode("LineMode");
			if (!IsAvailable(ptrLineMode) || !IsWritable(ptrLineMode))
			{
				std::cout << "Unable to get Line Mode(node retrieval). Aborting..." << endl << endl;
				return -1;
			}
			CEnumEntryPtr ptrOutput = ptrLineMode->GetEntryByName("Output");
			if (!IsAvailable(ptrOutput) || !IsReadable(ptrOutput))
			{
				std::cout << "Unable to get Output  (node retrieval). Aborting..." << endl << endl;
				return -1;
			}
			ptrLineMode->SetIntValue(ptrOutput->GetValue());

			// TODO: Select Line2 and enable 3.3V (for BFS)
			CEnumerationPtr ptrLine2Selector = nodeMap.GetNode("LineSelector");
			if (!IsAvailable(ptrLine2Selector) || !IsWritable(ptrLine2Selector))
			{
				std::cout << "Unable to set Lineselector  (node retrieval). Aborting..." << endl << endl;
				return -1;
			}
			CEnumEntryPtr ptrLine2 = ptrLine2Selector->GetEntryByName("Line2");
			if (!IsAvailable(ptrLine2) || !IsReadable(ptrLine2))
			{
				std::cout << "Unable to get Line2  (node retrieval). Aborting..." << endl << endl;
				return -1;
			}
			ptrLine2Selector->SetIntValue(ptrLine2->GetValue());

			// Enable 3.3V in Line2
			CBooleanPtr ptrV3_3Enable = nodeMap.GetNode("V3_3Enable");
			if (!IsAvailable(ptrV3_3Enable) || !IsWritable(ptrV3_3Enable))
			{
				std::cout << "Unable to set AcquisitionFrameRateControlEnabled. Aborting..." << endl << endl;
				return -1;
			}
			ptrV3_3Enable->SetValue(true);

			// Trigger mode not yet activated, triggering initialized with BeginAcquisition
		}
		else if (chosenTrigger == HARDWARE)
		{
			// Select Line3 as trigger input
			CEnumEntryPtr ptrTriggerSourceHardware = ptrTriggerSource->GetEntryByName("Line3");
			if (!IsAvailable(ptrTriggerSourceHardware) || !IsReadable(ptrTriggerSourceHardware))
			{
				std::cout << "Unable to set trigger mode (enum entry retrieval). Aborting..." << endl;
				return -1;
			}
			ptrTriggerSource->SetIntValue(ptrTriggerSourceHardware->GetValue());
			//std::cout << "2. Trigger source set to hardware" << endl;

			// Set TriggerOverlap (mode 0 or mode 14) (see p 24 https://gitlab.ruhr-uni-bochum.de/ikn/syncflir/uploads/b6f7d722be8529b22156a95b160d5ffb/BFS-U3-16S2-Technical-Reference.pdf)
			CEnumerationPtr ptrTriggerOverlap = nodeMap.GetNode("TriggerOverlap");
			if (!IsAvailable(ptrTriggerOverlap) || !IsWritable(ptrTriggerOverlap))
			{
				std::cout << "Unable to set trigger overlap (node retrieval). Aborting..." << endl;
				return -1;
			}

			// Set trigger to read out
			CEnumEntryPtr ptrTriggerOverlapReadOut = ptrTriggerOverlap->GetEntryByName("ReadOut");
			if (!IsAvailable(ptrTriggerOverlapReadOut) || !IsReadable(ptrTriggerOverlapReadOut))
			{
				std::cout << "Unable to get ReadOut value (node retrieval). Aborting..." << endl << endl;
				return -1;
			}
			ptrTriggerOverlap->SetIntValue(ptrTriggerOverlapReadOut->GetValue());

			// Set TriggerActivation Falling Edge TODO: better Rising Edge? ...
			CEnumerationPtr ptrTriggerActivation = nodeMap.GetNode("TriggerActivation");
			if (!IsAvailable(ptrTriggerActivation) || !IsWritable(ptrTriggerActivation))
			{
				std::cout << "Unable to set trigger activation (node retrieval). Aborting..." << endl;
				return -1;
			}
			CEnumEntryPtr ptrTriggerActivationFallingEdge = ptrTriggerActivation->GetEntryByName("FallingEdge");
			if (!IsAvailable(ptrTriggerActivationFallingEdge) || !IsReadable(ptrTriggerActivationFallingEdge))
			{
				std::cout << "Unable to get ReadOut value (node retrieval). Aborting..." << endl << endl;
				return -1;
			}
			ptrTriggerActivation->SetIntValue(ptrTriggerActivationFallingEdge->GetValue());

			// Turn trigger on and wait for incoming trigger
			CEnumEntryPtr ptrTriggerModeOn = ptrTriggerMode->GetEntryByName("On");
			if (!IsAvailable(ptrTriggerModeOn) || !IsReadable(ptrTriggerModeOn))
			{
				std::cout << "Unable to enable trigger mode (enum entry retrieval). Aborting..." << endl;
				return -1;
			}
			ptrTriggerMode->SetIntValue(ptrTriggerModeOn->GetValue());
			//std::cout << "3. Trigger mode activated" << endl;
		}
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
The function ConfigureExposure sets the Exposure setting for the cameras. The Exposure time will affect the overall brightness of the image, but also the framerate. Note that *exposureTime* is set from outside the function. To reach a Frame Rate of 200fps each frame cannot take longer than 1/200 = 5ms or 5000 microseconds.
=================
*/
int ConfigureExposure(INodeMap& nodeMap)
{
	int result = 0;
	//std::cout << endl << "CONFIGURING EXPOSURE:..." << endl;

	try
	{
		// Turn off automatic exposure mode
		CEnumerationPtr ptrExposureAuto = nodeMap.GetNode("ExposureAuto");
		if (!IsAvailable(ptrExposureAuto) || !IsWritable(ptrExposureAuto))
		{
			std::cout << "Unable to disable automatic exposure. Aborting..." << endl << endl;
			return -1;
		}
		CEnumEntryPtr ptrExposureAutoOff = ptrExposureAuto->GetEntryByName("Off");
		if (!IsAvailable(ptrExposureAutoOff) || !IsReadable(ptrExposureAutoOff))
		{
			std::cout << "Unable to disable automatic exposure. Aborting..." << endl << endl;
			return -1;
		}
		ptrExposureAuto->SetIntValue(ptrExposureAutoOff->GetValue());

		// Set exposure time manually; exposure time recorded in microseconds
		CFloatPtr ptrExposureTime = nodeMap.GetNode("ExposureTime");
		if (!IsAvailable(ptrExposureTime) || !IsWritable(ptrExposureTime))
		{
			std::cout << "Unable to set exposure time. Aborting..." << endl << endl;
			return -1;
		}

		// Desired exposure from config

		double exposureTimeToSet = exposureTime; // Exposure time will limit FPS by 1000000/exposure

		if (serialNumber == "21159646")
		{
			exposureTimeToSet = exposureTime;
		}
		else
		{
			exposureTimeToSet = exposureTime / 3 - 10;
		}
		// Ensure desired exposure time does not exceed the maximum or minimum
		const double exposureTimeMax = ptrExposureTime->GetMax();
		const double exposureTimeMin = ptrExposureTime->GetMin();
		//std::cout << "exposureTimeMax: " << exposureTimeMax << endl;

		if (exposureTimeToSet > exposureTimeMax || exposureTimeToSet < exposureTimeMin)
		{
			if (exposureTimeToSet > exposureTimeMax)
			{
				exposureTimeToSet = exposureTimeMax;
			}
			else
			{
				exposureTimeToSet = exposureTimeMin;
			}
		}
		ptrExposureTime->SetValue(exposureTimeToSet);
		//std::cout << std::fixed << "Exposure time set to " << exposureTimeToSet << " microseconds, resulting in " << 1000000 / exposureTimeToSet << "Hz" << endl;
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
The function ConfigureFramerate sets the desired Framerate for the trigger camera.
=================
*/
int ConfigureFramerate(INodeMap& nodeMap)
{
	int result = 0;
	//std::cout << endl << "CONFIGURING FRAMERATE:..." << endl;

	try
	{
		//Enable Aquisition Frame Rate 
		CBooleanPtr ptrAcquisitionFrameRateEnable = nodeMap.GetNode("AcquisitionFrameRateEnable");
		if (!IsAvailable(ptrAcquisitionFrameRateEnable) || !IsWritable(ptrAcquisitionFrameRateEnable))
		{
			// Trying Chameleon3 settings
			// Set Frame Rate Auto to Off 
			CEnumerationPtr ptrFrameRateAuto = nodeMap.GetNode("AcquisitionFrameRateAuto");
			if (!IsAvailable(ptrFrameRateAuto) || !IsWritable(ptrFrameRateAuto))
			{
				std::cout << "Unable to set FrameRateAuto. Aborting..." << endl << endl;
				return -1;
			}
			// Retrieve entry node from enumeration node
			CEnumEntryPtr ptrFrameRateAutoOff = ptrFrameRateAuto->GetEntryByName("Off");
			if (!IsAvailable(ptrFrameRateAutoOff) || !IsReadable(ptrFrameRateAutoOff))
			{
				std::cout << "Unable to set Frame Rate to Off. Aborting..." << endl << endl;
				return -1;
			}
			int64_t framerateAutoOff = ptrFrameRateAutoOff->GetValue();

			ptrFrameRateAuto->SetIntValue(framerateAutoOff);
			//std::cout << "Frame Rate Auto set to Off..." << endl;

			// Chameleon3 node is called Acquisition Frame Rate Control Enabled
			CBooleanPtr ptrAcquisitionFrameRateControlEnabled = nodeMap.GetNode("AcquisitionFrameRateEnabled");
			if (!IsAvailable(ptrAcquisitionFrameRateControlEnabled) || !IsWritable(ptrAcquisitionFrameRateControlEnabled))
			{
				std::cout << "Unable to set AcquisitionFrameRateControlEnabled. Aborting..." << endl << endl;
				return -1;
			}
			ptrAcquisitionFrameRateControlEnabled->SetValue(true);
			//std::cout << "Acquisition Frame Rate Control Enabled" << endl;
		}
		else
		{
			ptrAcquisitionFrameRateEnable->SetValue(true);
			//std::cout << "Acquisition Frame Rate Enabled" << endl;
		}

		// Set Maximum Acquisition FrameRate
		CFloatPtr ptrAcquisitionFrameRate = nodeMap.GetNode("AcquisitionFrameRate");
		if (!IsAvailable(ptrAcquisitionFrameRate) || !IsReadable(ptrAcquisitionFrameRate))
		{
			std::cout << "Unable to get node AcquisitionFrameRate. Aborting..." << endl << endl;
			return -1;
		}
		ptrAcquisitionFrameRate->SetValue(ptrAcquisitionFrameRate->GetMax());

		// Desired frame rate from config 
		double FPSToSet = FPS;

		// Maximum Acquisition Frame Rate
		double testAcqFrameRate = ptrAcquisitionFrameRate->GetValue();
		//std::cout << "Maximum Acquisition Frame Rate is  : " << ptrAcquisitionFrameRate->GetMax() << endl;

		// Maximum Resulting Frame Rate bounded by 1/Exposure
		double testResultingAcqFrameRate = 1000000 / exposureTime; // exposure in microseconds (10^-6)


		// If desired FPS too high, find next best available
		if (FPSToSet > testResultingAcqFrameRate || FPSToSet > testAcqFrameRate)
		{
			// take the lowest bounded frame rate
			if (testResultingAcqFrameRate > testAcqFrameRate)
			{
				FPSToSet = testAcqFrameRate;
			}
			else
			{
				FPSToSet = testResultingAcqFrameRate;
			}
		}
		// if FPS not too high, go ahead
		ptrAcquisitionFrameRate->SetValue(FPSToSet);
	}
	catch (Spinnaker::Exception& e)
	{
		std::cout << "Error: " << e.what() << endl;
		result = -1;
	}
	return result;
}


int ConfigurePixelFormat(INodeMap& nodeMap)
{
	if (serialNumber == "21159646")
	{
		CEnumerationPtr ptrPixelFormat = nodeMap.GetNode("PixelFormat");
		if (IsAvailable(ptrPixelFormat) && IsWritable(ptrPixelFormat))
		{
			// Retrieve the desired entry node from the enumeration node
			CEnumEntryPtr ptrPixelFormatBayerRGPolarized8 = ptrPixelFormat->GetEntryByName("BayerRGPolarized8");
			if (IsAvailable(ptrPixelFormatBayerRGPolarized8) && IsReadable(ptrPixelFormatBayerRGPolarized8))
			{
				// Retrieve the integer value from the entry node
				const int64_t pixelFormatBayerRGPolarized8 = ptrPixelFormatBayerRGPolarized8->GetValue();

				// Set integer as new value for enumeration node
				ptrPixelFormat->SetIntValue(pixelFormatBayerRGPolarized8);
				//isPixelFormatColor = true;
			}
			else
			{
				// Methods in the ImageUtilityPolarization class are supported for images of
				// polarized pixel formats only.
				std::cout << "Pixel format  " << ptrPixelFormatBayerRGPolarized8->GetSymbolic() << "  can not available(entry retrieval).Aborting..." << endl;
				return -1;
			}
		}
	}
	else
	{
		CEnumerationPtr ptrPixelFormat = nodeMap.GetNode("PixelFormat");
		if (IsAvailable(ptrPixelFormat) && IsWritable(ptrPixelFormat))
		{
			// Retrieve the desired entry node from the enumeration node
			CEnumEntryPtr ptrPixelFormatBayerRG8 = ptrPixelFormat->GetEntryByName("BayerRG8");
			if (IsAvailable(ptrPixelFormatBayerRG8) && IsReadable(ptrPixelFormatBayerRG8))
			{
				// Retrieve the integer value from the entry node
				const int64_t pixelFormatBayerRG8 = ptrPixelFormatBayerRG8->GetValue();

				// Set integer as new value for enumeration node
				ptrPixelFormat->SetIntValue(pixelFormatBayerRG8);
				//isPixelFormatColor = true;
			}
			else
			{
				// Methods in the ImageUtilityPolarization class are supported for images of
				// polarized pixel formats only.
				std::cout << "Pixel format  " << ptrPixelFormatBayerRG8->GetSymbolic() << "  can not available(entry retrieval).Aborting..." << endl;
				return -1;
			}
		}
	}
}
/*
=================
The function BufferHandlingSettings sets manual buffer handling mode to numBuffers set above.
=================
*/
int BufferHandlingSettings(CameraPtr pCam)
{
	int result = 0;
	//cout << endl << "CONFIGURING BUFFER..." << endl;

	// Retrieve Stream Parameters device nodemap
	Spinnaker::GenApi::INodeMap& sNodeMap = pCam->GetTLStreamNodeMap();

	// Retrieve Buffer Handling Mode Information
	CEnumerationPtr ptrHandlingMode = sNodeMap.GetNode("StreamBufferHandlingMode");
	if (!IsAvailable(ptrHandlingMode) || !IsWritable(ptrHandlingMode))
	{
		std::cout << "Unable to set Buffer Handling mode (node retrieval). Aborting..." << endl << endl;
		return -1;
	}

	CEnumEntryPtr ptrHandlingModeEntry = ptrHandlingMode->GetCurrentEntry();
	if (!IsAvailable(ptrHandlingModeEntry) || !IsReadable(ptrHandlingModeEntry))
	{
		std::cout << "Unable to set Buffer Handling mode (Entry retrieval). Aborting..." << endl << endl;
		return -1;
	}

	// Set stream buffer Count Mode to manual
	CEnumerationPtr ptrStreamBufferCountMode = sNodeMap.GetNode("StreamBufferCountMode");
	if (!IsAvailable(ptrStreamBufferCountMode) || !IsWritable(ptrStreamBufferCountMode))
	{
		std::cout << "Unable to set Buffer Count Mode (node retrieval). Aborting..." << endl << endl;
		return -1;
	}

	CEnumEntryPtr ptrStreamBufferCountModeManual = ptrStreamBufferCountMode->GetEntryByName("Manual");
	if (!IsAvailable(ptrStreamBufferCountModeManual) || !IsReadable(ptrStreamBufferCountModeManual))
	{
		std::cout << "Unable to set Buffer Count Mode entry (Entry retrieval). Aborting..." << endl << endl;
		return -1;
	}
	ptrStreamBufferCountMode->SetIntValue(ptrStreamBufferCountModeManual->GetValue());

	// Retrieve and modify Stream Buffer Count
	CIntegerPtr ptrBufferCount = sNodeMap.GetNode("StreamBufferCountManual");
	if (!IsAvailable(ptrBufferCount) || !IsWritable(ptrBufferCount))
	{
		std::cout << "Unable to set Buffer Count (Integer node retrieval). Aborting..." << endl << endl;
		return -1;
	}

	// Display Buffer Info
	//std::cout << "Stream Buffer Count Mode set to manual" << endl;
	//std::cout << "Default Buffer Count: " << ptrBufferCount->GetValue() << endl;
	//std::cout << "Maximum Buffer Count: " << ptrBufferCount->GetMax() << endl;
	if (ptrBufferCount->GetMax() < numBuffers)
	{
		ptrBufferCount->SetValue(ptrBufferCount->GetMax());
	}
	else
	{
		ptrBufferCount->SetValue(numBuffers);
	}

	//std::cout << "Manual Buffer Count: " << ptrBufferCount->GetValue() << endl;
	ptrHandlingModeEntry = ptrHandlingMode->GetEntryByName("OldestFirst");
	ptrHandlingMode->SetIntValue(ptrHandlingModeEntry->GetValue());
	//std::cout << "Buffer Handling Mode has been set to " << ptrHandlingModeEntry->GetDisplayName() << endl;

	return result;
}

/*
=================
The function CleanBuffer runs on all cameras in hardware trigger mode before the actual recording. If the buffer contains images, these are released in the for loop. When the buffer is empty, GetNextImage will get timeout error and end the loop.
=================
*/
int CleanBuffer(CameraPtr pCam)
{
	int result = 0;

	// Clean Buffer acquiring idle images
	pCam->BeginAcquisition();

	try
	{
		for (unsigned int imagesInBuffer = 0; imagesInBuffer < numBuffers; imagesInBuffer++)
		{
			// first numBuffer images are descarted
			ImagePtr pResultImage = pCam->GetNextImage(100);
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
char* monitoringImg1 = NULL;
char* monitoringImg2 = NULL;
bool AcquireOK = FALSE;
bool StopKey = FALSE;
bool RestKey = FALSE;
bool Runningkey = TRUE;
DWORD WINAPI AcquireImages(LPVOID lpParam)
{

	AcquireOK = TRUE;
	// START function in UN-locked thread

	// Initialize camera
	CameraPtr pCam = *((CameraPtr*)lpParam);
	pCam->Init();

	// Start actual image acquisition
	pCam->BeginAcquisition();

	// Initialize empty parameters outside of locked case
	ImagePtr pResultImage;
	char* imageData; // 모니터링을 위해 밖으로 빼고 싶은데, thread에 필요한 거라서 안될듯..
	string deviceUser_ID;
	int firstFrame = 1;
	int stopwait = 0;

	while (!StopKey)
	{
		// Retrieve and save images in while loop until manual ESC press
		while (RestKey)
		{
			if (GetAsyncKeyState(VK_DOWN))
			{
				Runningkey = TRUE;
				RestKey = FALSE;
			}
			printf("Pause... %d \n", pCam->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber"));
			Sleep(1000);
		}
		printf("Running...\n");
		while (Runningkey)
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
					INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();
					CStringPtr ptrStringSerial = pCam->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");
					if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
					{
						serialNumber = ptrStringSerial->GetValue();
					}

					INodeMap& nodeMap = pCam->GetNodeMap();
					CStringPtr ptrDeviceUserId = pCam->GetNodeMap().GetNode("DeviceUserID");
					if (IsAvailable(ptrDeviceUserId) && IsReadable(ptrDeviceUserId))
					{
						deviceUser_ID = ptrDeviceUserId->GetValue();
						cameraCnt = atoi(deviceUser_ID.c_str());
					}

					// For first loop in while ...
					if (firstFrame == 1)
					{
						// first image is descarted
						ImagePtr pResultImage = pCam->GetNextImage();
						char* imageData = static_cast<char*>(pResultImage->GetData());
						pResultImage->Release();

						// anounce start recording
						std::cout << "Camera [" << serialNumber << "] " << "Started recording with ID [" << cameraCnt << " ]..." << endl;
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
						std::cout << "Trigger signal disconnected. Stop recording for camera [" << serialNumber << "]" << endl;
						// End acquisition
						pCam->EndAcquisition();
						cameraFiles[cameraCnt].close();
						csvFile.close();
						// Deinitialize camera
						pCam->DeInit();

						break;
					}

					if (serialNumber == "21159646")
					{
						if (GetAsyncKeyState(VK_UP)) {
							StopKey = TRUE;
						}
						if (GetAsyncKeyState(VK_ESCAPE)) {
							Runningkey = FALSE;
							RestKey = TRUE;
						}
						monitoringImg1 = static_cast<char*>(pResultImage->GetData());
					}
					else
						monitoringImg2 = static_cast<char*>(pResultImage->GetData());
					// Acquire the image buffer to write to a file
					// GetData()가 두번 연속 나오는건 문제가 있어보인다.
					imageData = static_cast<char*>(pResultImage->GetData());
					// Do the writing to assigned cameraFile
					cameraFiles[cameraCnt].write(imageData, pResultImage->GetImageSize());

					csvFile << pResultImage->GetFrameID() << "," << pResultImage->GetTimeStamp() << "," << serialNumber << "," << cameraCnt << "," << getTimeStamp() << endl;

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
	}
	// End acquisition
	pCam->EndAcquisition();
	cameraFiles[cameraCnt].close();
	csvFile.close();

	// Deinitialize camera
	pCam->DeInit();
	std::cout << "pCam->DeInit()" << cameraCnt << "!!" << endl;
	AcquireOK = FALSE;
	return 1;
}

DWORD WINAPI MonitoringThread(LPVOID lpParam)
{
	std::cout << "MonitoringThread start!" << endl;
	// 리사이즈할 크기 지정
	// 이미지 리사이즈
	cv::Mat resized_image = cv::Mat::zeros(cv::Size(2248 / 2, 2048 / 4), CV_8UC1);
	cv::Mat img1 = cv::Mat::zeros(2248, 2048, CV_8UC1);
	cv::Mat img2 = cv::Mat::zeros(2248, 2048, CV_8UC1);
	ImagePtr pImage1, pImage2;
	cv::Mat concatImg = cv::Mat::zeros(cv::Size(2248 * 2, 2048), CV_8UC1);;
	cv::Scalar_<int> blue(255, 0, 0);
	string ExposureTimeMonitor = "exposure time : " + to_string(exposureTime);
	string FrameMonitor = "Frame rate : " + to_string(FPS);
	while (AcquireOK)
	{
		
		if (monitoringImg1 != NULL && monitoringImg2 != NULL)
		{
			pImage1 = Image::Create(2448, 2048, 0, 0, PixelFormat_BayerRGPolarized8, monitoringImg1);
			pImage2 = Image::Create(2448, 2048, 0, 0, PixelFormat_BayerRG8, monitoringImg2);
			pImage1->Save("test1.jpg");
			pImage2->Save("test2.jpg");
			img1 = cv::imread("test1.jpg", 1);
			img2 = cv::imread("test2.jpg", 1);
			if (img1.empty()) printf("img1.empty");
			if (img2.empty()) printf("img2.empty");

			cv::hconcat(img1, img2, concatImg);
			if (concatImg.empty()) printf("concatImg.empty");
			if (resized_image.empty()) printf("resized_image.empty");
			cv::resize(concatImg, resized_image, cv::Size(2248 / 2, 2048 / 4));
			cv::putText(resized_image, ExposureTimeMonitor, cv::Point(0,30), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, 8);
			cv::putText(resized_image, FrameMonitor, cv::Point(0, 60), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, 8);
			cv::imshow("img", resized_image);
			//Mat img = imread("test.jpg", 1);
			//imshow("img", img);
			
		}
		int keycode = cv::waitKey(5);
		
	}
	cv::destroyAllWindows();
	std::cout << "MonitoringThread done!" << endl;
	return 0;
}

/*
=================
The function InitializeMultipleCameras bundles the initialization process for all cameras on the system. It is called in RunMultipleCameras and starts a loop to set Buffer, Strobe, Exposure and Trigger, as well as to create binary files for each camera.
=================
*/
int InitializeMultipleCameras(CameraList camList, CameraPtr* pCamList, unsigned int camListSize)
{
	int result = 0;
	string CurrentDateTime = getCurrentDateTime();
	try
	{
		for (unsigned int i = 0; i < camListSize; i++)
		{
			// Select camera in loop
			pCamList[i] = camList.GetByIndex(i); // TODO: try to get order of USB Interface/primary vs secondary // get by serial?
			pCamList[i]->Init();

			INodeMap& nodeMap = pCamList[i]->GetNodeMap();
			CStringPtr ptrStringSerial = pCamList[i]->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");

			if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
			{
				serialNumber = ptrStringSerial->GetValue();
			}

			// Set DeviceUserID to loop counter to assign camera order to cameraFiles in parallel threads
			CStringPtr ptrDeviceUserId = nodeMap.GetNode("DeviceUserID");
			if (!IsAvailable(ptrDeviceUserId) || !IsWritable(ptrDeviceUserId))
			{
				std::cout << "Unable to get node ptrDeviceUserId. Aborting..." << endl << endl;
				return -1;
			}

			stringstream sstreamDeviceUserID;
			sstreamDeviceUserID << i;
			string DeviceUserID = sstreamDeviceUserID.str();
			ptrDeviceUserId->SetValue(DeviceUserID.c_str());
			cameraCnt = atoi(DeviceUserID.c_str());

			std::cout << "Initializing camera " << serialNumber << "... ";

			// Set Buffer
			BufferHandlingSettings(pCamList[i]);

			// Set Exposure
			ConfigureExposure(nodeMap);

			// Set Image Settings
			ImageSettings(nodeMap);

			// Start all cameras in hardware trigger mode 
			chosenTrigger = HARDWARE;
			ConfigureTrigger(nodeMap);

			// Set pixelForamt
			ConfigurePixelFormat(nodeMap);

			// Clean buffer
			CleanBuffer(pCamList[i]);

			// Create binary files for each camera and overall .csv logfile
			CreateFiles(serialNumber, cameraCnt, CurrentDateTime);

			// For trigger camera:
			if (serialNumber == triggerCam)
			{
				// Configure Trigger
				chosenTrigger = SOFTWARE;
				ConfigureTrigger(nodeMap);

				// Set Framerate
				ConfigureFramerate(nodeMap);

				// read framerate for output
				CFloatPtr ptrAcquisitionFrameRate = nodeMap.GetNode("AcquisitionFrameRate");
				if (!IsAvailable(ptrAcquisitionFrameRate) || !IsReadable(ptrAcquisitionFrameRate))
				{
					std::cout << "Unable to get node AcquisitionFrameRate. Aborting..." << endl << endl;
					return -1;
				}
				NewFrameRate = ptrAcquisitionFrameRate->GetValue();
				std::cout << "set as trigger with " << NewFrameRate << "Hz... ";
			}
			pCamList[i]->DeInit();
			std::cout << "Done!" << endl;
		}
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
The function RunMultipleCameras acts as the body of the program. It calls the InitializeMultipleCameras function to configure all cameras, then creates parallel threads and starts AcquireImages on each thred. With the setting configured above, threads are waiting for synchronized trigger as a central clock.
=================
*/
int RunMultipleCameras(CameraList camList)
{
	int result = 0;
	unsigned int camListSize = 0;

	try
	{
		// Retrieve camera list size
		camListSize = camList.GetSize();

		// Create an array of handles
		CameraPtr* pCamList = new CameraPtr[camListSize];

		// Initialize cameras in camList 
		result = InitializeMultipleCameras(camList, pCamList, camListSize);

		// START RECORDING
		std::cout << endl << "*** START RECORDING ***" << endl;

		HANDLE* grabThreads = new HANDLE[camListSize+1];
		
		for (unsigned int i = 0; i < camListSize + 1; i++)
		{
			// Start grab thread
			if (i < camListSize)
			{
				grabThreads[i] = CreateThread(nullptr, 0, AcquireImages, &pCamList[i], 0, nullptr); // call AcquireImages in parallel threads
				assert(grabThreads[i] != nullptr);

			}
			else
			{
				grabThreads[i] = CreateThread(nullptr, 0, MonitoringThread, nullptr, 0, nullptr);
				assert(grabThreads[i] != nullptr);
			}
		}
		//HANDLE monitoringThread = CreateThread(nullptr, 0, MonitoringThread, nullptr, 0, nullptr);
		// Wait for all threads to finish
		WaitForMultipleObjects(
			camListSize+1, // number of threads to wait for
			grabThreads, // handles for threads to wait for
			TRUE,        // wait for all of the threads
			INFINITE     // wait forever
		);
		//WaitForSingleObject(monitoringThread, INFINITE);
		std::cout << endl << "*** CloseHandle  ***" << endl;
		CloseHandle(ghMutex);

		// Check thread return code for each camera
		for (unsigned int i = 0; i < camListSize; i++)
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
		for (unsigned int i = 0; i < camListSize; i++)
		{
			pCamList[i] = 0;
			CloseHandle(grabThreads[i]);
		}
		//CloseHandle(monitoringThread);
		// Delete array pointer
		delete[] grabThreads;
		//delete monitoringThread;

		// End recording
		std::cout << endl << "*** STOP RECORDING ***" << endl;

		// Reset Cameras
		result = ResetSettings(camList, pCamList, camListSize);

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
/$$$$$$$/$$ |  $$ $$$$$$$  /$$$$$$$/$$    |  $$ |       $$ | $$    $$< 
$$      \$$ |  $$ $$ |  $$ $$ |     $$$$$/   $$ |       $$ | $$$$$$$  |
 $$$$$$  $$ \__$$ $$ |  $$ $$ \_____$$ |     $$ |_____ _$$ |_$$ |  $$ |
/     $$/$$    $$ $$ |  $$ $$       $$ |     $$       / $$   $$ |  $$ |
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
	SystemPtr system = System::GetInstance();

	// Print out current library version
	const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
	std::cout << "Spinnaker library version: " << spinnakerLibraryVersion.major << "." << spinnakerLibraryVersion.minor << "." << spinnakerLibraryVersion.type << "." << spinnakerLibraryVersion.build;

	// Retrieve list of cameras from the system
	CameraList camList = system->GetCameras();

	unsigned int numCameras = camList.GetSize();

	std::cout << "	Number of cameras detected: " << numCameras << endl << endl;

	// Finish if there are no cameras
	if (numCameras == 0)
	{
		// Clear camera list before releasing system
		camList.Clear();

		// Release system
		system->ReleaseInstance();

		std::cout << "No cameras connected!" << endl;
		std::cout << "Done! Press Enter to exit..." << endl;
		getchar();

		return -1;
	}

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
	result = RunMultipleCameras(camList);

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
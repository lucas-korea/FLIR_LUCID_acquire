#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <fstream>
#include <cstdlib>
#include <queue>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

queue<ImagePtr> myQueue;

void ImageAcquisition(CameraPtr pCam);
void SaveImages(const char* filename);

// This function acquires and saves 10 images from a device.
int AcquireImagesThread(CameraPtr pCam, INodeMap& nodeMap, INodeMap& nodeMapTLDevice)
{
    int result = 0;

    cout << endl << endl << "*** IMAGE ACQUISITION ***" << endl << endl;

    try
    {
        CEnumerationPtr ptrAcquisitionMode = nodeMap.GetNode("AcquisitionMode");
        if (!IsAvailable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode))
        {
            cout << "Unable to set acquisition mode to continuous (enum retrieval). Aborting..." << endl << endl;
            return -1;
        }

        // Retrieve entry node from enumeration node
        CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
        if (!IsAvailable(ptrAcquisitionModeContinuous) || !IsReadable(ptrAcquisitionModeContinuous))
        {
            cout << "Unable to set acquisition mode to continuous (entry retrieval). Aborting..." << endl << endl;
            return -1;
        }

        // Retrieve integer value from entry node
        const int64_t acquisitionModeContinuous = ptrAcquisitionModeContinuous->GetValue();

        // Set integer value from entry node as new value of enumeration node
        ptrAcquisitionMode->SetIntValue(acquisitionModeContinuous);

        cout << "Acquisition mode set to continuous..." << endl;

#ifdef _DEBUG
        cout << endl << endl << "*** DEBUG ***" << endl << endl;

        cout << endl << endl << "*** END OF DEBUG ***" << endl << endl;
#endif
        pCam->BeginAcquisition();

        cout << "Acquiring images..." << endl;
        gcstring deviceSerialNumber("");
        CStringPtr ptrStringSerial = nodeMapTLDevice.GetNode("DeviceSerialNumber");
        if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
        {
            deviceSerialNumber = ptrStringSerial->GetValue();

            cout << "Device serial number retrieved as " << deviceSerialNumber << "..." << endl;
        }
        cout << endl;

        // Retrieve, convert, and save images
        //const unsigned int k_numImages = 10;


        try
        {
            /*ImagePtr pResultImage = pCam->GetNextImage(1000);
            if (pResultImage->IsIncomplete())
            {
                // Retrieve and print the image status description
                cout << "Image incomplete: " << Image::GetImageStatusDescription(pResultImage->GetImageStatus())
                        << "..." << endl
                        << endl;
            }
            else
            {
                const size_t width = pResultImage->GetWidth();
                const size_t height = pResultImage->GetHeight();
                cout << "Grabbed image " << imageCnt << ", width = " << width << ", height = " << height << endl;
                
                ImagePtr convertedImage = pResultImage->Convert(PixelFormat_BayerRGPolarized8, NO_COLOR_PROCESSING);

                // Create a unique filename
                ostringstream filename;

                filename << "Acquisition-";
                if (!deviceSerialNumber.empty())
                {
                    filename << deviceSerialNumber.c_str() << "-";
                }
                filename << imageCnt << ".jpg";

                convertedImage->Save(filename.str().c_str());

                cout << "Image saved at " << filename.str() << endl;
            }
            
            pResultImage->Release();
            cout << endl;*/
            thread t1(ImageAcquisition, pCam);
            thread t2(SaveImages, "test.bin");

            t1.join();
            t2.join();
            //ImageAcquisition(pCam);
            //SaveImages("");
            cout << "thread Done!" << endl;
        }
        catch (Spinnaker::Exception& e)
        {
            cout << "Error: " << e.what() << endl;
            result = -1;
        }

        pCam->EndAcquisition();
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        return -1;
    }

    return result;
}

void ImageAcquisition(CameraPtr pCam)
{
    for (size_t i = 0; i < 30; i++)
    {
        cout << "Acq! " << i << endl;
        ImagePtr pResultImage = pCam->GetNextImage(1000);
        if (pResultImage->IsIncomplete())
        {
            // Retrieve and print the image status description
            cout << "Image incomplete: " << Image::GetImageStatusDescription(pResultImage->GetImageStatus())
                << "..." << endl
                << endl;
        }

        

        myQueue.push(pResultImage);
        //cout << pResultImage << endl;
        //cout << &myQueue.front() << endl;
        //pResultImage->Save(filename_.c_str());
        //pResultImage->Release();


        //cout << filename << endl;
        //myQueue.front()->Save(filename_.c_str());

        //cout << myQueue.size() << endl;
    }
    
}
void SaveImages(const char* filename)
{
    ofstream file(filename, std::ios::binary);
    file << "P6\n" << "\n255\n";
    for (int i = 0; i < 30; i++)
    {
        cout << "save!! " << i << endl;
        if (!myQueue.empty())
        {
            //this_thread::sleep_for(chrono::milliseconds(100));
            char ii[4];
            string filename_;
            itoa(i, ii, 10);
            filename_ ="test";
            filename_ += ii;
            filename_ += ".jpg";
            //cout << filename << endl;
            myQueue.front()->Save(filename_.c_str());
            //myQueue.front()->Save(filename_.c_str());
            //file.write(reinterpret_cast<const char*>(&myQueue.front()), sizeof(myQueue.front()));
            //cout << myQueue.size() << endl;
            myQueue.pop();
        }
        else
        {
            this_thread::sleep_for(chrono::milliseconds(100));
            i--;
        }
    }
    file.close();
}

// This function acts as the body of the example; please see NodeMapInfo example
// for more in-depth comments on setting up cameras.
int RunSingleCamera(CameraPtr pCam)
{
    int result;

    try
    {
        // Retrieve TL device nodemap and print device information
        INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();

        // Initialize camera
        pCam->Init();

        // Retrieve GenICam nodemap
        INodeMap& nodeMap = pCam->GetNodeMap();

        CFloatPtr ptrExposureTime = nodeMap.GetNode("ExposureTime");
        double exposureTimeToSet = 33331;
        ptrExposureTime->SetValue(exposureTimeToSet);

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

                cout << "Pixel format set to " << ptrPixelFormat->GetCurrentEntry()->GetSymbolic() << "..." << endl;
            }
            else
            {
                // Methods in the ImageUtilityPolarization class are supported for images of
                // polarized pixel formats only.
                cout << "currente Pixel format is " << ptrPixelFormat->GetCurrentEntry()->GetSymbolic() << "..." << endl;
                cout << "Pixel format  " << ptrPixelFormatBayerRGPolarized8->GetSymbolic()  << "  can not available(entry retrieval).Aborting..." << endl;
                return -1;
            }
        }


        // Acquire images
        result = result | AcquireImagesThread(pCam, nodeMap, nodeMapTLDevice);

        // Deinitialize camera
        pCam->DeInit();
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

int main(int /*argc*/, char** /*argv*/)
{
    // Since this application saves images in the current folder
    // we must ensure that we have permission to write to this folder.
    // If we do not have permission, fail right away.
    FILE* tempFile = fopen("test.txt", "w+");
    if (tempFile == nullptr)
    {
        cout << "Failed to create file in current folder.  Please check "
                "permissions."
             << endl;
        cout << "Press Enter to exit..." << endl;
        getchar();
        return -1;
    }
    fclose(tempFile);
    remove("test.txt");

    // Print application build information
    cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl << endl;

    // Retrieve singleton reference to system object
    SystemPtr system = System::GetInstance();

    // Print out current library version
    const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
    cout << "Spinnaker library version: " << spinnakerLibraryVersion.major << "." << spinnakerLibraryVersion.minor
         << "." << spinnakerLibraryVersion.type << "." << spinnakerLibraryVersion.build << endl
         << endl;

    // Retrieve list of cameras from the system
    CameraList camList = system->GetCameras();

    const unsigned int numCameras = camList.GetSize();
    cout << "Number of cameras detected: " << numCameras << endl << endl;

    // Finish if there are no cameras
    if (numCameras == 0)
    {
        camList.Clear();
        system->ReleaseInstance();
        cout << "Not enough cameras!" << endl;
        cout << "Done! Press Enter to exit..." << endl;
        getchar();
        return -1;
    }

    CameraPtr pCam = nullptr;

    int result = 0;

    // Run example on each camera
    for (unsigned int i = 0; i < numCameras; i++)
    {
        // Select camera
        pCam = camList.GetByIndex(i);

        cout << endl << "Running example for camera " << i << "..." << endl;

        // Run example
        result = result | RunSingleCamera(pCam);

        cout << "Camera " << i << " example complete..." << endl << endl;
    }


    pCam = nullptr;

    // Clear camera list before releasing system
    camList.Clear();

    // Release system
    system->ReleaseInstance();

    cout << endl << "Done! Press Enter to exit..." << endl;
    getchar();

    return result;
}

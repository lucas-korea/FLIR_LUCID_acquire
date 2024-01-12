#include <windows.h>
#include <atlstr.h>
#include <assert.h>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>
#define COM_MAXBLOCK     4095

DWORD WINAPI CommWatchProc(LPVOID lpData);
using namespace std;

class CSerialPort
{
public:
    CSerialPort(void);
    virtual ~CSerialPort(void);

private:


public:
    HANDLE  m_hComm;    //컴포트 디바이스 연결 핸들
    DCB     m_dcb;
    BOOL    fConnected; //컴포트가 연결되면 1로 설정
    DWORD       dwThreadID;    // 쓰레드 ID
    COMMTIMEOUTS m_CommTimeouts;
    HANDLE        grabThread;    // 쓰레드 핸들
    bool    m_bPortReady;
    bool    m_bWriteRC;
    bool    m_bReadRC;
    DWORD   m_iBytesWritten;
    DWORD   m_iBytesRead;
    DWORD   m_dwBytesRead;
    void ClosePort();
    bool ReadByte(BYTE& resp);
    bool ReadByte(BYTE*& resp, UINT size);
    int ReadCommBlock(LPSTR lpszBlock, int nMaxLength);
    bool WriteByte(BYTE bybyte);
    bool OpenPort(CString portname);
    bool SetCommunicationTimeouts(DWORD ReadIntervalTimeout,
        DWORD ReadTotalTimeoutMultiplier, DWORD ReadTotalTimeoutConstant,
        DWORD WriteTotalTimeoutMultiplier, DWORD WriteTotalTimeoutConstant);
    bool ConfigurePort(DWORD BaudRate, BYTE ByteSize, DWORD fParity,
        BYTE  Parity, BYTE StopBits);
    OVERLAPPED  osWrite, osRead;

};
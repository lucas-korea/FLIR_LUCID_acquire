#include "SerialPort.h"  

using namespace std;


CSerialPort::CSerialPort()
{
}

CSerialPort::~CSerialPort()
{
}

bool CSerialPort::OpenPort(CString portname)

{   // 포트 상태 관련 변수들을 초기화 한다
    osRead.Offset = 0;
    osRead.OffsetHigh = 0;

    // Overlapped I/O에 쓰이는 Event 객체들을 생성
    // Read를 위한 Event 객체 생성
    osRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (osRead.hEvent == NULL)
    {
        return FALSE;
    }

    // 포트를 생성한다
    m_hComm = CreateFile(L"//./" + portname,
        GENERIC_READ | GENERIC_WRITE,
        0,
        0,
        OPEN_EXISTING,
        0,
        0);
    SetCommMask(m_hComm, EV_RXCHAR | EV_BREAK | EV_ERR);


    SetupComm(m_hComm, 4096, 4096);	// 버퍼 설정
    // 대기중인 모든 데이타를 지운다	
    PurgeComm(m_hComm, PURGE_TXABORT | PURGE_RXABORT |
        PURGE_TXCLEAR | PURGE_RXCLEAR);

    if (m_hComm == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    else
        //cout << "m_hComm == INVALID_HANDLE_VALUE" << endl;
        return true;
}

bool CSerialPort::ConfigurePort(DWORD BaudRate, BYTE ByteSize, DWORD fParity,
    BYTE Parity, BYTE StopBits)
{
    if ((m_bPortReady = GetCommState(m_hComm, &m_dcb)) == 0)
    {
        CloseHandle(m_hComm);
        return false;
    }

    m_dcb.BaudRate = BaudRate;
    m_dcb.ByteSize = ByteSize;
    m_dcb.Parity = Parity;
    m_dcb.StopBits = StopBits;
    m_dcb.fBinary = true;
    m_dcb.fDsrSensitivity = false;
    m_dcb.fParity = fParity;
    m_dcb.fOutX = false;
    m_dcb.fInX = false;
    m_dcb.fNull = false;
    m_dcb.fAbortOnError = true;
    m_dcb.fOutxCtsFlow = false;
    m_dcb.fOutxDsrFlow = false;
    m_dcb.fDtrControl = DTR_CONTROL_DISABLE;
    m_dcb.fDsrSensitivity = false;
    m_dcb.fRtsControl = RTS_CONTROL_DISABLE;
    m_dcb.fOutxCtsFlow = false;
    m_dcb.fOutxCtsFlow = false;

    m_bPortReady = SetCommState(m_hComm, &m_dcb);

    if (m_bPortReady == 0)
    {
        CloseHandle(m_hComm);
        return false;
    }
    fConnected = TRUE;
    return true;
}

bool CSerialPort::SetCommunicationTimeouts(DWORD ReadIntervalTimeout,
    DWORD ReadTotalTimeoutMultiplier, DWORD ReadTotalTimeoutConstant,
    DWORD WriteTotalTimeoutMultiplier, DWORD WriteTotalTimeoutConstant)
{
    m_CommTimeouts.ReadIntervalTimeout = ReadIntervalTimeout;
    m_CommTimeouts.ReadTotalTimeoutConstant = ReadTotalTimeoutConstant;
    m_CommTimeouts.ReadTotalTimeoutMultiplier = ReadTotalTimeoutMultiplier;
    m_CommTimeouts.WriteTotalTimeoutConstant = WriteTotalTimeoutConstant;
    m_CommTimeouts.WriteTotalTimeoutMultiplier = WriteTotalTimeoutMultiplier;

    m_bPortReady = SetCommTimeouts(m_hComm, &m_CommTimeouts);


    return true;
}



int CSerialPort::ReadCommBlock(LPSTR lpszBlock, int nMaxLength)
{
    BOOL       fReadStat;
    COMSTAT    ComStat;
    DWORD      dwErrorFlags;
    DWORD      dwLength;

    // only try to read number of bytes in queue 
    ClearCommError(m_hComm, &dwErrorFlags, &ComStat);
    // 읽기버퍼에 들어와있지만 아직 읽지 못하고 남아있는 바이트 수
    dwLength = min((DWORD)nMaxLength, ComStat.cbInQue);

    if (dwLength > 0)
    {
        fReadStat = ReadFile(m_hComm, lpszBlock, dwLength, &dwLength, &osRead);

        if (!fReadStat)
        {
            cout << "here" << endl;
            if (GetLastError() == ERROR_IO_PENDING)
            {
                //OutputDebugString("\n\rIO Pending");
                // we have to wait for read to complete.  This function will timeout
                // according to the CommTimeOuts.ReadTotalTimeoutConstant variable
                // Every time it times out, check for port errors								
                while (!GetOverlappedResult(m_hComm, &osRead,
                    &dwLength, TRUE))
                {
                    DWORD dwError = GetLastError();
                    if (dwError == ERROR_IO_INCOMPLETE)
                    {
                        // normal result if not finished
                        continue;
                    }
                    else
                    {
                        // CAN DISPLAY ERROR MESSAGE HERE
                        // an error occured, try to recover
                        ::ClearCommError(m_hComm, &dwErrorFlags, &ComStat);
                        if (dwErrorFlags > 0)
                        {
                            // CAN DISPLAY ERROR MESSAGE HERE
                        }
                        break;
                    }
                }
            }
            else
            {
                // some other error occured
                dwLength = 0;
                ClearCommError(m_hComm, &dwErrorFlags, &ComStat);
                if (dwErrorFlags > 0)
                {
                    // CAN DISPLAY ERROR MESSAGE HERE
                }
            }
        }
    }

    return (dwLength);
}


void CSerialPort::ClosePort()
{
    fConnected = FALSE;

    // 이벤트 발생을 중지한다
    SetCommMask(m_hComm, 0);

    // DTR(Data-Terminal-Ready) 시그널을 Clear 한다
    EscapeCommFunction(m_hComm, CLRDTR);

    // 대기중인 모든 데이타를 지운다	
    PurgeComm(m_hComm, PURGE_TXABORT | PURGE_RXABORT |
        PURGE_TXCLEAR | PURGE_RXCLEAR);

    CloseHandle(osRead.hEvent);
    CloseHandle(m_hComm);

    return;
}
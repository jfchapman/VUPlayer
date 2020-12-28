#pragma once

namespace APE
{

#define APE_FILE_BEGIN 0
#define APE_FILE_CURRENT 1
#define APE_FILE_END 2

class CIO
{   
public:
    // construction / destruction
    CIO() 
    { 
        m_nSeekPosition = 0;
        m_nSeekMethod = APE_FILE_BEGIN;
    }
    virtual ~CIO() { };

    // open / close
    virtual int Open(const wchar_t * pName, bool bOpenReadOnly = false) = 0;
    virtual int Close() = 0;
    
    // read / write
    virtual int Read(void * pBuffer, unsigned int nBytesToRead, unsigned int * pBytesRead) = 0;
    virtual int Write(const void * pBuffer, unsigned int nBytesToWrite, unsigned int * pBytesWritten) = 0;
    
    // seek
    void SetSeekPosition(int64 nPosition)
    {
        m_nSeekPosition = nPosition;
    }
    void SetSeekMethod(unsigned int nMethod)
    {
        m_nSeekMethod = nMethod;
    }
    virtual int64 PerformSeek() = 0;
    
    // creation / destruction
    virtual int Create(const wchar_t * pName) = 0;
    virtual int Delete() = 0;

    // other functions
    virtual int SetEOF() = 0;

    // attributes
    virtual int64 GetPosition() = 0;
    virtual int64 GetSize() = 0;
    virtual int GetName(wchar_t * pBuffer) = 0;

protected:

    int64 m_nSeekPosition;
    unsigned int m_nSeekMethod;
};

}

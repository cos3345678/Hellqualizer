#ifndef _RINGBUFFER
#define _RINGBUFFER
class RingBuffer
{
public:
    RingBuffer( int sizeBytes );
    ~RingBuffer();
    int Read( unsigned char* dataPtr, int numBytes );
    int Write( unsigned char *dataPtr, int numBytes );
    bool Empty( void );
    int GetSize( );
    int GetWriteAvail( );
    int GetReadAvail( );
private:
    unsigned char * _data;
    int _size;
    int _readPtr;
    int _writePtr;
    int _writeBytesAvail;
};
#endif

////////////////////////////////////////////////////////////////////////////////
/// @file   I2cDriverTransportChannel.cpp
//
//  Copyright 2012 Bose Corporation, Framingham, MA
////////////////////////////////////////////////////////////////////////////////

#include "I2cDriverTransportChannel.h"
#include "DPrint.h"
#include <unistd.h>

#define MAX_I2C_TOTAL_TRIES     4

static DPrint s_logger( "I2cDriverT" );

////////////////////////////////////////////////////////////////////////
/// Constructor
/// @brief Constructs an instance of the I2cDriverTransportChannel object.
////////////////////////////////////////////////////////////////////////
I2cDriverTransportChannel::I2cDriverTransportChannel( bool debug )
    : DriverTransportChannel( debug )
{
    mHandle = -1;
}

////////////////////////////////////////////////////////////////////////
/// FUNCTION_NAME_HERE
/// @brief FUNCTION_DESCRIPTION_HERE
/// @param PARAMAMETER_NAME(SHOULD_MATCH_BELOW) (WITH DESCIPTION)
/// @return DriverTransportReturn_t
////////////////////////////////////////////////////////////////////////
I2cDriverTransportChannel::~I2cDriverTransportChannel( )
{

}

////////////////////////////////////////////////////////////////////////
/// Initialize
/// @brief Initializes the transport channel's operation
/// @param void* pInitData - ptr. to truct I2cDriverTransportChannel::InitData, filled in:
/// UINT8 p->I2cAdapterNumber: "small" decimal number 'N', corresponding to "/dev/i2c-N"
/// UINT8 p->BusAddress: logical bus address of the corresponding physical device
/// bool p->UsesPec: true if physical device requires error correction protocol
/// @return DriverTransportReturn_t ENOERROR (0) on success, -errno otherwise
/// -ENFILE: no C-level static storage left to open another I2C transport channel
///     (fix by increasing size of 'sI2cTransportChannels').
/// -ENOENT: call to linux 'open' failed w/device name we gave it.
////////////////////////////////////////////////////////////////////////
DriverTransportReturn_t I2cDriverTransportChannel::Initialize( void* pInitData )
{
    struct I2cDriverTransportChannel::InitData* pData
        = ( I2cDriverTransportChannel::InitData* )pInitData;

    i2c_transport_channel_initialize_system();

    mHandle = i2c_transport_channel_open( pData->I2cAdapterNumber, pData->BusAddress,
                                          pData->UsesPec ? TRUE : FALSE,
                                          pData->UsesSmbusProtocol ? TRUE : FALSE,
                                          Debug() );  // pass our debug flag to lower levels
    if( Debug() )
    {
        if( mHandle >= 0 )          // everything hunky-dory
            PrintTransportChannelDebugInfo( __FILE__, __LINE__ );
        else    // mega-possum bummer  {>;-<   (R2L: frowning, crying, furrowed brow, w/a hat on)
            s_logger.LogInfo( "%s:%d: i2c_transport_channel_open(%d,0x%02X,%d,%d) returned %d\n",
                              __FILE__, __LINE__,
                              pData->I2cAdapterNumber, pData->BusAddress, pData->UsesPec,
                              pData->UsesSmbusProtocol, ( int )mHandle );
    }
    return ( mHandle >= 0 ? ENOERROR : mHandle );
}

////////////////////////////////////////////////////////////////////////
/// PrintTransportChannelDebugInfo
////////////////////////////////////////////////////////////////////////
void I2cDriverTransportChannel::PrintTransportChannelDebugInfo( bool override_mDebug )
{
    PrintTransportChannelDebugInfo( __FILE__, __LINE__, override_mDebug );
}

////////////////////////////////////////////////////////////////////////
/// PrintTransportChannelDebugInfo
////////////////////////////////////////////////////////////////////////
void I2cDriverTransportChannel::PrintTransportChannelDebugInfo( const char* file, int line,
                                                                bool override_mDebug )
{
    if( Debug() || override_mDebug )
    {
        i2c_transport_channel_info_t* pI;
        char errbuf[256];
        pI = i2c_transport_channel_get_info( mHandle, errbuf );
        if( !pI )
        {
            s_logger.LogInfo( "no i2c_trans_ch_in %s", errbuf );
            return;
        }
        s_logger.LogInfo( "%s:%d: Device:%s  mDesc:%d  mFd:%d  mBusAddress:0x%02X"
                          "  mErrno:%d  mState:%d  mUsesPec:%d\n",
                          file, line,
                          pI->mDevName, ( int )pI->mDesc, ( int )pI->mFd, ( unsigned int )pI->mBusAddress,
                          ( int )pI->mErrno, pI->mState, pI->mUsesPec );
    }
}

////////////////////////////////////////////////////////////////////////
/// Read
/// @brief Reads the amount of data from the peripheral as specified
/// @param I2cLocation_t location: Register address of the I2C device to read from;
/// for the type "Receive 1 Byte", this should be 'special' comand register
///     I2C_TRANSPORT_CHANNEL_SEND_OR_RECEIVE_COMMAND
/// @param void* pRdBuf: where to store the result. For 1 byte, it's a UINT8*; for 2 bytes,
/// it's a UINT16*; for all else, it's a UINT8* (buffer).
/// @param uint32_t size: the number of bytes desired to read; for a block read, if
/// you don't know how many you'll read, make it I2C_TRANSPORT_CHANNEL_MAX_BLOCK_READ_SIZE.
/// @return I2cReturn_t - Number of bytes read if success
/// -ENXIO iff a bad handle passed in
/// -EINVAL iff block read w/a size of 0 or too big
/// whatever errno was if it fails (as a negative value); see ioctl return codes
////////////////////////////////////////////////////////////////////////
DriverTransportReturn_t I2cDriverTransportChannel::Read( /*DriverTransportHandle_t,*/
    DriverTransportLocation_t Location, void* pRdBuf, uint32_t RdSize )
{
    DriverTransportReturn_t rc;
    for( int i = 0; i < MAX_I2C_TOTAL_TRIES; i++ )
    {
        rc = i2c_transport_channel_read( mHandle, Location, pRdBuf, RdSize );
        if( rc != ( DriverTransportReturn_t )RdSize )
        {
            PrintTransportChannelDebugInfo( __FILE__, __LINE__, true );
            if( Debug() )
                s_logger.LogInfo( "On TRY %d, failed to read %d bytes from location 0x%02X; rc = %d\n",
                                  i, ( int )RdSize, ( int )Location, ( int )rc );
        }
        else
            break;
        // TODO FIX ME to not be hardcoded; make # retries a variable
        usleep( 5000 );
    }
    return rc;
}

////////////////////////////////////////////////////////////////////////
/// Write
/// @brief Writes the amount of data from the peripheral as specified
/// @param I2cLocation_t location: Register address of the I2C device to write to;
/// for the type "Write 1 Byte", this should be 'special' comand register
///     I2C_TRANSPORT_CHANNEL_SEND_OR_RECEIVE_COMMAND
/// @param void* pBuf: data to write. For 1 byte, it's a UINT8*; for 2 bytes,
/// it's a UINT16*; for all else, it's a UINT8* (buffer).
/// @param uint32_t size: the number of bytes desired to write.
/// @return I2cReturn_t - Number of bytes written if success
/// -ENXIO iff a bad handle passed in
/// -EINVAL iff block read w/a size of 0 or too big
/// whatever errno was if it fails (as a negative value); see ioctl return codes
////////////////////////////////////////////////////////////////////////
DriverTransportReturn_t I2cDriverTransportChannel::Write( /*DriverTransportHandle_t,*/
    DriverTransportLocation_t Location, void* pWrBuf, uint32_t WrSize )
{
    DriverTransportReturn_t rc;
    for( int i = 0; i < MAX_I2C_TOTAL_TRIES; i++ )
    {
        rc = i2c_transport_channel_write( mHandle, Location, pWrBuf, WrSize );
        if( rc != ( DriverTransportReturn_t )WrSize )
        {
            PrintTransportChannelDebugInfo( __FILE__, __LINE__ );
            if( Debug() )
                s_logger.LogInfo( "ON TRY %d, failed to write %d bytes to location 0x%02X; rc = %d\n",
                                  i, ( int )WrSize, ( int )Location, ( int )rc );
        }
        else
            break;
    }
    return rc;
}

////////////////////////////////////////////////////////////////////////
/// WriteThenRead
/// @brief Writes the amount of data from the peripheral as specified, then
/// Reads the amount of data from the peripheral as specified. This is a.k.a.
/// either a "Process call" (2 bytes) or "Block write-Block read Process call".
/// @param I2cLocation_t location: Register address of the I2C device to read from;
/// for the type "Receive 1 Byte", this should be 'special' comand register
///     I2C_TRANSPORT_CHANNEL_SEND_OR_RECEIVE_COMMAND
/// @param UINT8* pWrBuf: data to write
/// @param uint32_t WrSize: the number of bytes desired to write.
/// @param UINT8* pRdBuf: where to store the result
/// @param uint32_t RdSize: the number of bytes desired to read; for a block read, if
/// you don't know how many you'll read, make it I2C_TRANSPORT_CHANNEL_MAX_BLOCK_READ_SIZE.
/// @return I2cReturn_t - Number of bytes read if success
/// -ENXIO iff a bad handle passed in
/// -EINVAL iff block read w/a size of 0 or too big
/// whatever errno was if it fails (as a negative value); see ioctl return codes
////////////////////////////////////////////////////////////////////////
DriverTransportReturn_t I2cDriverTransportChannel::WriteThenRead( /*DriverTransportHandle_t,*/
    DriverTransportLocation_t Location,
    void* pWrBuf, uint32_t WrSize, void* pRdBuf, uint32_t RdSize )
{
    DriverTransportReturn_t rc;
    rc = i2c_transport_channel_write_then_read( mHandle, Location, pWrBuf, WrSize, pRdBuf, RdSize );
    for( int i = 0; i < MAX_I2C_TOTAL_TRIES; i++ )
    {
        if( rc != ( DriverTransportReturn_t )RdSize )
        {
            PrintTransportChannelDebugInfo( __FILE__, __LINE__ );
            if( Debug() )
                s_logger.LogInfo( "ON TRY %d, failed to write %d bytes, or read %d bytes, to location 0x%02X\n",
                                  i, ( int )WrSize, ( int )RdSize, ( int )Location );
        }
        else
            break;
    }
    return rc;
}

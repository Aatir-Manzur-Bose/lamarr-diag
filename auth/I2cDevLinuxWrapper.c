////////////////////////////////////////////////////////////////////////////////
///
/// @file   I2cDevLinuxWrapper.c
/// @brief  Definitions of pure C-level functions which form the User
///         Space API to the I2C channels present in Linux Kernel Space.
/// That is, this layer calls into "/dev/i2c-N", so no application-layer
/// code ever need (nor dare) to.
/// @version    %Version:1 %
/// @author dv15727
/// @date   %Creation Date: Feb 13, 2012%
//
//  Copyright 2012 Bose Corporation, Framingham, MA
//
///////////////////////////////////////////////////////////////////////////////////


#include "I2cDevLinuxWrapper.h"
#include <stdio.h>                  // for perror
#include <stdlib.h>                 // for errno global
#include <errno.h>                  // for errno values
#include <fcntl.h>                  // open
#include <unistd.h>                 // close, usleep
#include <sys/ioctl.h>              // ioctl
#include <string.h>
#include "diag_common.h"

#ifdef I2C_READ_RETRY
#pragma message("==> I2C_READ_RETRY defined")
#endif
#ifdef I2C_WRITE_RETRY
#pragma message("==> I2C_WRITE_RETRY defined")
#endif
#ifdef I2C_RETRY_DISPLAY
#pragma message("==> I2C_RETRY_DISPLAY defined")
#endif

//!<<<< globals
extern FLAG Verbosity;
#define verbosity Verbosity.Get()
static i2c_transport_channel_info_t sI2cTransportChannels[I2C_TRANSPORT_CHANNEL_MAX_CHANNELS] = {};
static bool_t sI2cTransportInitialized = FALSE;
static char tmperr[256];
//!<<<< extern char pLogFile[];

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< I2c_GetRead/Write_Retries()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#ifdef I2C_READ_RETRY
int i2c_read_retries=0;
int I2c_GetReadRetries (void) {
  return (i2c_read_retries);
}
#endif

#ifdef I2C_WRITE_RETRY
int i2c_write_retries=0;
int I2c_GetWriteRetries (void) {
  return (i2c_write_retries);
}
#endif

////////////////////////////////////////////////////////////////////////
/// i2c_read_data
/// @brief
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
static I2cReturn_t i2c_read_data( int file, UINT32 BusAddr,
                                  UINT8 Register,
                                  int ReadBufSize, UINT8* pReadBuf,
                                  bool_t debug )
{
    struct i2c_rdwr_ioctl_data msgset;
    struct i2c_msg  msgs[2];

    UINT8 data_to_write = Register;

    msgs[0].addr = BusAddr;   //slave addr for write
    msgs[0].len = 1;
    msgs[0].flags = 0;
    msgs[0].buf = &data_to_write; // Register to start reading from


    msgs[1].addr = BusAddr;    //slave addr for read
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = ReadBufSize;
    msgs[1].buf = pReadBuf;

    diag_verbose (VVVERBOSE, verbosity, "%s:%d: fd=%d, flags=0x%02X, BusAddr=0x%02X, Register=%d, read size=%d\n", __FILE__, __LINE__, file, msgs[1].flags, msgs[1].addr, data_to_write, msgs[1].len );
    msgset.nmsgs = 1;
    msgset.msgs = &msgs[0];

#ifdef I2C_READ_RETRY
    if( ioctl( file, I2C_RDWR, ( unsigned long )&msgset ) < 0 ) {
      i2c_read_retries++;
      if( ioctl( file, I2C_RDWR, ( unsigned long )&msgset ) < 0 ) {
	diag_verbose (VERBOSE, verbosity,"%s:%d: i2c I2C_RDWR write preceeding read failed; errno=%d(%s) bus=%ld reg=%d\n", __FILE__, __LINE__, errno, strerror( errno ), BusAddr, Register );
	return -1;
      }
    }
#else
    if( ioctl( file, I2C_RDWR, ( unsigned long )&msgset ) < 0 ) {
      diag_verbose (VERBOSE, verbosity, "%s:%d: i2c I2C_RDWR write preceeding read failed; errno=%d(%s) bus=%ld reg=%d\n", __FILE__, __LINE__, errno, strerror( errno ), BusAddr, Register );
      return -1;
    }
#endif

    usleep( 5000 );

    int i;
    for( i = 20; i; i-- )
    {
        msgset.nmsgs = 1;
        msgset.msgs = &msgs[1];
        if( ioctl( file, I2C_RDWR, ( unsigned long )&msgset ) < 0 )
	  {
	    diag_verbose (VERBOSE, verbosity, "%s:%d: i2c I2C_RDWR read(%d) failed; errno=%d(%s) bus=%ld reg=%d\n",__FILE__, __LINE__, i, errno, strerror( errno ), BusAddr, Register );
	  }
        else
            break;      // it worked!
        usleep( 500 );
    }
    if( !i )
        return -1;
    return ReadBufSize;
}

////////////////////////////////////////////////////////////////////////
/// i2c_write_data
/// @brief
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
static I2cReturn_t i2c_write_data( int file, UINT32 BusAddr,
                                   UINT8 Register,
                                   int WriteBufSize, UINT8* pWriteBuf,
                                   bool_t debug )
{
    struct i2c_rdwr_ioctl_data msgset;
    struct i2c_msg  msg;
    UINT8  buf[ 1 + WriteBufSize ];

    buf[0] = Register;
    memcpy( &buf[1], pWriteBuf, WriteBufSize );

    msg.addr = BusAddr;   //slave addr for write
    msg.len = 1 + WriteBufSize;
    msg.flags = 0;
    msg.buf = buf;

    diag_verbose (VVVERBOSE, verbosity, "%s:%d: fd=%d, BusAddr=0x%02X, Register=0x%02X, TOTAL write size=%d\n", __FILE__, __LINE__, file, msg.addr, buf[0], msg.buf[0] );
    
    msgset.nmsgs = 1;
    msgset.msgs = &msg;

#ifdef I2C_WRITE_RETRY
    if( ioctl( file, I2C_RDWR, ( unsigned long )&msgset ) < 0 ) {
      i2c_write_retries++;
      if( ioctl( file, I2C_RDWR, ( unsigned long )&msgset ) < 0 ) {
	diag_verbose(VERBOSE, verbosity,"%s:%d: i2c I2C_RDWR write failed; errno=%d(%s) bus=%ld reg=%d\n", __FILE__, __LINE__, errno, strerror( errno ), BusAddr, Register );
	return -1;
      }
    }
#else
    if( ioctl( file, I2C_RDWR, ( unsigned long )&msgset ) < 0 ) {
      diag_verbose(VERBOSE, verbosity,"%s:%d: i2c I2C_RDWR write failed; errno=%d(%s) bus=%ld reg=%d\n", __FILE__, __LINE__, errno, strerror( errno ), BusAddr, Register );
      return -1;
    }
#endif
    return WriteBufSize;
}

////////////////////////////////////////////////////////////////////////
///
/// @brief
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
static I2cReturn_t i2c_smbus_access( int file, char read_write, UINT8 command,
                                     int size, union i2c_smbus_data *data )
{
    struct i2c_smbus_ioctl_data args;

    args.read_write = read_write;
    args.command = command;
    args.size = size;
    args.data = data;
    return ioctl( file, I2C_SMBUS, &args );
}


////////////////////////////////////////////////////////////////////////
///
/// @brief
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
#if 0
static I2cReturn_t i2c_smbus_write_quick( int file, UINT8 value )
{
    return i2c_smbus_access( file, value, 0, I2C_SMBUS_QUICK, NULL );
}
#endif

////////////////////////////////////////////////////////////////////////
///
/// @brief
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
static I2cReturn_t i2c_smbus_read_byte( int file )
{
    union i2c_smbus_data data;
    if( i2c_smbus_access( file, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data ) )
        return -1;

    return 0x0FF & data.byte;
}

////////////////////////////////////////////////////////////////////////
///
/// @brief
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
static I2cReturn_t i2c_smbus_write_byte( int file, UINT8 value )
{
    return i2c_smbus_access( file, I2C_SMBUS_WRITE, value,
                             I2C_SMBUS_BYTE, NULL );
}

////////////////////////////////////////////////////////////////////////
///
/// @brief
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
static I2cReturn_t i2c_smbus_read_byte_data( int file, UINT8 command )
{
    union i2c_smbus_data data;
    if( i2c_smbus_access( file, I2C_SMBUS_READ, command,
                          I2C_SMBUS_BYTE_DATA, &data ) )
        return -1;

    return 0x0FF & data.byte;
}

////////////////////////////////////////////////////////////////////////
///
/// @brief
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
static I2cReturn_t i2c_smbus_write_byte_data( int file, UINT8 command,
                                              UINT8 value )
{
    union i2c_smbus_data data;
    data.byte = value;
    return i2c_smbus_access( file, I2C_SMBUS_WRITE, command,
                             I2C_SMBUS_BYTE_DATA, &data );
}

////////////////////////////////////////////////////////////////////////
///
/// @brief
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
static I2cReturn_t i2c_smbus_read_word_data( int file, UINT8 command )
{
    union i2c_smbus_data data;
    if( i2c_smbus_access( file, I2C_SMBUS_READ, command,
                          I2C_SMBUS_WORD_DATA, &data ) )
        return -1;

    return 0x0FFFF & data.word;
}

////////////////////////////////////////////////////////////////////////
///
/// @brief
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
static I2cReturn_t i2c_smbus_write_word_data( int file, UINT8 command,
                                              UINT16 value )
{
    union i2c_smbus_data data;
    data.word = value;
    return i2c_smbus_access( file, I2C_SMBUS_WRITE, command,
                             I2C_SMBUS_WORD_DATA, &data );
}

////////////////////////////////////////////////////////////////////////
///
/// @brief
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
static I2cReturn_t i2c_smbus_process_call( int file, UINT8 command, UINT16 value )
{
    union i2c_smbus_data data;
    data.word = value;
    if( i2c_smbus_access( file, I2C_SMBUS_WRITE, command,
                          I2C_SMBUS_PROC_CALL, &data ) )
        return -1;

    return 0x0FFFF & data.word;
}


/* Returns the number of read bytes */
////////////////////////////////////////////////////////////////////////
///
/// @brief
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
#if 0
static I2cReturn_t i2c_smbus_read_block_data( int file, UINT8 command,
                                              UINT8 *values )
{
    union i2c_smbus_data data;
    int i;
    if( i2c_smbus_access( file, I2C_SMBUS_READ, command,
                          I2C_SMBUS_BLOCK_DATA, &data ) )
        return -1;
    else
    {
        for( i = 1; i <= data.block[0]; i++ )
            values[i - 1] = data.block[i];
    }
    return data.block[0];
}
#endif
////////////////////////////////////////////////////////////////////////
///
/// @brief
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
static I2cReturn_t i2c_smbus_write_block_data( int file, UINT8 command,
                                               UINT8 length, UINT8 *values )
{
    union i2c_smbus_data data;
    int i;
    if( length > 32 )
        length = 32;
    for( i = 1; i <= length; i++ )
        data.block[i] = values[i - 1];
    data.block[0] = length;
    return i2c_smbus_access( file, I2C_SMBUS_WRITE, command,
                             I2C_SMBUS_BLOCK_DATA, &data );
}

////////////////////////////////////////////////////////////////////////
/// i2c_smbus_read_i2c_block_data
/// @brief Read a block of data I2C-style (not SMBus-style)
/// @param int file: the file descriptor to read from
/// @param UINT8 command: the location to read it from
/// @param UINT8 length: the MAXIMUM # of bytes to read (!)
/// @partam UINT8* values: where to copy the data read to
/// @return
////////////////////////////////////////////////////////////////////////
/* Returns the number of read bytes */
/* Until kernel 2.6.22, the length is hardcoded to 32 bytes. If you
   ask for less than 32 bytes, your code will only work with kernels
   2.6.23 and later. */
static I2cReturn_t i2c_smbus_read_i2c_block_data( int file, UINT8 command,
                                                  UINT8 length, UINT8 *values, bool_t debug )
{
    union i2c_smbus_data data;
    I2cReturn_t rc;
    int i;

    if( length > 32 )
        length = 32;
    data.block[0] = length;
    rc = i2c_smbus_access( file, I2C_SMBUS_READ, command,
                           length == 32 ? I2C_SMBUS_I2C_BLOCK_BROKEN : I2C_SMBUS_I2C_BLOCK_DATA,
                           &data );
    if( debug )
        printf( "%s:%d: rc(%d) = i2c_smbus_read_i2c_block_data(..,0x%02X,%d,..);\n",
                __FILE__, __LINE__, ( int )rc, command, length );

    if( rc != ENOERROR )    // return code s.b. 0 (zero)
    {
        return rc;
    }

    // the call to i2c_smbus_access succeeded. Deal w/the block of data it read back.
    if( debug )
    {
        printf( "%s:%d: data.block[0] (# bytes read in, as reported by I2C slave) = %d\n",
                __FILE__, __LINE__, data.block[0] );
        if( length < data.block[0] && debug )
            printf( "\t\007TRUNCATING: %d bytes read, space only for %d bytes passed in\n",
                    data.block[0], length );
    }

    // Copy only the bytes from the data-portion of the block read in down into
    // the passed-in storage area (that is, skip the leading 'returned length' byte).
    for( i = 1; i <= data.block[0] && i <= length; i++ )
        values[i - 1] = data.block[i];

    return data.block[0];
}

////////////////////////////////////////////////////////////////////////
///
/// @brief
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
#if 0
static I2cReturn_t i2c_smbus_write_i2c_block_data( int file, UINT8 command,
                                                   UINT8 length, UINT8 *values )
{
    union i2c_smbus_data data;
    int i;
    if( length > 32 )
        length = 32;
    for( i = 1; i <= length; i++ )
        data.block[i] = values[i - 1];
    data.block[0] = length;
    return i2c_smbus_access( file, I2C_SMBUS_WRITE, command,
                             I2C_SMBUS_I2C_BLOCK_BROKEN, &data );
}
#endif

////////////////////////////////////////////////////////////////////////
///
/// @brief
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
/* Returns the number of read bytes */
static I2cReturn_t i2c_smbus_block_process_call( int file, UINT8 command,
                                                 UINT8 length, UINT8 *values )
{
    union i2c_smbus_data data;
    int i;
    if( length > 32 )
        length = 32;
    for( i = 1; i <= length; i++ )
        data.block[i] = values[i - 1];
    data.block[0] = length;
    if( i2c_smbus_access( file, I2C_SMBUS_WRITE, command,
                          I2C_SMBUS_BLOCK_PROC_CALL, &data ) )
        return -1;
    else
    {
        for( i = 1; i <= data.block[0]; i++ )
            values[i - 1] = data.block[i];
    }
    return data.block[0];
}

////////////////////////////////////////////////////////////////////////
/// i2c_transport_channel_initialize_system
/// @brief MUST be called at time of over-all system initialization. NOTE THAT
/// this is NO LONGER "lazy-called" via the boolean 'sI2cTransportInitialized',
/// set at boot-time to 'false'. DUE TO boolean debug flag being passed-in from
/// above now (dynamic at run-time).
/// @return void
////////////////////////////////////////////////////////////////////////
GLOBAL void i2c_transport_channel_initialize_system()
{
    i2c_transport_channel_info_t* pChan = sI2cTransportChannels;
    int i;

    if( sI2cTransportInitialized )
        return;
    for( i = 0; i < I2C_TRANSPORT_CHANNEL_MAX_CHANNELS; i++, pChan++ )
    {
        pChan->mDesc = i;   // so a ptr. to this struct can know its descriptor
        memset( pChan->mDevName, 0, sizeof( pChan->mDevName ) );
        pChan->mErrno = 0;
        pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_UNUSED;
        pChan->mUsesPec = pChan->mDebug = FALSE;
    }
    sI2cTransportInitialized = TRUE;
}

////////////////////////////////////////////////////////////////////////
/// i2c_transport_channel_get_errno
/// @brief Returns the error code associated with the last call on this handle
/// @param I2cHandle_t handle
/// @return SINT32 NEGATIVE errno value; -ENXIO iff a bad handle passed in
////////////////////////////////////////////////////////////////////////
GLOBAL SINT32 i2c_transport_channel_get_errno( I2cHandle_t handle )
{
    if( !I2C_HANDLE_IN_RANGE( handle ) )
        return -ENXIO;
    return -sI2cTransportChannels[handle].mErrno;
}

////////////////////////////////////////////////////////////////////////
/// i2c_transport_channel_get_info
/// @brief Returns a pointer to the operating parameters associated with this handle
/// @param I2cHandle_t handle
/// @return i2c_transport_channel_info_t*, or NULL (if handle out of range, or not init'ed)
////////////////////////////////////////////////////////////////////////
GLOBAL i2c_transport_channel_info_t* i2c_transport_channel_get_info( I2cHandle_t handle, char *errStr )
{
    if( !sI2cTransportInitialized )
    {
        sprintf( errStr, "%s:%d: Call to i2c_transport_channel_initialize_system() not made yet!\n",
                 __FILE__, __LINE__ );
        return NULL;
    }
    if( !I2C_HANDLE_IN_RANGE( handle ) )
    {
        sprintf( errStr, "bad handle %d\n", ( int )handle );
        return NULL;
    }
    return &sI2cTransportChannels[handle];
}

////////////////////////////////////////////////////////////////////////
/// i2c_transport_channel_open
/// @brief Opens a unique transport "channel" to a specific I2C-connected device.
/// @param UINT8 I2cAdapterNumber: a value from 0 to X, as used in "/dev/i2c-X".
/// @param UINT8 BusAddress: the address of the device on the SMBus. The even-numbered
/// address is used to write to the device; the odd-numbered address is for reading.
/// We store the address as the (even) 'write' address, regardless of whether the
/// read or write address is passed in. It MUST be passed in as an 8-bit address!!!
/// @param bool_t UsesPec - Set to TRUE iff all transfers must have error correction
/// @param bool_t UsesSmbusProtocol - Set to FALSE if no "combined" transfers allowed,
/// doesn't follow the SMBUS Protocol standard
/// @return I2cHandle_t; On Success-a "small" positive unique descriptor
/// On Error: a negative value of 'errno', as follows:
/// -ENOPKG: i2c_transport_channel_initialize_system() not called yet ("package not installed")
/// -ENFILE: Max. number of open transport channels already
/// -ENOENT: /dev/i2c-N would not 'open'
////////////////////////////////////////////////////////////////////////
GLOBAL I2cHandle_t i2c_transport_channel_open( UINT8 I2cAdapterNumber,
                                               UINT8 BusAddress, bool_t UsesPec,
                                               bool_t UsesSmbusProtocol, bool_t Debug )
{
    I2cHandle_t handle;
    char DevName[I2C_DEVICE_MAX_NAME_LENGTH];

    if( !sI2cTransportInitialized )
    {
        if( Debug )
            printf( "%s:%d: Call to i2c_transport_channel_initialize_system() not made yet!\n",
                    __FILE__, __LINE__ );
        return -ENOPKG;
    }

    memset( DevName, 0, I2C_DEVICE_MAX_NAME_LENGTH );   // NIL out the buffer
    snprintf( DevName, I2C_DEVICE_MAX_NAME_LENGTH - 1,
              "%s%d", I2C_DEVICE_BASE_NAME, I2cAdapterNumber );
    if( Debug )
        printf( "i2c_transport_channel_open( I2cAdapterNumber=%d, BusAddress=0x%02X,"
                " .., UsesPec=%d) strlen(I2C_DEVICE_BASE_NAME)=%d, DevName: %s\n",
                ( int )I2cAdapterNumber, ( int )BusAddress, ( int )UsesPec,
                strlen( I2C_DEVICE_BASE_NAME ), DevName );

    // find if already open, or if not an empty slot in our transport channel table
    i2c_transport_channel_info_t* pChan = sI2cTransportChannels;
    int i;
    // is this exact transport channel already opened?
    for( i = 0; i < I2C_TRANSPORT_CHANNEL_MAX_CHANNELS; i++, pChan++ )
    {
        if( ( strcmp( pChan->mDevName, DevName ) == 0 )
            && pChan->mBusAddress == BusAddress
            && pChan->mUsesPec == UsesPec )
        {
            if( Debug )
                printf( "Transport channel (%s,0x%02X) already open\n", DevName, BusAddress );
            return pChan->mDesc;
        }
    }
    // look for an empty slot instead
    pChan = sI2cTransportChannels;
    for( i = 0; i < I2C_TRANSPORT_CHANNEL_MAX_CHANNELS; i++, pChan++ )
    {
        // look for an unused slot
        if( pChan->mState == I2C_TRANSPORT_CHANNEL_STATE_UNUSED )
            break;
    }
    if( i == I2C_TRANSPORT_CHANNEL_MAX_CHANNELS )
    {
        // no empty slots
        if( Debug )
            printf( "%s:%d: out of transport channel storage\n", __FILE__, __LINE__ );
        return( -ENFILE );
    }
    pChan->mDesc = i;           // the handle is the index into this array
    handle = pChan->mDesc;      // we'll return this value

    // build & copy the device name into the info structure
    memset( pChan->mDevName, 0, I2C_DEVICE_MAX_NAME_LENGTH );   // NIL out the buffer
    memcpy( pChan->mDevName, DevName, strlen( DevName ) );
    pChan->mDebug = Debug;

    // is the device already open?
    for( i = 0; i < I2C_TRANSPORT_CHANNEL_MAX_CHANNELS; i++ )
    {
        // look for this device name in an open channel
        if( sI2cTransportChannels[i].mState == I2C_TRANSPORT_CHANNEL_STATE_UNUSED )
            continue;       // channel not in use, ignore any devname left there
        if( strcmp( pChan->mDevName, sI2cTransportChannels[i].mDevName ) == 0 )
            break;      // channel's in use, name matches-this "/dev" is already opened
    }
    if( i == I2C_TRANSPORT_CHANNEL_MAX_CHANNELS )
    {
        // device not already opened, so open it
        pChan->mFd = open( pChan->mDevName, O_RDWR );
        if( pChan->mFd < 0 )
        {
            /* ERROR HANDLING; you can check errno to see what went wrong */
            if( Debug )
                printf( "%s:%d: couldn't open device; errno=%d\n", __FILE__, __LINE__, errno );
            pChan->mErrno = errno;          // why did we fail exactly???
            pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
            return( -ENOENT );
        }
    }
    else
    {
        // device already opened, so don't re-open it (that would likely fail). Instead,
        // use the file descriptor already assigned to this "/dev"
        pChan->mFd = sI2cTransportChannels[i].mFd;
    }
    pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_OPEN;   // mark it as in use
    pChan->mBusAddress = BusAddress;     // this is the 7-bit address, w/o read(1),write(0) lsb!
    pChan->mUsesPec = UsesPec;
    pChan->mUsesSmbusProtocol = UsesSmbusProtocol;

    if( Debug )
        printf( "%s:%d: i2c_transport_channel_open(%d, 0x%02X, %d, %d) "
                "- Devname:%s, mDesc:%d, mFd:%d, mBusAddr:0x%02X \n",
                __FILE__, __LINE__, I2cAdapterNumber, BusAddress, UsesPec, UsesSmbusProtocol,
                pChan->mDevName, ( int )pChan->mDesc, ( int )pChan->mFd, ( int )pChan->mBusAddress );

    return handle;
}

////////////////////////////////////////////////////////////////////////
/// i2c_transport_channel_close
/// @brief Returns the error code associated with the last call on this handle
/// @param I2cHandle_t handle
/// @return I2cReturn_t - 0 if success
///     -ENOPKG: i2c_transport_channel_initialize_system() not called yet ("package not installed")
/// -ENXIO iff a bad handle passed in
/// whatever errno was if it fails (as a negative value)
////////////////////////////////////////////////////////////////////////
GLOBAL I2cReturn_t i2c_transport_channel_close( I2cHandle_t handle )
{
    if( !sI2cTransportInitialized )
    {
        printf( "%s:%d: Call to i2c_transport_channel_initialize_system() not made yet!\n",
                __FILE__, __LINE__ );
        return -ENOPKG;
    }
    if( !I2C_HANDLE_IN_RANGE( handle ) )
        return ENXIO;

    i2c_transport_channel_info_t* pChan = i2c_transport_channel_get_info( handle, tmperr );
    if( !pChan )
    {
        perror( "no i2c_transport_channel info" );
        return ENXIO;
    }
    if( pChan->mState == I2C_TRANSPORT_CHANNEL_STATE_UNUSED )
        return 0;               // it's already closed

    pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_UNUSED; // free up the slot

    int i;
    // don't close the file assoc. w/this handle if any other channel is still using it
    for( i = 0; i < I2C_TRANSPORT_CHANNEL_MAX_CHANNELS; i++ )
    {
        // look for this device name in an open channel
        if( pChan == &sI2cTransportChannels[i] )
            continue;                   // yea; that's us
        if( sI2cTransportChannels[i].mState == I2C_TRANSPORT_CHANNEL_STATE_UNUSED )
            continue;                   // channel not in use
        // found an open channel (not ours); is it using the same device?
        if( strcmp( pChan->mDevName, sI2cTransportChannels[i].mDevName ) == 0 )
            break;      // channel's in use, name matches-this "/dev" is in multiple use
    }
    if( i == I2C_TRANSPORT_CHANNEL_MAX_CHANNELS )
    {
        // nobody else using it; we can close it
        if( close( pChan->mFd ) < 0 )
        {
            if( pChan->mDebug )
                printf( "%s:%d: Failed to close device; errno=%d\n", __FILE__, __LINE__, errno );
            pChan->mErrno = errno;          // why did we fail exactly???
            return -pChan->mErrno;
        }
    }
    else
    {
        // somebody else is using this "/dev" device, so we'll just quietly walk away...
        ;
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////
/// i2c_transport_channel_read
/// @brief Reads the amount of data from the peripheral as specified
/// @param I2cHandle_t handle
/// @param I2cLocation_t location: Register address of the I2C device to read from;
/// for the type "Receive 1 Byte", this should be 'special' comand register
///     I2C_TRANSPORT_CHANNEL_SEND_OR_RECEIVE_COMMAND
/// @param void* pBuf: where to store the result. For 1 byte, it's a UINT8*; for 2 bytes,
/// it's a UINT16*; for all else, it's a UINT8* (buffer).
/// @param UINT32 size: the number of bytes desired to read; for a block read, if
/// you don't know how many you'll read, make it I2C_TRANSPORT_CHANNEL_MAX_BLOCK_READ_SIZE.
/// @return I2cReturn_t - Number of bytes read if success
///     -ENOPKG: i2c_transport_channel_initialize_system() not called yet ("package not installed")
/// -ENXIO iff a bad handle passed in
/// -EINVAL iff block read w/a size of 0 or too big
/// whatever errno was if it fails (as a negative value); see ioctl return codes
////////////////////////////////////////////////////////////////////////
GLOBAL I2cReturn_t i2c_transport_channel_read( I2cHandle_t handle,
                                               I2cLocation_t location, void* pBuf, UINT32 size )
{
    I2cReturn_t rc;

    if( !sI2cTransportInitialized )
      {
	diag_verbose(VVVERBOSE, verbosity, "%s:%d: Call to i2c_transport_channel_initialize_system() not made yet!\n",  __FILE__, __LINE__ );
        return -ENOPKG;
      }
    if( !I2C_HANDLE_IN_RANGE( handle ) )
      {
        return -ENXIO;
      }

    i2c_transport_channel_info_t* pChan = i2c_transport_channel_get_info( handle, tmperr );
    if( !pChan )
    {
        return -ENXIO;
    }

    if( pChan->mDebug )
      diag_verbose(VVVERBOSE, verbosity,"%s:%d: i2c_transport_channel_read( handle=%d, location=0x%02X, .., size=%d)\n",   __FILE__, __LINE__, ( int )handle, ( int )location, ( int )size );

    // FIRST, set the bus address we'll be talking to!
//    rc = ioctl( pChan->mFd, I2C_SLAVE, pChan->mBusAddress );
    rc = ioctl( pChan->mFd, I2C_SLAVE_FORCE, pChan->mBusAddress );
    if( rc < 0 )
    {
        if( pChan->mDebug )
	  diag_verbose(VVVERBOSE, verbosity, "%s:%d: Failed to set I2C bus address for %d to %d; errno=%d\n",  __FILE__, __LINE__, ( int )pChan->mFd, ( int )pChan->mBusAddress, errno );
        pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
        pChan->mErrno = errno;
        return -pChan->mErrno;
    }

    // SECOND, set the error correction protocol for this xfer
    rc = ioctl( pChan->mFd, I2C_PEC, pChan->mUsesPec );
    if( rc < 0 )
    {
        if( pChan->mDebug )
	  diag_verbose (VVVERBOSE, verbosity, "%s:%d: Failed to set I2C error correction state; errno=%d\n", __FILE__, __LINE__, errno );
        pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
        pChan->mErrno = errno;
        return -pChan->mErrno;
    }

    // NOW do the actual READ operation
    if( pChan->mUsesSmbusProtocol == FALSE )
    {
        rc = i2c_read_data( pChan->mFd, pChan->mBusAddress,
                            location, size, ( UINT8* )pBuf, pChan->mDebug );
        if( pChan->mDebug )
	  diag_verbose (VVVERBOSE, verbosity, "%s:%u: I2C raw data read returned %d\n",  __FILE__, __LINE__, ( int )rc );
        return rc;
    }

    // do an SMBUS protocol read
    switch( size )
    {
    case 1: // I2C_TRANSPORT_CHANNEL_ACCESS_RCV_BYTE, I2C_TRANSPORT_CHANNEL_ACCESS_READ_BYTE
        if( location == I2C_TRANSPORT_CHANNEL_SEND_OR_RECEIVE_COMMAND )
        {
            // special case; receiving a byte w/o sending a command register address 1st
            rc = i2c_smbus_read_byte( pChan->mFd );
        }
        else
        {
            // read 1 byte from a specific command register
            rc = i2c_smbus_read_byte_data( pChan->mFd, location );
        }
        if( rc < 0 )
        {
            if( pChan->mDebug )
	      diag_verbose (VVVERBOSE, verbosity, "%s:%d: Failed to read 1 byte; errno=%d\n", __FILE__, __LINE__, errno );
            pChan->mErrno = errno;          // why did we fail exactly???
            pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
            return -pChan->mErrno;
        }
        // single-byte read succeeded
        *( ( UINT8* )pBuf ) = ( UINT8 )rc;
        return 1;

    case 2: // I2C_TRANSPORT_CHANNEL_ACCESS_READ_2_BYTES
        rc = i2c_smbus_read_word_data( pChan->mFd, location );
        if( pChan->mDebug )
	  diag_verbose (VVVERBOSE, verbosity, "%s:%d: READ of 2 bytes returned 0x%04X\n", __FILE__, __LINE__, ( int )rc );
        if( rc < 0 )
        {
            if( pChan->mDebug )
	      diag_verbose (VVVERBOSE, verbosity, "%s:%d: Failed to read 2 bytes; errno=%d\n", __FILE__, __LINE__, errno );
            pChan->mErrno = errno;          // why did we fail exactly???
            pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
            return -pChan->mErrno;
        }
        // two-byte read succeeded
        *( ( UINT16* )pBuf ) = ( UINT16 )rc;
        return 2;

    default:    // I2C_TRANSPORT_CHANNEL_ACCESS_BLOCK_READ
        // TODO "NEED A FLAG FOR SMBUS READ OR RAW-I2C READ!

        if( !size || size > I2C_TRANSPORT_CHANNEL_MAX_BLOCK_READ_SIZE )
        {
	  if( pChan->mDebug )
	    diag_verbose (VVVERBOSE, verbosity,  "%s:%u: invalid block read size; size=%lu\n", __FILE__, __LINE__, size );
	  pChan->mErrno = EINVAL;     // why did we fail exactly???
	  pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
	  return -pChan->mErrno;
        }
        rc = i2c_smbus_read_i2c_block_data( pChan->mFd, location, size,
                                            ( UINT8* )pBuf, pChan->mDebug );
        if( rc < 0 )
        {
            if( pChan->mDebug )
	      diag_verbose (VVVERBOSE, verbosity, "%s:%d: Failed to read block of %d bytes from fd %d; rc=%d, errno=%d\n",  __FILE__, __LINE__, ( int )size, ( int )pChan->mFd, ( int )rc, errno );
            pChan->mErrno = errno;          // why did we fail exactly???
            pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
            return -pChan->mErrno;
        }
        // multi-byte read succeeded, data already copied into output buffer
        return rc;
    }
}

////////////////////////////////////////////////////////////////////////
/// i2c_transport_channel_write
/// @brief Writes the amount of data from the peripheral as specified
/// @param I2cHandle_t handle
/// @param I2cLocation_t location: Register address of the I2C device to write to;
/// for the type "Write 1 Byte", this should be 'special' comand register
///     I2C_TRANSPORT_CHANNEL_SEND_OR_RECEIVE_COMMAND
/// @param void* pBuf: data to write. For 1 byte, it's a UINT8*; for 2 bytes,
/// it's a UINT16*; for all else, it's a UINT8* (buffer).
/// @param UINT32 size: the number of bytes desired to write.
/// @return I2cReturn_t - Number of bytes written if success
///     -ENOPKG: i2c_transport_channel_initialize_system() not called yet ("package not installed")
/// -ENXIO iff a bad handle passed in
/// -EINVAL iff block read w/a size of 0 or too big
/// whatever errno was if it fails (as a negative value); see ioctl return codes
////////////////////////////////////////////////////////////////////////
GLOBAL I2cReturn_t i2c_transport_channel_write( I2cHandle_t handle,
                                                I2cLocation_t location, void* pBuf, UINT32 size )
{
    I2cReturn_t rc;

    if( !sI2cTransportInitialized )
    {
        printf( "%s:%d: Call to i2c_transport_channel_initialize_system() not made yet!\n",
                __FILE__, __LINE__ );
        return -ENOPKG;
    }
    if( !I2C_HANDLE_IN_RANGE( handle ) )
    {
        return -ENXIO;
    }

    i2c_transport_channel_info_t* pChan = i2c_transport_channel_get_info( handle, tmperr );
    if( !pChan )
    {
        return -ENXIO;
    }

    if( pChan->mDebug )
        printf( "%s:%d: i2c_transport_channel_write( handle=%d, location=0x%02X, .., size=%lu)\n",
                __FILE__, __LINE__, ( int )handle, ( unsigned int )location, size );

    // FIRST, set the bus address we'll be talking to!
//    rc = ioctl( pChan->mFd, I2C_SLAVE, pChan->mBusAddress );
    rc = ioctl( pChan->mFd, I2C_SLAVE_FORCE, pChan->mBusAddress );
    if( rc < 0 )
    {
        if( pChan->mDebug )
            printf( "%s:%d: Failed to set I2C bus address; errno=%d\n", __FILE__, __LINE__, errno );
        pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
        pChan->mErrno = errno;
        return -pChan->mErrno;
    }

    // SECOND, set the error correction protocol for this xfer
    rc = ioctl( pChan->mFd, I2C_PEC, pChan->mUsesPec );
    if( rc < 0 )
    {
        if( pChan->mDebug )
            printf( "%s:%d: Failed to set I2C error correction state; errno=%d\n", __FILE__, __LINE__, errno );
        pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
        pChan->mErrno = errno;
        return -pChan->mErrno;
    }

    // NOW do the actual WRITE operation
    if( pChan->mUsesSmbusProtocol == FALSE )
    {
        rc = i2c_write_data( pChan->mFd, pChan->mBusAddress,
                             location, size, ( UINT8* )pBuf, pChan->mDebug );
        if( pChan->mDebug )
            printf( "%s:%u: I2C raw data write returned %d\n",
                    __FILE__, __LINE__, ( int )rc );
        return rc;
    }
    // do an SMBUS Protocol write
    switch( size )
    {
    case 1: // I2C_TRANSPORT_CHANNEL_ACCESS_SEND_BYTE, I2C_TRANSPORT_CHANNEL_ACCESS_WRITE_BYTE
        if( location == I2C_TRANSPORT_CHANNEL_SEND_OR_RECEIVE_COMMAND )
        {
            // special case; sending a byte w/o sending a command register address 1st
            rc = i2c_smbus_write_byte( pChan->mFd, *( ( UINT8* )pBuf ) );
        }
        else
        {
            // write 1 byte to a specific command register
            rc = i2c_smbus_write_byte_data( pChan->mFd, location, *( ( UINT8* )pBuf ) );
        }
        if( rc < 0 )
        {
            if( pChan->mDebug )
                printf( "%s:%d: Failed to write 1 byte; errno=%d\n", __FILE__, __LINE__, errno );
            pChan->mErrno = errno;          // why did we fail exactly???
            pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
            return -pChan->mErrno;
        }
        break;

    case 2: // I2C_TRANSPORT_CHANNEL_ACCESS_WRITE_2_BYTES
        rc = i2c_smbus_write_word_data( pChan->mFd, location, *( ( UINT16* )pBuf ) );
        if( rc < 0 )
        {
            if( pChan->mDebug )
                printf( "%s:%d: Failed to write 2 bytes; errno=%d\n", __FILE__, __LINE__, errno );
            pChan->mErrno = errno;          // why did we fail exactly???
            pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
            return -pChan->mErrno;
        }
        break;

    default:    // I2C_TRANSPORT_CHANNEL_ACCESS_BLOCK_WRITE
        if( !size || size > I2C_TRANSPORT_CHANNEL_MAX_BLOCK_READ_SIZE )
        {
            if( pChan->mDebug )
                printf( "%s:%d: invalid block write size; size=%lu\n", __FILE__, __LINE__, size );
            pChan->mErrno = EINVAL;     // why did we fail exactly???
            pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
            return -pChan->mErrno;
        }
        rc = i2c_smbus_write_block_data( pChan->mFd, location, size, ( UINT8* )pBuf );
        if( rc < 0 )
        {
            if( pChan->mDebug )
                printf( "%s:%d: Failed to write block of bytes; errno=%d\n",
                        __FILE__, __LINE__, errno );
            pChan->mErrno = errno;          // why did we fail exactly???
            pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
            return -pChan->mErrno;
        }
        break;
    }
    return size;
}


////////////////////////////////////////////////////////////////////////
/// i2c_transport_channel_write_then_read
/// @brief Writes the amount of data from the peripheral as specified, then
/// Reads the amount of data from the peripheral as specified. This is a.k.a.
/// either a "Process call" (2 bytes) or "Block write-Block read Process call".
/// @param I2cHandle_t handle
/// @param I2cLocation_t location: Register address of the I2C device to read from;
/// for the type "Receive 1 Byte", this should be 'special' comand register
///     I2C_TRANSPORT_CHANNEL_SEND_OR_RECEIVE_COMMAND
/// @param UINT8* pWrBuf: data to write
/// @param UINT32 WrSize: the number of bytes desired to write.
/// @param UINT8* pRdBuf: where to store the result
/// @param UINT32 RdSize: the number of bytes desired to read; for a block read, if
/// you don't know how many you'll read, make it I2C_TRANSPORT_CHANNEL_MAX_BLOCK_READ_SIZE.
/// @return I2cReturn_t - Number of bytes read if success
///     -ENOPKG: i2c_transport_channel_initialize_system() not called yet ("package not installed")
/// -ENXIO iff a bad handle passed in
/// -EINVAL iff block read w/a size of 0 or too big
/// whatever errno was if it fails (as a negative value); see ioctl return codes
////////////////////////////////////////////////////////////////////////
GLOBAL I2cReturn_t i2c_transport_channel_write_then_read(
    I2cHandle_t handle, I2cLocation_t location,
    void* pWrBuf, UINT32 WrSize, void* pRdBuf, UINT32 RdSize )
{
    I2cReturn_t rc;
    UINT8 lBuf[I2C_TRANSPORT_CHANNEL_MAX_BLOCK_READ_SIZE];
    unsigned int i;

    if( !sI2cTransportInitialized )
    {
        printf( "%s:%d: Call to i2c_transport_channel_initialize_system() not made yet!\n",
                __FILE__, __LINE__ );
        return -ENOPKG;
    }
    if( !I2C_HANDLE_IN_RANGE( handle ) )
    {
        return -ENXIO;
    }

    i2c_transport_channel_info_t* pChan = i2c_transport_channel_get_info( handle, tmperr );
    if( !pChan )
    {
        return -ENXIO;
    }

    if( pChan->mDebug )
        printf( "%s:%d: i2c_transport_channel_write_then_read( handle=%d, location=0x%02X, .., WrSize=%lu, RdSize=%lu)\n",
                __FILE__, __LINE__, ( int )handle, ( unsigned int )location, WrSize, RdSize );

    // FIRST, set the bus address we'll be talking to!
//    pChan->mErrno = ioctl( pChan->mFd, I2C_SLAVE, pChan->mBusAddress );
    pChan->mErrno = ioctl( pChan->mFd, I2C_SLAVE_FORCE, pChan->mBusAddress );
    if( pChan->mErrno < 0 )
    {
        if( pChan->mDebug )
            printf( "%s:%d: Failed to set I2C bus address; errno=%d\n", __FILE__, __LINE__, errno );
        pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
        return -pChan->mErrno;
    }

    // SECOND, set the error correction protocol for this xfer
    pChan->mErrno = ioctl( pChan->mFd, I2C_PEC, pChan->mUsesPec );
    if( pChan->mErrno < 0 )
    {
        if( pChan->mDebug )
            printf( "%s:%d: Failed to set I2C error correction state; errno=%d\n", __FILE__, __LINE__, errno );
        pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
        return -pChan->mErrno;
    }

    // NOW do the actual WRITE followed by the READ operation
    if( WrSize == 2 && RdSize == 2 )
    {
        // simpler "Process call" case
        rc = i2c_smbus_process_call( pChan->mFd, location, *( ( UINT16* )pWrBuf ) );
        if( rc < 0 )
        {
            if( pChan->mDebug )
                printf( "%s:%d: Failed to write or read a 2 byte block; errno=%d\n", __FILE__, __LINE__, errno );
            pChan->mErrno = errno;          // why did we fail exactly???
            pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
            return -pChan->mErrno;
        }
        // two-byte write followed by a 2-byte read succeeded
        *( ( UINT16* )pRdBuf ) = ( UINT16 )rc;
        return 2;
    }

    // Is a Block-write / Block-read Process call
    if( !WrSize || !RdSize
        || ( WrSize + RdSize ) > I2C_TRANSPORT_CHANNEL_MAX_BLOCK_READ_SIZE )
    {
        if( pChan->mDebug )
            printf( "%s:%d: invalid block-write/block-read size; WrSize=%lu, RdSize=%lu\n",
                    __FILE__, __LINE__, WrSize, RdSize );
        pChan->mErrno = EINVAL;     // why did we fail exactly???
        pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
        return -pChan->mErrno;
    }
    // The underlying call uses the same buffer to read into that it wrote from;
    // use a local buffer then, so we don't overwrite caller's buffer.
    UINT8* lpBuf = ( UINT8* )pWrBuf;    // 'local' pointer: 'lp'
    for( i = 0; i < WrSize; i++ )
        lBuf[i] = *lpBuf++;
    rc = i2c_smbus_block_process_call( pChan->mFd, location, WrSize, lBuf );
    if( rc < 0 )
    {
        if( pChan->mDebug )
            printf( "%s:%d: Failed to write block/read block of bytes; errno=%d\n",
                    __FILE__, __LINE__, errno );
        pChan->mErrno = errno;          // why did we fail exactly???
        pChan->mState = I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR;
        return -pChan->mErrno;
    }
    // read data is in 'lBuf', # bytes read in 'rc'; copy to 'pRdBuf'
    lpBuf = ( UINT8* )pRdBuf;
    for( i = 0; i < (unsigned int)rc && i < I2C_TRANSPORT_CHANNEL_MAX_BLOCK_READ_SIZE; i++ )
        *lpBuf++ = lBuf[i];

    return rc;
}




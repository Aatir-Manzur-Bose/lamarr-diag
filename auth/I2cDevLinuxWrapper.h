////////////////////////////////////////////////////////////////////////////////
/// @file          I2cDevLinuxWrapper.h
/// @brief  Definitions of pure C-level functions which form the User
///         Space API to the I2C channels present in Linux Kernel Space.
/// That is, this layer calls into "/dev/i2c-N", so no application-layer
/// code ever need (nor dare) to.
/// This file also serves to wrap the linux i2c include files i2c.h & i2c-dev.h.
/// @version       %Version:1 %
/// @author        dv15727
/// @date          %Created: Feb 13, 2012%
//
//  Copyright 2012 Bose Corporation, Framingham, MA
//
///////////////////////////////////////////////////////////////////////////////////

#ifndef __i2c_dev_linux_wrapper_h__
#define __i2c_dev_linux_wrapper_h__

#include <RTOS.h>

/*
 *  local defines
 */

typedef int32_t             I2cHandle_t;
typedef int32_t             I2cReturn_t;
typedef uint32_t           I2cLocation_t;

#define GLOBAL
#define I2C_DEVICE_BASE_NAME                "/dev/i2c-"
#define I2C_DEVICE_MAX_NAME_LENGTH          32
#define I2C_TRANSPORT_CHANNEL_MAX_CHANNELS      8
#define I2C_TRANSPORT_CHANNEL_MAX_BLOCK_READ_SIZE   I2C_SMBUS_BLOCK_MAX

#define I2C_TRANSPORT_CHANNEL_SEND_OR_RECEIVE_COMMAND   0x100

typedef enum
{
    I2C_TRANSPORT_CHANNEL_STATE_UNUSED,     // 0
    I2C_TRANSPORT_CHANNEL_STATE_OPEN,       // 1
    I2C_TRANSPORT_CHANNEL_STATE_IN_ERROR    // 2
} I2C_TRANSPORT_CHANNEL_STATE;

typedef unsigned char bool_t;

// any "/dev/i2c-N" device can have multiple slave chips wired to it, ea. at their own
// unique address (pair; even is WRITE (lsb=0), odd is READ (lsb=1). Each such combo is
// hereafter called a "transport channel". Once a "/dev/i2c-N" has been opened, an ioctl
// must set the slave address to talk to. We'll save that info, such that client code can
// call 'i2c_transport_channel_write' without having to set the slave address every time.
typedef struct i2c_transport_channel_info
{
    UINT32 mDesc;           // "small", unique descriptor for this I2C device/address combo
    SINT32 mFd;             // file descriptor associated w/this
    UINT32 mBusAddress;         // the device's I2C Write (even) bus address (odd for Read)
    char mDevName[I2C_DEVICE_MAX_NAME_LENGTH];
    SINT32 mErrno;          // last return from a failed system call; stored as positive, returned as NEGATIVE!!
//    UINT32 mSpeed;            // in bits/sec.
    I2C_TRANSPORT_CHANNEL_STATE mState;
    bool_t mUsesPec;            // TRUE iff error correction required
    bool_t mUsesSmbusProtocol;  // false if no "combined" transactions allowed
    bool_t mDebug;
} i2c_transport_channel_info_t;

#define I2C_HANDLE_IN_RANGE(h)  ((h >= 0) && (h < I2C_TRANSPORT_CHANNEL_MAX_CHANNELS))
//#define i2c_transport_channel_get_info(h) (&sI2cTransportChannels[h])


/****#### dv15727: the following is copied from <linux/i2c.h>:
 * ######################################################################
 * struct i2c_msg - an I2C transaction segment beginning with START
 * @addr: Slave address, either seven or ten bits.  When this is a ten
 *  bit address, I2C_M_TEN must be set in @flags and the adapter
 *  must support I2C_FUNC_10BIT_ADDR.
 * @flags: I2C_M_RD is handled by all adapters.  No other flags may be
 *  provided unless the adapter exported the relevant I2C_FUNC_*
 *  flags through i2c_check_functionality().
 * @len: Number of data bytes in @buf being read from or written to the
 *  I2C slave address.  For read transactions where I2C_M_RECV_LEN
 *  is set, the caller guarantees that this buffer can hold up to
 *  32 bytes in addition to the initial length byte sent by the
 *  slave (plus, if used, the SMBus PEC); and this value will be
 *  incremented by the number of block data bytes received.
 * @buf: The buffer into which data is read, or from which it's written.
 *
 * An i2c_msg is the low level representation of one segment of an I2C
 * transaction.  It is visible to drivers in the @i2c_transfer() procedure,
 * to userspace from i2c-dev, and to I2C adapter drivers through the
 * @i2c_adapter.@master_xfer() method.
 *
 * Except when I2C "protocol mangling" is used, all I2C adapters implement
 * the standard rules for I2C transactions.  Each transaction begins with a
 * START.  That is followed by the slave address, and a bit encoding read
 * versus write.  Then follow all the data bytes, possibly including a byte
 * with SMBus PEC.  The transfer terminates with a NAK, or when all those
 * bytes have been transferred and ACKed.  If this is the last message in a
 * group, it is followed by a STOP.  Otherwise it is followed by the next
 * @i2c_msg transaction segment, beginning with a (repeated) START.
 *
 * Alternatively, when the adapter supports I2C_FUNC_PROTOCOL_MANGLING then
 * passing certain @flags may have changed those standard protocol behaviors.
 * Those flags are only for use with broken/nonconforming slaves, and with
 * adapters which are known to support the specific mangling options they
 * need (one or more of IGNORE_NAK, NO_RD_ACK, NOSTART, and REV_DIR_ADDR).
 */
#include <linux/i2c.h>              // defines struct i2c_msg,
// I2C_FUNC_*, I2C_FUNC_SMBUS_*,
// I2C_SMBUS_* defines

#include <linux/i2c-dev.h>          // defines /dev/i2c-X ioctl cmds,
// struct i2c_smbus_ioctl_data,
// struct i2c_rdwr_ioctl_data

typedef enum
{
    I2C_TRANSPORT_CHANNEL_ACCESS_WRITE_QUICK = 1,
    I2C_TRANSPORT_CHANNEL_ACCESS_SEND_BYTE,
    I2C_TRANSPORT_CHANNEL_ACCESS_RCV_BYTE,
    I2C_TRANSPORT_CHANNEL_ACCESS_WRITE_BYTE,
    I2C_TRANSPORT_CHANNEL_ACCESS_READ_BYTE,
    I2C_TRANSPORT_CHANNEL_ACCESS_WRITE_2_BYTES,
    I2C_TRANSPORT_CHANNEL_ACCESS_READ_2_BYTES,
    I2C_TRANSPORT_CHANNEL_ACCESS_WRITE2_READ2,  // "process call"
    I2C_TRANSPORT_CHANNEL_ACCESS_BLOCK_WRITE,
    I2C_TRANSPORT_CHANNEL_ACCESS_BLOCK_READ,
    I2C_TRANSPORT_CHANNEL_ACCESS_BLOCK_WRITE_READ,
    I2C_TRANSPORT_CHANNEL_ACCESS_HOST_NOTIFY,
} I2C_TRANSPORT_CHANNEL_ACCESS_TYPE;


// ALL THE GLOBALLY-AVAILABLE (extern) CALLS HAVE AN INTERNAL GUARD TO DETECT THAT
// 'i2c_transport_channel_initialize_system()' HAS ALREADY BEEN CALLED!

extern void i2c_transport_channel_initialize_system();

extern SINT32 i2c_transport_channel_get_errno( I2cHandle_t handle );

extern i2c_transport_channel_info_t* i2c_transport_channel_get_info( I2cHandle_t handle, char *buf );

extern I2cHandle_t i2c_transport_channel_open( UINT8 I2cAdapterNumber,
                                               UINT8 SmBusAddress, bool_t UsesPec,
                                               bool_t UsesSmbusProtocol, bool_t Debug );

extern I2cHandle_t i2c_transport_channel_close( I2cHandle_t handle );

extern I2cReturn_t i2c_transport_channel_read( I2cHandle_t handle,
                                               I2cLocation_t location, void* pBuf, UINT32 size );

extern I2cReturn_t i2c_transport_channel_write( I2cHandle_t handle,
                                                I2cLocation_t location, void* pBuf, UINT32 size );

extern I2cReturn_t i2c_transport_channel_write_then_read(
    I2cHandle_t handle, I2cLocation_t location,
    void* pWrBuf, UINT32 WrSize, void* pRdBuf, UINT32 RdSize );


// THESE ARE ALL THE LOCAL (static) FUNCTIONS USED ONLY WITHIN I2cDevLinuxWrapper.c

#if 0       // these are locals to c file, not externs
extern I2cReturn_t i2c_smbus_access( int file, char read_write, UINT8 command,
                                     int size, union i2c_smbus_data *data );

extern I2cReturn_t i2c_smbus_write_quick( int file, UINT8 value );

extern I2cReturn_t i2c_smbus_read_byte( int file );

extern I2cReturn_t i2c_smbus_write_byte( int file, UINT8 value );

extern I2cReturn_t i2c_smbus_read_byte_data( int file, UINT8 command );

extern I2cReturn_t i2c_smbus_write_byte_data( int file, UINT8 command,
                                              UINT8 value );

extern I2cReturn_t i2c_smbus_read_word_data( int file, UINT8 command );

extern I2cReturn_t i2c_smbus_write_word_data( int file, UINT8 command,
                                              UINT16 value );

extern I2cReturn_t i2c_smbus_process_call( int file, UINT8 command, UINT16 value );

/* Returns the number of read bytes */
extern I2cReturn_t i2c_smbus_read_block_data( int file, UINT8 command,
                                              UINT8 *values );

extern I2cReturn_t i2c_smbus_write_block_data( int file, UINT8 command,
                                               UINT8 length, UINT8 *values );

/* Returns the number of read bytes */
/* Until kernel 2.6.22, the length is hardcoded to 32 bytes. If you
   ask for less than 32 bytes, your code will only work with kernels
   2.6.23 and later. */
extern I2cReturn_t i2c_smbus_read_i2c_block_data( int file, UINT8 command,
                                                  UINT8 length, UINT8 *values );

extern I2cReturn_t i2c_smbus_write_i2c_block_data( int file, UINT8 command,
                                                   UINT8 length, UINT8 *values );

/* Returns the number of read bytes */
extern I2cReturn_t i2c_smbus_block_process_call( int file, UINT8 command,
                                                 UINT8 length, UINT8 *values );
#endif

#ifdef I2C_READ_RETRY
int I2c_GetReadRetries (void);
#endif
#ifdef I2C_WRITE_RETRY
int I2c_GetWriteRetries (void);
#endif


#endif  // __i2c_dev_linux_wrapper_h__

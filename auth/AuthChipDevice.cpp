////////////////////////////////////////////////////////////////////////////////
/// @file   AuthChipDevice.cpp
/// @brief  This is the definition of the AuthChip device, a concrete class
///     derived from the (partially) abstract base Device class.
/// @version    %Version:1 %
/// @author dv15727
/// @date   %Creation Date:%
//
//  Copyright 2012 Bose Corporation, Framingham, MA
///////////////////////////////////////////////////////////////////////////////////

#include <cstring>
#include <memory>
#include <fcntl.h>
#include <unistd.h>
#include "errno.h"
#include <string.h>      // for strerror()
#include "AuthChipDevice.h"
#include "I2cDriverTransportChannel.h"
#if GPIO
#include "AuthChipDevGpioDefines.h"
#endif
#define  __STDC_LIMIT_MACROS    // so UINT8_MAX etc. are defined
#include <stdint.h>
#include "VariantMgr.h"

//#define AUTHCHIP_DEBUG_LOG_LEVEL  DPrint::INSANE
//#define AUTHCHIP_DEBUG_LOG_LEVEL  DPrint::VERBOSE   // this or above turns on transport msgs also
//#define   AUTHCHIP_DEBUG_LOG_LEVEL  DPrint::DEBUG
#define   AUTHCHIP_DEBUG_LOG_LEVEL  DPrint::INFO


AuthChipDeviceIF::AuthChipDevicePtr AuthChipDevice::msp_instance;

////////////////////////////////////////////////////////////////////////
/// Default Constructor
/// @param bool debug - true turns on local mLogger.LogDebug's.
/// @return disallowed
////////////////////////////////////////////////////////////////////////
AuthChipDevice::AuthChipDevice( bool debug )
    : AuthChipDeviceIF( AUTHCHIP_LOGGER_NAME, debug )
    , mLogger( AUTHCHIP_LOGGER_NAME )
{
    mLogLevel = Debug() ? AUTHCHIP_DEBUG_LOG_LEVEL : DPrint::ERROR;
#if 0 /* Prefer to use dprint.conf rather than re-enabling this code.
         See http://hepdwiki.bose.com/bin/view/Projects/DebuggingWithDPrint */
    if( Debug() )
    {
        if( AUTHCHIP_DEBUG_LOG_LEVEL > DPrint::INFO )
            DPrint::SetGlobalLogLevel( mLogLevel );
        mLogger.SetLogLevel( AUTHCHIP_LOGGER_NAME, mLogLevel );
    }
#endif
    mLogger.LogVerbose( "AuthChipDevice constructor; pid=%d,tid=%ld\n", getpid(), gettid() );

#if 0
    // Set up all the GPIO pins we'll need
    if( ! ConfigGpioSignals() )
    {
        mLogger.LogCritical( "FAULT! Failed to cleanly set up GPIO signals\n" );
    }
#endif

    I2cDriverTransportChannel::InitData initData;

    // true turns on lower-level mLogger.LogDebug's
    mpTransport = new I2cDriverTransportChannel( AUTHCHIP_DEBUG_LOG_LEVEL > DPrint::DEBUG );

    // fill in the structure that'll initialize this transport channel uniquely
    initData.I2cAdapterNumber = SHELBY_AUTHCHIP_ADAPTER_NUMBER;
    // Ginger uses a different i2c address, see SHELBY-44912
    mi2cAddress = initData.BusAddress = ( VersionInfo::GetShelbyVariant() == VersionInfo::ShelbyVariant::GINGER ) ?
                                        kMFiAuthBusAddressLow : kMFiAuthBusAddressHigh;
    initData.UsesPec = SHELBY_AUTHCHIP_REQUIRES_PEC;
    initData.UsesSmbusProtocol = false;

    DriverTransportReturn_t r = mpTransport->Initialize( ( void* )&initData );
    mLogger.LogVerbose( "mpTransport->Initialize() = %d\n", ( int )r );

#if 1
    GetAllStatus();
    usleep( 50000 );
    if( CacheCertificate() != kNoErr )
        mLogger.LogCritical( "Failed to cache Certificate\n" );
#endif

    // Set up the CliClient we'll use to listen for CLI / Etap commands
#if 1
    // Base class news the CLIClient, calls SharedCLIServerCommands().
    AddUniqueCLIServerCommands();       // modifies mCliCmds, mUsage if need be

    mpCliClient->Initialize( Owner(), mCliCmds, std::bind( &AuthChipDevice::HandleSharedEtapCommands, this,
                                                           std::placeholders::_1,
                                                           std::placeholders::_2,
                                                           std::placeholders::_3 ) );
#else
    mpCliClient = new CLIClient( AUTHCHIP_LOGGER_NAME );

    mpCliClient->Initialize( Owner(), CLIServerCommands(), std::bind( &AuthChipDevice::HandleEtapCommand, this,
                                                                      std::placeholders::_1,
                                                                      std::placeholders::_2,
                                                                      std::placeholders::_3 ) );
#endif
}

////////////////////////////////////////////////////////////////////////
/// Default Destructor
/// @param none
/// @return disallowed
////////////////////////////////////////////////////////////////////////
AuthChipDevice::~AuthChipDevice()
{
#if 0
    if( mpCliClient )
    {
        delete mpCliClient;
        mpCliClient = NULL;
    }
#endif
    delete mpReset;
    msp_instance.reset();
}

////////////////////////////////////////////////////////////////////////
/// Singleton fetcher (& lazy-creator)
/// @param none
/// @return disallowed
////////////////////////////////////////////////////////////////////////
AuthChipDeviceIF::AuthChipDevicePtr AuthChipDevice::GetInstance()
{
    if( !msp_instance )
    {
        bool debug = ( AUTHCHIP_DEBUG_LOG_LEVEL > DPrint::INFO );
        msp_instance = std::make_shared<AuthChipDevice>( debug );
    }
    return msp_instance;
}

////////////////////////////////////////////////////////////////////////
/// ConfigGpioSignals
/// @brief  Configures the GPIO signals concerning the Auth Chip
/// @param  void
/// @return void
////////////////////////////////////////////////////////////////////////
bool AuthChipDevice::ConfigGpioSignals()
{
    // configure the RST line (GPIO2_6)
    mpReset = new GpioDevice( AUTHCHIP_DEVICE_RESET, Owner(), false, false, Debug() );
    mLastErrno = mpReset->LastError();
    if( LastError() != ENOERROR )
    {
        mLogger.LogCritical( "FAULT! Failed to create auth reset GPIO; error=%d\n",
                             LastError() );
        return false;
    }
#if 1
    int32_t rc;
    uint8_t v = 0;
    rc = mpReset->Write( &v, 1 );
    mLastErrno = mpReset->LastError();
    if( rc < 0 || LastError() != ENOERROR )
        mLogger.LogError( "Failed to write auth reset low; rc=%d, LastError()=%d\n",
                          rc, LastError() );
#else
    ResetAuthChip();    // on power-up, RST is already low, but we need it high to enable Warm Reset
#endif
    return true;
}

////////////////////////////////////////////////////////////////////////
/// ResetAuthChip
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
void AuthChipDevice::ResetAuthChip()
{
    // Out of power-up, the RST line must be brought high AFTER the first I2C transmission.
    // This is because the RST line at power-up determines the device's I2C bus address.
    // This also goes for the case of a "Warm Reset", performed by bringing RST low.
#if 0
    int32_t rc;
    uint8_t v = 0;
    rc = mpReset->Write( &v, 1 );
    mLastErrno = mpReset->LastError();
    if( rc < 0 || LastError() != ENOERROR )
        mLogger.LogError( "Failed to write auth reset low; rc=%ld, LastError()=%ld\n",
                          rc, LastError() );
    GetErrorCode();     // need to do something on the I2C bus
    v = 1;
    rc = mpReset->Write( &v, 1 );
    mLastErrno = mpReset->LastError();
    if( rc < 0 || LastError() != ENOERROR )
        mLogger.LogError( "Failed to write auth reset high; rc=%ld, LastError()=%ld\n",
                          rc, LastError() );
#elif 0
    system( "echo -n 0 > /sys/class/gpio/gpio38/value" );
    GetErrorCode();     // need to do something on the I2C bus
    system( "echo -n 1 > /sys/class/gpio/gpio38/value" );
#endif
}

////////////////////////////////////////////////////////////////////////
/// GetAllStatus
/// @abstract
/// @param
/// @return OSStatus
////////////////////////////////////////////////////////////////////////
OSStatus AuthChipDevice::GetAllStatus()
{
    DeviceReturn_t rc;
    uint8_t buf[32];

    // read the Certificate length
    rc = Read( kMFiAuthReg_DeviceVersion, buf, 9 );
    if( rc < 0 )
    {
        mLogger.LogError( "Failed to read all status; rc=%d, LastError()=%d\n",
                          rc, LastError() );
        return kUnknownErr;
    }
    // TODO Display all status
    return kNoErr;
}

////////////////////////////////////////////////////////////////////////
/// CacheCertificate
/// @abstract
/// @return OSStatus
////////////////////////////////////////////////////////////////////////
OSStatus AuthChipDevice::CacheCertificate( void )
{
    OSStatus stat = kNoErr;
    // Cache the certificate at startup since the certificate doesn't change
    // and this saves ~200 ms each time.
    uint8_t *pCert = NULL;
    size_t CertLen = 0;
    stat = CopyCertificate( &pCert, &CertLen );
    if( stat == kNoErr )
    {
        mLogger.LogDebug( "CacheCertificate: cached Certificate of %d bytes\n", CertLen );
        mCachedCertificate.assign( ( const char* )pCert, CertLen );
        free( pCert );
    }
    else
    {
        mLogger.LogError( "Failed to cache Certificate; err=%d\n", stat );
    }
    return( stat );
}

////////////////////////////////////////////////////////////////////////
/// Initialize
/// @abstract
/// @return OSStatus
////////////////////////////////////////////////////////////////////////
OSStatus AuthChipDevice::Initialize( void )
{
    OSStatus stat = kNoErr;
    if( mCachedCertificate.empty() )
        stat = CacheCertificate();
    return stat;
}

////////////////////////////////////////////////////////////////////////
/// Finalize
/// @abstract
/// @return OSStatus
////////////////////////////////////////////////////////////////////////
OSStatus AuthChipDevice::Finalize( void )
{
    mCachedCertificate.clear();
    return kNoErr;
}

////////////////////////////////////////////////////////////////////////
/// CopyCertificate
/// @abstract   Copy the certificate from the MFi authentication coprocessor.
/// @param outCertificatePtr   Receives malloc()'d ptr to a DER-encoded PKCS#7 message
/// containing the certificate. Caller must free() on success.
/// @param outCertificateLen   Number of bytes in the DER-encoded certificate.
/// @return OSStatus
////////////////////////////////////////////////////////////////////////
OSStatus AuthChipDevice::CopyCertificate( uint8_t **outCertificatePtr,
                                          size_t *outCertificateLen )
{
    DeviceReturn_t rc;
    uint8_t buf[32];
    size_t certificateLen;
    uint8_t *certificatePtr;

    if( GetCertificateLength() != 0 )
    {
        // return the cached Certificate
        certificatePtr = ( uint8_t * ) malloc( GetCertificateLength() );
        if( !certificatePtr )
        {
            mLogger.LogError( "Out of memory\n" );
            return kNoMemoryErr;
        }
        memcpy( certificatePtr, mCachedCertificate.data(), GetCertificateLength() );
        *outCertificatePtr = certificatePtr;
        *outCertificateLen = GetCertificateLength();
        mLogger.LogDebug( "Returning cached Certificate of %d bytes\n", GetCertificateLength() );
        return kNoErr;
    }
    // read the Certificate length
    rc = Read( kMFiAuthReg_AccessoryDeviceCertificateSize, buf, 2 );
    if( rc < 0 )
    {
        mLogger.LogError( "Failed to read Certificate Length; rc=%d, LastError()=%d\n",
                          rc, LastError() );
        return kUnknownErr;
    }
    certificateLen = ( buf[0] << 8 ) | buf[1];
    if( certificateLen == 0 )
    {
        mLogger.LogError( "Certificate Length is zero\n" );
        return kSizeErr;
    }
    // read the Certificate data
    certificatePtr = ( uint8_t * ) malloc( certificateLen );
    if( !certificatePtr )
    {
        mLogger.LogError( "Out of memory\n" );
        return kNoMemoryErr;
    }
    // Note: reads from the data1 register auto-increment to data2, data3, etc. registers that follow it.
    rc = Read( kMFiAuthReg_AccessoryDeviceCertificateData1, certificatePtr, certificateLen );
    if( ( rc < 0 ) || ( ( size_t )rc != certificateLen ) )
    {
        mLogger.LogError( "Failed to read %d bytes of Certificate data; rc=%d, LastError()=%d\n",
                          certificateLen, rc, LastError() );
        free( certificatePtr );
        return kUnknownErr;
    }
    mLogger.LogDebug( "copy Certificate read %zu bytes\n", certificateLen );
    *outCertificatePtr = certificatePtr;
    *outCertificateLen = certificateLen;

    return kNoErr;
}

////////////////////////////////////////////////////////////////////////
/// CreateSignature
/// @abstract   Create an RSA signature from the specified SHA-1 digest using the MFi
/// authentication coprocessor.
/// @param      inDigestPtr         Ptr to 20-byte SHA-1 digest (aka "Challenge").
/// @param      inDigestLen         Number of bytes in the digest. Must be 20.
/// @param      outSignaturePtr     Receives malloc()'d ptr to RSA signature. Caller
/// must free() on success.
/// @param      outSignatureLen     Receives number of bytes in RSA signature.
/// @return     kSizeErr            inDigestLen != 20
////////////////////////////////////////////////////////////////////////
OSStatus AuthChipDevice::CreateSignature( const void *inDigestPtr,
                                          size_t inDigestLen,
                                          uint8_t **outSignaturePtr,
                                          size_t *outSignatureLen )
{
    DeviceReturn_t rc;
    uint8_t buf[32];
    size_t signatureLen;
    uint8_t* signaturePtr;

    mLogger.LogDebug( "CreateSignature: inLen=%d\n", inDigestLen );
    if( inDigestLen != kMFiAuthReg_RequiredChallengeLength )
    {
        mLogger.LogCritical( "CreateSignature: incoming Challenge length is wrong!\n" );
        return kSizeErr;
    }

    // Write the data to sign.
    // Note: writes to the size register auto-increment to the data register that follows it.
    buf[0] = ( uint8_t )( ( inDigestLen >> 8 ) & 0xFF );
    buf[1] = ( uint8_t )( inDigestLen        & 0xFF );
    memcpy( &buf[2], inDigestPtr, inDigestLen );
    rc = Write( kMFiAuthReg_ChallengeSize, buf, 2 + inDigestLen );
    if( rc < 0 )
    {
        mLogger.LogError( "Failed to write Challenge; rc=%d, LastError()=%d\n",
                          rc, LastError() );
        return kUnknownErr;
    }

    // Set Signature Length to maximum allowable
    buf[0] = 0;
    buf[1] = 128;
    rc = Write( kMFiAuthReg_SignatureSize, buf, 2 );
    if( rc < 0 )
    {
        mLogger.LogError( "Failed to set Signature size to maximum; rc=%d, LastError()=%d\n",
                          rc, LastError() );
        return kUnknownErr;
    }

    // Generate the signature.
    buf[0] = kMFiAuthControl_GenerateSignature;
    rc = Write( kMFiAuthReg_Control_Status, buf, 1 );
    if( rc < 0 )
    {
        mLogger.LogError( "Failed to start signature generation; rc=%d, LastError()=%d\n",
                          rc, LastError() );
        return kUnknownErr;
    }
    // read status of signature generation operation
    rc = Read( kMFiAuthReg_Control_Status, buf, 1 );
    if( rc < 0 )
    {
        mLogger.LogError( "Failed to read signature generation status; rc=%d, LastError()=%d\n",
                          rc, LastError() );
        return kUnknownErr;
    }
    if( buf[0] & kMFiAuthStatus_ErrorSet_Mask )
    {
        // an error occurred; which is in the Error register
        rc = Read( kMFiAuthReg_ErrorCode, buf, 1 );
        if( rc < 0 )
        {
            mLogger.LogError( "Failed to read Error register; rc=%d, LastError()=%d\n",
                              rc, LastError() );
            return kUnknownErr;
        }
        mLogger.LogError( "Error on Signature Generation=%d\n", ( int ) buf[0] );
        return kUnknownErr;
    }

    // Read the signature.
    rc = Read( kMFiAuthReg_SignatureSize, buf, 2 );
    if( rc < 0 )
    {
        mLogger.LogError( "Failed to read signature size; rc=%d, LastError()=%d\n",
                          rc, LastError() );
        return kUnknownErr;
    }
    signatureLen = ( buf[0] << 8 ) | buf[1];
    if( signatureLen == 0 )
    {
        mLogger.LogError( "FAULT! Generated Signature size is zero\n" );
        return kSizeErr;
    }
    signaturePtr = ( uint8_t * ) malloc( signatureLen );
    if( !signaturePtr )
    {
        mLogger.LogError( "Out of memory\n" );
        return kNoMemoryErr;
    }
    rc = Read( kMFiAuthReg_SignatureData, signaturePtr, signatureLen );
    if( ( rc < 0 ) || ( ( size_t )rc != signatureLen ) )
    {
        mLogger.LogError( "Failed to read %d bytes of signature data; rc=%d, LastError()=%d\n",
                          signatureLen, rc, LastError() );
        free( signaturePtr );
        return kUnknownErr;
    }
    mLogger.LogDebug( "created signature %d bytes long\n", ( int ) signatureLen );
    *outSignaturePtr = signaturePtr;
    *outSignatureLen = signatureLen;

    return kNoErr;
}

////////////////////////////////////////////////////////////////////////
/// GetDeviceVersion
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
uint8_t AuthChipDevice::GetDeviceVersion()
{
    uint8_t v = UINT8_MAX;
    DeviceReturn_t rc;

    rc = Read( kMFiAuthReg_DeviceVersion, &v, sizeof( v ) );
    if( rc < 0 || LastError() != ENOERROR )
        mLogger.LogError( "Failed to read device version; rc=%d, LastError()=%d\n",
                          rc, LastError() );
    else if( Debug() )
        mLogger.LogVerbose( "device version=%d [rc=%d]\n", ( int )v, ( int )rc );
    return v;
}

////////////////////////////////////////////////////////////////////////
/// GetFirmwareVersion
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
uint8_t AuthChipDevice::GetFirmwareVersion()
{
    uint8_t v = UINT8_MAX;
    DeviceReturn_t rc;

    rc = Read( kMFiAuthReg_FirmwareVersion, &v, sizeof( v ) );
    if( rc < 0 || LastError() != ENOERROR )
        mLogger.LogError( "Failed to read firmware version; rc=%d, LastError()=%d\n",
                          rc, LastError() );
    else if( Debug() )
        mLogger.LogVerbose( "firmware version=%d [rc=%d]\n", ( int )v, ( int )rc );
    return v;
}

////////////////////////////////////////////////////////////////////////
/// GetAuthProtocolMajorVersion
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
uint8_t AuthChipDevice::GetAuthProtocolMajorVersion()
{
    uint8_t v = UINT8_MAX;
    DeviceReturn_t rc;

    rc = Read( kMFiAuthReg_ProtocolMajorVersion, &v, sizeof( v ) );
    if( rc < 0 || LastError() != ENOERROR )
        mLogger.LogError( "Failed to read protocol major version; rc=%d, LastError()=%d\n",
                          rc, LastError() );
    else if( Debug() )
        mLogger.LogVerbose( "protocol major version=%d [rc=%d]\n", ( int )v, ( int )rc );
    return v;
}

////////////////////////////////////////////////////////////////////////
/// GetAuthProtocolMinorVersion
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
uint8_t AuthChipDevice::GetAuthProtocolMinorVersion()
{
    uint8_t v = UINT8_MAX;
    DeviceReturn_t rc;

    rc = Read( kMFiAuthReg_ProtocolMinorVersion, &v, sizeof( v ) );
    if( rc < 0 || LastError() != ENOERROR )
        mLogger.LogError( "Failed to read protocol minor version; rc=%d, LastError()=%d\n",
                          rc, LastError() );
    else if( Debug() )
        mLogger.LogVerbose( "protocol minor version=%d [rc=%d]\n", ( int )v, ( int )rc );
    return v;
}

////////////////////////////////////////////////////////////////////////
/// GetDeviceID
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
uint32_t AuthChipDevice::GetDeviceID()
{
    uint32_t v = UINT32_MAX;
    DeviceReturn_t rc;

    rc = Read( kMFiAuthReg_DeviceID, &v, sizeof( v ) );
    if( rc < 0 || LastError() != ENOERROR )
        mLogger.LogError( "Failed to read device ID; rc=%d, LastError()=%d\n",
                          rc, LastError() );
    else if( Debug() )
        mLogger.LogVerbose( "protocol device ID=%d [rc=%d]\n", v, rc );
    return v;
}

////////////////////////////////////////////////////////////////////////
/// GetErrorCode
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
uint8_t AuthChipDevice::GetErrorCode()
{
    uint8_t v = UINT8_MAX;
    DeviceReturn_t rc;

    rc = Read( kMFiAuthReg_ErrorCode, &v, sizeof( v ) );
    if( rc < 0 || LastError() != ENOERROR )
        mLogger.LogError( "Failed to read error code; rc=%d, LastError()=%d\n",
                          rc, LastError() );
    else if( Debug() )
        mLogger.LogVerbose( "protocol error code;=%d [rc=%d]\n", ( int )v, ( int )rc );
    return v;
}

////////////////////////////////////////////////////////////////////////
/// GetSystemEventCounter
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
uint8_t AuthChipDevice::GetSystemEventCounter()
{
    uint8_t v = UINT8_MAX;
    DeviceReturn_t rc;

    rc = Read( kMFiAuthReg_SystemEventCounter, &v, sizeof( v ) );
    if( rc < 0 || LastError() != ENOERROR )
        mLogger.LogError( "Failed to read system event counter; rc=%d, LastError()=%d\n",
                          rc, LastError() );
    else if( Debug() )
        mLogger.LogVerbose( "protocol system event counter=%d [rc=%d]\n", ( int )v, ( int )rc );
    return v;
}

////////////////////////////////////////////////////////////////////////
/// GetAuthenticationStatus
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
uint8_t AuthChipDevice::GetAuthenticationStatus()
{
    uint8_t v = UINT8_MAX;
    DeviceReturn_t rc;

    rc = Read( kMFiAuthReg_Control_Status, &v, sizeof( v ) );
    if( rc < 0 || LastError() != ENOERROR )
        mLogger.LogError( "Failed to read auth status; rc=%d, LastError()=%d\n",
                          rc, LastError() );
    else if( Debug() )
        mLogger.LogVerbose( "auth status=0x%02X [rc=%d]\n", ( int )v, ( int )rc );
    return v;
}

////////////////////////////////////////////////////////////////////////
/// GetCertificateLength
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
uint16_t AuthChipDevice::GetCertificateLength() const
{
    return mCachedCertificate.length();
}

////////////////////////////////////////////////////////////////////////
/// GetI2CAddress
/// @param
/// @return the i2c register address used to access the device
////////////////////////////////////////////////////////////////////////
uint8_t AuthChipDevice::GetI2CAddress() const
{
    return mi2cAddress;
}

////////////////////////////////////////////////////////////////////////
/// AddUniqueCLIServerCommands
/// @param
/// @return
////////////////////////////////////////////////////////////////////////
void AuthChipDevice::AddUniqueCLIServerCommands()
{
    typedef CLIClient::CmdPtr           CmdPtr;
    typedef CLIClient::CLICmdDescriptor CLICmdDescriptor;

    mUsage.append( "\n\treal  : this is the real chip" );
    mCliCmds.push_back( ( CmdPtr )new CLICmdDescriptor( "auth real",
                                                        "this is the real chip",
                                                        "" ) );
    mUsage.append( "\n\treset : perform a warm reset" );
    mCliCmds.push_back( ( CmdPtr )new CLICmdDescriptor( "auth reset",
                                                        "perform warm reset",
                                                        "" ) );
}

////////////////////////////////////////////////////////////////////////////////
/// @name HandleUniqueEtapCommands
/// @brief Called back by the base handler when no handler was found in common.
/// @param[in]  command
/// @param[in]  argList - list of strings the TAP user typed
/// @param[out] Response_ - what we want to appear in the TAP window
////////////////////////////////////////////////////////////////////////////////
bool  AuthChipDevice::HandleUniqueEtapCommands( const std::string & command,
                                                const CLIClient::StringListType& argList,
                                                const string& Response_ )
{
    // BOILERPLATE: Needed to get around the Callback<> template making things into
    // const references, no matter what I tried!
    string& response = const_cast<string&>( Response_ );
    bool handled = false;
    response.clear();
    // end BOILERPLATE

    if( command.compare( "auth reset" ) == 0 )
    {
        // perform a warm reset
        ResetAuthChip();
        handled = true;
    }

    else if( command.compare( "auth real" ) == 0 )
    {
        handled = true;
    }

    else
    {
        response.append( "\tAuth Chip commands:\n" );
        response.append( mUsage );
        return true;
    }

    return handled;
}

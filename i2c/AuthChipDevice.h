////////////////////////////////////////////////////////////////////////////////
/// @file   AuthChipDevice.h
/// @brief  Concrete class for the Apple Authorization chip.
///     derived class, based on the Device interface, that implements
///     a particular type of physical device; i.e. a battery. It instantiates the specific
///     type of communication ("transport") channel to talk to its connected (IC) device.
/// @version    %Version:1 %
/// @author dv15727
/// @date   %Created:%
//
//  Copyright 2014 Bose Corporation, Framingham, MA
///////////////////////////////////////////////////////////////////////////////////

#ifndef __AuthChipDevice_h__
#define __AuthChipDevice_h__

#include <memory>
//#include <queue>
//#include <functional>
//#include <list>
#include <vector>
#include <string>
#include "GpioDevice.h"
#include "DPrint.h"
//#include "CliClient.h"
#include "ThreadMutex.h"

#include "AuthChipDeviceIF.h"
#include "AuthChipDefines.h"


//using std::list;
using std::vector;
using std::string;

//class NotifyTargetTaskIF;

#define AUTHCHIP_LOGGER_NAME   "AuthChipDevice"


#define AUTHCHIP_DEVICE_RESET "/dev/gpiodev/auth_resetn"

// The Adapter number is 1-based, where the bus number is 0-based
#define SHELBY_AUTHCHIP_ADAPTER_NUMBER       1      // as in "/dev/i2c-2"
#define SHELBY_AUTHCHIP_REQUIRES_PEC         false  // error correction on I2C packets req'd?


///////////////////////////////////////////////////////////////////////////////
/// AuthChipDevice - derived class, based on the Device interface, that implements
/// a particular type of physical device; i.e. a battery. It instantiates the specific
/// type of communication ("transport") channel to talk to its connected (IC) device.
///////////////////////////////////////////////////////////////////////////////

class AuthChipDevice : public AuthChipDeviceIF,
    public std::enable_shared_from_this<AuthChipDevice>
{
public:
    static AuthChipDeviceIF::AuthChipDevicePtr GetInstance();
    virtual ~AuthChipDevice();
    AuthChipDevice( bool debug = false );

private:
    // BEGIN concrete implementations
    // High-level functionality
    OSStatus Initialize() override;
    OSStatus Finalize() override;
    OSStatus CopyCertificate( uint8_t **outCertificatePtr,
                              size_t *outCertificateLen ) override;
    OSStatus CreateSignature( const void *inDigestPtr,
                              size_t inDigestLen,
                              uint8_t **outSignaturePtr,
                              size_t *outSignatureLen ) override;
    // Accessors
    uint8_t GetDeviceVersion() override;
    uint8_t GetFirmwareVersion() override;
    uint8_t GetAuthProtocolMajorVersion() override;
    uint8_t GetAuthProtocolMinorVersion() override;
    uint32_t GetDeviceID() override;
    uint8_t GetErrorCode() override;
    uint8_t GetSystemEventCounter() override;
    uint16_t GetCertificateLength() const override;
    uint8_t GetI2CAddress() const override;
    // END concrete implementations

    uint8_t GetAuthenticationStatus();
    OSStatus GetAllStatus();
    // Low-level functionality
    void ResetAuthChip();
    OSStatus CacheCertificate();
    // Initializers
    bool ConfigGpioSignals();
    void AddUniqueCLIServerCommands();

    static AuthChipDeviceIF::AuthChipDevicePtr msp_instance;
    CThreadMutex mMutex;
    DPrint mLogger;
    DPrint::Level mLogLevel;
    GpioDevice* mpReset;
    std::string mCachedCertificate;
    uint8_t mi2cAddress;

    //##### Boilerplate Stuff needed to host CLI / TAP commands: #############
#if 1
    bool  HandleUniqueEtapCommands( const std::string & command,
                                    const CLIClient::StringListType& argList,
                                    const string& response ) override;
#else
    std::string mUsage;
    CLIClient * mpCliClient;
    std::vector<CLIClient::CmdPtr> mCliCmds;
    bool  HandleEtapCommand( const std::string & command,
                             const CLIClient::StringListType& argList,
                             const string& response );
    const std::vector<CLIClient::CmdPtr> & CLIServerCommands();
#endif
    //##### END Boilerplate Stuff #############

    //Disable Copies
    AuthChipDevice( const AuthChipDevice & source ) = delete;
    AuthChipDevice operator=( const AuthChipDevice & source ) = delete;
};

#endif // __AuthChipDevice_h__

#ifndef __AUTH_H__
#define __AUTH_H__

//!<<<< definitions
#define MODULE_NAME          "auth.c"
#define DIAG_NAME            "dt_auth"

//!<<<< from :
//!<<<<   TI-HSP/DeviceDrivers/AuthChip/AuthChipDevice.h:
//!<<<<   TI-HSP/DeviceDrivers/AuthChip/AuthChipDefines.h:
//!<<<<   ADAPTER_NUMBER:  was "1", changed to "4" because I see /dev/i2c=4. as in "/dev/i2c-2"
#define SHELBY_AUTHCHIP_ADAPTER_NUMBER       4      // X as in "/dev/i2c-X"
#define SHELBY_AUTHCHIP_REQUIRES_PEC         false  // error correction on I2C packets req'd?
#define kMFiAuthBusAddressLow                0x10   // this is the 7-bit address when RST is held low while the chip VCC is applied
#define kMFiAuthBusAddressHigh               0x11   // this is the 7-bit address when RST is high while the chip VCC is applied
#define CERT_STR_MAX                         (2*KB)
#define AUTH_ITERATIONS_DEFAULT              "10"
#define AUTH_ADAPTER_DEFAULT                 "4"
#define AUTH_BUS_DEFAULT                     "16"
#define AUTH_TEST_DEFAULT                    "1"

//!<<<< auth chip register definitions (from authDevice.h)
#define kMFiAuthReg_DeviceVersion                   0x00    // 1 byte, ro; 0x05
#define kMFiAuthReg_DeviceVersion_Value             0x05
#define kMFiAuthReg_FirmwareVersion                 0x01    // 1 byte, ro; 0x01
#define kMFiAuthReg_FirmwareVersion_Value           0x01
#define kMFiAuthReg_ProtocolMajorVersion            0x02    // 1 byte, ro; 0x02
#define kMFiAuthReg_ProtocolMajorVersion_value      0x02
#define kMFiAuthReg_ProtocolMinorVersion            0x03    // 1 byte, ro; 0x00
#define kMFiAuthReg_ProtocolMinorVersion_Value      0x00
#define kMFiAuthReg_DeviceID                        0x04    // 4 bytes, ro; 0x00.00.02.00
#define kMFiAuthReg_DeviceID_Value                  0x00000200
#define kMFiAuthReg_ErrorCode                       0x05    // 1 byte, ro; !!CLEARED ON READ!!
#define kMFiAuthErrorCode_NoError                           0x00
#define kMFiAuthErrorCode_InvalidReadRegister               0x01
#define kMFiAuthErrorCode_InvalidWriteRegister              0x02
#define kMFiAuthErrorCode_InvalidSigLen                     0x03
#define kMFiAuthErrorCode_InvalidChallengeLen               0x04
#define kMFiAuthErrorCode_InvalidCertLen                    0x05
#define kMFiAuthErrorCode_SigGenProcErr                     0x06
#define kMFiAuthErrorCode_ChallengeGenProcErr               0x07
#define kMFiAuthErrorCode_SigVerifProcErr                   0x08
#define kMFiAuthErrorCode_CertValidationProcErr             0x09
#define kMFiAuthErrorCode_InvalidProcessControl             0x0A
#define kMFiAuthErrorCode_ProcCtrlOutOfSequence             0x0B
// Holds current value of CP's "event counter", which auto-decrements once per second.
// Power should not be removed from the CP until this counter has decremented to zero.
#define kMFiAuthReg_SystemEventCounter              0x4D    // 1, ro; undefined
// END of a Sequentially-Readable block of Registers
// BEGINNING of a Sequentially-Readable block of Registers  // length, Access; contents after reset
#define kMFiAuthReg_Control_Status                  0x10    // 1 byte, ro; 0x00
#define kMFiAuthReg_AccessoryDeviceCertificateSize      0x30    // 2 bytes, ro; <= 1280
#define kMFiAuthReg_AccessoryDeviceCertificateData1     0x31    // 128 bytes, ro; Certificate
#define kMFiAuthReg_AccessoryDeviceCertificateData2     0x32    // 128 bytes, ro; Certificate
#define kMFiAuthReg_AccessoryDeviceCertificateData3     0x33    // 128 bytes, ro; Certificate
#define kMFiAuthReg_AccessoryDeviceCertificateData4     0x34    // 128 bytes, ro; Certificate
#define kMFiAuthReg_AccessoryDeviceCertificateData5     0x35    // 128 bytes, ro; Certificate
#define kMFiAuthReg_AccessoryDeviceCertificateData6     0x36    // 128 bytes, ro; Certificate
#define kMFiAuthReg_AccessoryDeviceCertificateData7     0x37    // 128 bytes, ro; Certificate
#define kMFiAuthReg_AccessoryDeviceCertificateData8     0x38    // 128 bytes, ro; Certificate
#define kMFiAuthReg_AccessoryDeviceCertificateData9     0x39    // 128 bytes, ro; Certificate
#define kMFiAuthReg_AccessoryDeviceCertificateData10    0x3A    // 128 bytes, ro; Certificate
// END of a Sequentially-Readable block of Registers

#define kMFiAuthReg_SelfTest_Control_Status             0x40    // 1, rw; 0x00
// ON WRITE TO REGISTER: (controls the built-in self-test functions of CP processes)
#define kMFiAuthSelfTestControl_ProcessControl_Mask         0x07    // bits 2-0
#define kMFiAuthSelfTestControl_ProcessControl_RightShift   0
#define kMFiAuthSelfTestControl_NoOperation                 0
// Runs X.509 certificate and private key tests. NOTE THAT these tests ONLY verify that
// the elements are present in Flash memory;no authentication is performed.
#define kMFiAuthSelfTestControl_RunTests                    1
// ON READ OF REGISTER: (reports status the built-in self-test functions of CP processes)
// Reading this Register clears it to 0x00
#define kMFiAuthSelfTestStatus_CertificateNotFound          0x80
#define kMFiAuthSelfTestStatus_PrivateKeyNotFound           0x40

//!<<<< auth test error codes
typedef enum {
  NOK_DEVVER=2,
  NOK_FIRMVER,
  NOK_MAJVER,
  NOK_MINVER,
  NOK_DEVID,
  NOK_CERTLEN,
  NOK_CERTDATA,
  NOK_SELFTSTW,
  NOK_SELFTSTR,
  NOK_SELFTST,
} AUTH_ERR;

//!<<<< thread parameters
typedef struct {
  unsigned int secs;
  char cmd [GP_STR_LEN];
} AUTH_THREAD_ARGS, *pAUTH_THREAD_ARGS;
typedef struct STATS {
  unsigned char version;
  unsigned char firmware_version;
  unsigned char major_version;
  unsigned char minor_version;
  unsigned int  device_id;
  unsigned char error_code;
  unsigned char system_event_counter;
  unsigned char auth_status;
  unsigned char self_test_status;
  unsigned short cert_length;
  unsigned char cert_data[CERT_STR_MAX];
  unsigned int intervalStatus;
} STATS, *pSTATS;
//!<<<< prototypes
int main (int argc, char *argv[]);
void auth_usage (void);
void auth_usage_short (void);
void *auth_thread (void *args);
int auth_GetDeviceVersion(unsigned char *version);
int auth_GetFirmwareVersion(unsigned char *firmware_version);
int auth_GetProtocolMajorVersion(unsigned char *major_version);
int auth_GetProtocolMinorVersion(unsigned char *minor_version);
int auth_GetDeviceId(unsigned int *device_id);
int auth_GetErrorCode(unsigned char *error_code);
int auth_GetSystemEventCounter(unsigned char *system_event_counter);
int auth_GetAuthStatus(unsigned char *auth_status);
int auth_GetCertLength(unsigned short *cert_length);
int auth_GetCertData(unsigned char *cert_data, unsigned short cert_len);
int auth_GetSelfTestStatus(unsigned char *self_test_status);
int auth_SetSelfTestStatus(unsigned char *self_test_status);

#endif

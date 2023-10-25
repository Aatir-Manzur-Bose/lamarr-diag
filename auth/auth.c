#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h> 
#include <pthread.h>
#include <getopt.h>
#include <ncurses.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <byteswap.h>
#include "diag_common.h"
#include "I2cDevLinuxWrapper.h"
#include "auth.h"
#include "jWrite.h"

#ifdef MOF_INT
#pragma message("==> MOF_INT defined")
#endif
#ifdef AUTH_CLI_OFF
#pragma message("==> AUTH_CLI_OFF defined")
#endif

//!<<<< globals
extern FLAG Verbosity;
extern LOGFILE LogFile;
extern LOGFILE JsonFile;
#define verbosity Verbosity.Get()
#define pLogFile LogFile.Get()
#define pJsonFile JsonFile.Get()
extern STR_TAB_ENT diag_mofRespStrTab[];
MOF_BUFFER mofBuffer;
FLAG mofMode;
int auth_thread_stop=0;
pAUTH_THREAD_ARGS t_auth_args;
pthread_t tid_auth;
STATS stats;
I2cHandle_t mHandle=-1; 
int bus_address=-1;
int adapter=-1;
char adapter_str [GP_STR_LEN];
int show_info=0;
int loop_mode=0;
int iterations=-1;
int test_flag=0;

//!<<<< error strings
STR_TAB_ENT authErrorStrTab[] = {
  {PASS,DIAG_PASS},
  {FAIL,DIAG_FAIL},
  {NOK_DEVVER,DIAG_NAME    ":error reading device version register"},
  {NOK_FIRMVER,DIAG_NAME   ":error reading firmware version register"},
  {NOK_MAJVER,DIAG_NAME    ":error reading ProtocolMajorVersion register"},
  {NOK_MINVER,DIAG_NAME    ":error reading ProtocolMinorVersion register"},
  {NOK_DEVID,DIAG_NAME     ":error reading DeviceId register"},
  {NOK_CERTLEN,DIAG_NAME   ":error reading CertLength register"},
  {NOK_CERTDATA,DIAG_NAME  ":error reading CertData register"},
  {NOK_SELFTSTW,DIAG_NAME  ":error writing SelfTestStatus register"},
  {NOK_SELFTSTR,DIAG_NAME  ":error reading SelfTestStatus register"},
  {NOK_SELFTST,DIAG_NAME   ":self test failed"},
};

//!<<<< AUTH valid platform table
DIAG_VALID_PLATFORMS_ENT valid_platforms_tab[] = {
  DIAG_PLATFORM_PROFESSOR,
  DIAG_PLATFORM_EDDIE,
  DIAG_PLATFORM_GINGERCHEEVERS,
  DIAG_PLATFORM_FLIPPER,
  DIAG_PLATFORM_TAYLOR,
  //!<<<< temp debug
  DIAG_PLATFORM_QUALCOMM,
  NULL,
};

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< usage()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void auth_usage_short (void) {
  diag_usage_short (DIAG_NAME, "Apple Authorization Test");   
}
void auth_usage (void) {
  fprintf (stdout, "usage:  %s <-VhHvil:L:Ra:b:>\n", DIAG_NAME); 
  fprintf (stdout, " -h,--help             Print this message\n"); 
  fprintf (stdout, " -H,--help-short       Print test summary message\n"); 
  fprintf (stdout, " -V,--version          show build version\n"); 
  fprintf (stdout, " -v[vv],--verbose      Verbose level\n"); 
  fprintf (stdout, " -L,--logfile          Output to logfile in /opt/Bose/tmp/, default %s.log\n", DIAG_NAME); 
  fprintf (stdout, " -R,--resetlogfile     Reset logfile\n"); 
  fprintf (stdout, " -M,--mof              Use Manufacturing Output Format (MOF)\n"); 
  fprintf (stdout, " -a,--adapter          Bus number, default %x (i.e. /dev/i2c-%x)\n",SHELBY_AUTHCHIP_ADAPTER_NUMBER, SHELBY_AUTHCHIP_ADAPTER_NUMBER); 
  fprintf (stdout, " -b,--bus-address      Bus address (device) (decimal!), default %s\n", AUTH_BUS_DEFAULT); 
  fprintf (stdout, " -i,--info             Show info only\n"); 
  fprintf (stdout, " -l,--loop             Loop reading registers N times, default %s, 0 is infinite\n", AUTH_ITERATIONS_DEFAULT); 
  fprintf (stdout, "Example: %s -a4 -b%s -l10 (professor)\n", DIAG_NAME,  AUTH_BUS_DEFAULT); 
  fprintf (stdout, "Example: %s -l10\n", DIAG_NAME); 
  fprintf (stdout, "Example: %s -i\n", DIAG_NAME); 
  NewLine ();
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< auth thread
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void *auth_thread (void *args) {
  int rc=0;
  int l=0;
  int exit_status=0;
  pAUTH_THREAD_ARGS p_args = (pAUTH_THREAD_ARGS)args;
  char *p_buffer=0;
  unsigned int buflen=GP_STR_LEN*4;
  struct jWriteControl jwc;
  char gp_str [GP_STR_LEN];
  char gp_str3 [GP_STR_LEN];
  int rc_00,rc_01,rc_02,rc_03,rc_04,rc_30,rc_31,rc_40w,rc_40r;
  int first_time=!0;
  int interval=0;
  const char *p=NULL;

  diag_verbose (VVERBOSE, verbosity, "starting auth thread secs=%d\n", p_args->secs);

  //!<<<< ................................................................
  //!<<<< info mode
  //!<<<< ................................................................
  if (show_info) {
    //!<<<< INFO MODE >>>>
    //!<<<< Just Show Info and Exit >>>>
    
    //!<<<< init
    diag_verbose(VVVERBOSE, verbosity, "%s\n", "doing i2c channel init");
    i2c_transport_channel_initialize_system();
    sleep (1);
    diag_verbose(VVVERBOSE, verbosity, "%s\n", "done init");
    
    //!<<<< open
    mHandle = i2c_transport_channel_open( adapter,                        //!<<<< i.e: 4 for /dev/i2c-4 
					  bus_address,                    //!<<<< or kMFiAuthBusAddressLow
					  SHELBY_AUTHCHIP_REQUIRES_PEC,   //!<<<< pData->UsesPec ? TRUE : FALSE, >>>>
					  FALSE,                          //!<<<< pData->UsesSmbusProtocol ? TRUE : FALSE, >>>>
					  (VVVERBOSE==verbosity) ? !0 : 0 );                    //!<<<< pass our debug flag to lower levels
    if (0 > mHandle) {
      diag_verbose (ALWAYS, verbosity, "error opening i2c channel, handle=%x adapter=%x bus address=%x\n", mHandle, adapter, bus_address);
      exit_status = EXIT_FAILURE;
      goto test_exit;
    }
    sleep (1);
    diag_verbose(VVVERBOSE, verbosity, "%s mHandle=%d using address %d\n", "done open", mHandle,  kMFiAuthBusAddressLow);
    
    
    //!<<<< ................................................................
    //!<<<< do register reads here...
    //!<<<< ................................................................
    auth_GetDeviceVersion(&stats.version);
    auth_GetDeviceVersion(&stats.version);
    auth_GetFirmwareVersion(&stats.firmware_version);
    auth_GetProtocolMajorVersion(&stats.major_version);
    auth_GetProtocolMinorVersion(&stats.minor_version);
    auth_GetDeviceId(&stats.device_id);
    auth_GetErrorCode(&stats.error_code);
    auth_GetSystemEventCounter(&stats.system_event_counter);
    auth_GetAuthStatus(&stats.auth_status);
    auth_GetSelfTestStatus(&stats.self_test_status);
    auth_GetCertLength(&stats.cert_length);
    rc=auth_GetCertData(stats.cert_data, stats.cert_length);
    if (!rc) {
      diag_verbose (ALWAYS, verbosity, "error reading certificate, rc=%x cert_length=%x\n", rc, stats.cert_length);
      exit_status = EXIT_FAILURE;
      goto test_exit;
    }

    //!<<<< output
    diag_printf ("%12s ",	    "");
    diag_printf ("%8s ",	    "");
    diag_printf ("%35s ",	    "Versions");
    diag_printf ("%8s ",	    "");
    diag_printf ("%8s ",	    "");
    diag_printf ("%8s ",	    "");
    diag_printf ("%24s\n",    "");
    
    diag_printf ("%12s ",	"Bus");
    diag_printf ("%8s ",	"Device");
    diag_printf ("%8s ",	"Chip");
    diag_printf ("%8s ",	"FW");
    diag_printf ("%8s ",	"Maj");
    diag_printf ("%8s ",	"Min");
    diag_printf ("%8s ",	"DevID");
    diag_printf ("%8s ",	"SelfTest");
    diag_printf ("%8s ",     "Len");
    diag_printf ("%24s\n",    "Cert []");
    
    diag_printf ("%12s ",	"------------");
    diag_printf ("%8s ",	"--------");
    diag_printf ("%35s ",	"-------- -------- -------- --------");
    diag_printf ("%8s ",	"--------");
    diag_printf ("%8s ",	"--------");
    diag_printf ("%8s ",	"--------");
    diag_printf ("%24s\n","------------------------");
    
    //!<<<< log entry
    diag_printf ("%12s ",	 adapter_str);
    diag_printf ("%8d ",	 bus_address);
    diag_printf ("%8x ",	 stats.version);
    diag_printf ("%8x ",	 stats.firmware_version );
    diag_printf ("%8x ",	 stats.major_version );
    diag_printf ("%8x ",	 stats.minor_version );
    diag_printf ("%8x ",	 stats.device_id );
    diag_printf ("%8x ",	 stats.self_test_status );
    diag_printf ("%8x ",	 stats.cert_length );
#if 1
    diag_printf ("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",	 
		 stats.cert_data[0],
		 stats.cert_data[1],
		 stats.cert_data[2],
		 stats.cert_data[3],
		 stats.cert_data[4],
		 stats.cert_data[5],
		 stats.cert_data[6],
		 stats.cert_data[7],
		 stats.cert_data[8],
		 stats.cert_data[9],
		 stats.cert_data[10],
		 stats.cert_data[11] );
#endif
  
#if 1
    NewLine();
    diag_printf ("%s\n", "cert_data follows...");
    Dump ("certificate", stats.cert_data, stats.cert_length, INCLUDE_ASCII);
    NewLine();
#endif

    if (0<mHandle) i2c_transport_channel_close(mHandle);
    
    exit_status = EXIT_SUCCESS;
    goto test_exit;
  } //!<<<< if (show_info...


  //!<<<< ................................................................
  //!<<<< loop mode
  //!<<<< ................................................................
  if (loop_mode) {  
    //!<<<< init >>>>
    exit_status = EXIT_SUCCESS;
    diag_verbose(VVVERBOSE, verbosity, "%s\n", "doing i2c channel init");
    i2c_transport_channel_initialize_system();
    sleep (1);
    //!<<<< open >>>>
    diag_verbose(VVVERBOSE, verbosity, "%s\n", "done init");
    mHandle = i2c_transport_channel_open( adapter,                        //!<<<< i.e: 4 for /dev/i2c-4 
					  bus_address,                    //!<<<< or kMFiAuthBusAddressLow
					  SHELBY_AUTHCHIP_REQUIRES_PEC,   //!<<<< pData->UsesPec ? TRUE : FALSE, >>>>
					  FALSE,                          //!<<<< pData->UsesSmbusProtocol ? TRUE : FALSE, >>>>
					  (VVVERBOSE==verbosity) ? !0 : 0 );                    //!<<<< pass our debug flag to lower levels
    if (0 > mHandle) {
      diag_verbose (ALWAYS, verbosity, "error opening i2c channel, handle=%x adapter=%x bus address=%x\n", mHandle, adapter, bus_address);
      exit_status=EXIT_FAILURE;
      goto test_exit;
    }
    sleep (1);
    diag_verbose(VVVERBOSE, verbosity, "%s mHandle=%d using address %d\n", "done open", mHandle,  kMFiAuthBusAddressLow);

    diag_log_ncurses_init(180,20);
    for (l=0; l<iterations; l++) {    


      //!<<<< ----------------------------------------------------------------
      //!<<<< do register reads here 
      //!<<<< keep error codex, error checking will occur later
      //!<<<< ----------------------------------------------------------------
      memset ((void *)&stats, 0, (sizeof(STATS)));

      //!<<<< ----------------------------------------------------------------
      //!<<<< error handling per interval
      //!<<<< fail on and register read error
      //!<<<< fail on self test failure
      //!<<<< do not fail on registers contents that may change (version, ID, etc)
      //!<<<< ----------------------------------------------------------------
      p=diag_StrTabIndex2Str(authErrorStrTab,sizeof(authErrorStrTab),PASS);
      if (mofMode.Get()) mofBuffer.SetErrorCode (PASS);
      if (mofMode.Get()) mofBuffer.SetErrorDetails (p);

      //!<<<< add dummy read, first time error
      auth_GetDeviceVersion(&stats.version);
      rc_00 = auth_GetDeviceVersion(&stats.version);
      if (!rc_00) {
	p=diag_StrTabIndex2Str(authErrorStrTab,sizeof(authErrorStrTab),NOK_DEVVER);
	diag_verbose (VVVERBOSE,verbose,p, rc_00, stats.version);
	if (mofMode.Get()) mofBuffer.SetErrorCode (NOK_DEVVER);
	if (mofMode.Get()) mofBuffer.SetErrorDetails (p, rc_00, stats.version);
	exit_status = EXIT_FAILURE;
	goto auth_err_10;
      }
      rc_01 = auth_GetFirmwareVersion(&stats.firmware_version);
      if ((!rc_01) || (1==test_flag)) {
	p=diag_StrTabIndex2Str(authErrorStrTab,sizeof(authErrorStrTab),NOK_FIRMVER);
	diag_verbose (VVVERBOSE,verbose,p, rc_01, stats.firmware_version);
	if (mofMode.Get()) mofBuffer.SetErrorCode (NOK_FIRMVER);
	if (mofMode.Get()) mofBuffer.SetErrorDetails (p, rc_01, stats.firmware_version);
	exit_status = EXIT_FAILURE;
	goto auth_err_10;
      }
      rc_02 = auth_GetProtocolMajorVersion(&stats.major_version);
      if ((!rc_02) || (2==test_flag)) {
	p=diag_StrTabIndex2Str(authErrorStrTab,sizeof(authErrorStrTab),NOK_MAJVER);
	diag_verbose (VVVERBOSE,verbose,p, rc_02, stats.major_version);
	if (mofMode.Get()) mofBuffer.SetErrorCode (NOK_MAJVER);
	if (mofMode.Get()) mofBuffer.SetErrorDetails (p, rc_02, stats.major_version);
	exit_status = EXIT_FAILURE;
	goto auth_err_10;
      }
      rc_03 = auth_GetProtocolMinorVersion(&stats.minor_version);
      if ((!rc_03) || (3==test_flag)) {
	p=diag_StrTabIndex2Str(authErrorStrTab,sizeof(authErrorStrTab),NOK_MINVER);
	diag_verbose (VVVERBOSE,verbose,p, rc_03, stats.minor_version);
	if (mofMode.Get()) mofBuffer.SetErrorCode (NOK_MINVER);
	if (mofMode.Get()) mofBuffer.SetErrorDetails (p, rc_03, stats.minor_version);
	exit_status = EXIT_FAILURE;
	goto auth_err_10;
      }
      rc_04 = auth_GetDeviceId(&stats.device_id);
      if (!rc_04) {
	p=diag_StrTabIndex2Str(authErrorStrTab,sizeof(authErrorStrTab),NOK_DEVID);
	diag_verbose (VVVERBOSE,verbose,p, rc_04, stats.device_id);
	if (mofMode.Get()) mofBuffer.SetErrorCode (NOK_DEVID);
	if (mofMode.Get()) mofBuffer.SetErrorDetails (p, rc_04, stats.device_id);
	exit_status = EXIT_FAILURE;
	goto auth_err_10;
      }
      rc_30 = auth_GetCertLength(&stats.cert_length);
      if (!rc_30) {
	p=diag_StrTabIndex2Str(authErrorStrTab,sizeof(authErrorStrTab),NOK_CERTLEN);
	diag_verbose (VVVERBOSE,verbose,p, rc_30, stats.cert_length);
	if (mofMode.Get()) mofBuffer.SetErrorCode (NOK_CERTLEN);
	if (mofMode.Get()) mofBuffer.SetErrorDetails (p, rc_30, stats.cert_length);
	exit_status = EXIT_FAILURE;
	goto auth_err_10;
      }
      rc_31 = auth_GetCertData(stats.cert_data, stats.cert_length);
      if (!rc_31) {
	p=diag_StrTabIndex2Str(authErrorStrTab,sizeof(authErrorStrTab),NOK_CERTDATA);
	diag_verbose (VVVERBOSE,verbose,p, rc_31, 0);
	if (mofMode.Get()) mofBuffer.SetErrorCode (NOK_CERTDATA);
	if (mofMode.Get()) mofBuffer.SetErrorDetails (p, rc_31, 0);
	exit_status = EXIT_FAILURE;
	goto auth_err_10;
      }
      stats.self_test_status = kMFiAuthSelfTestControl_RunTests;
      rc_40w = auth_SetSelfTestStatus(&stats.self_test_status);
      if (!rc_40w) {
	p=diag_StrTabIndex2Str(authErrorStrTab,sizeof(authErrorStrTab),NOK_SELFTSTW);
	diag_verbose (VVVERBOSE,verbose,p, rc_40w, stats.self_test_status);
	if (mofMode.Get()) mofBuffer.SetErrorCode (NOK_SELFTSTW);
	if (mofMode.Get()) mofBuffer.SetErrorDetails (p, rc_40w, stats.self_test_status);
	exit_status = EXIT_FAILURE;
	goto auth_err_10;
      }
      rc_40r = auth_GetSelfTestStatus(&stats.self_test_status);
      if (!rc_40r) {
	p=diag_StrTabIndex2Str(authErrorStrTab,sizeof(authErrorStrTab),NOK_SELFTSTR);
	diag_verbose (VVVERBOSE,verbose,p, rc_40r, stats.self_test_status);
	if (mofMode.Get()) mofBuffer.SetErrorCode (NOK_SELFTSTR);
	if (mofMode.Get()) mofBuffer.SetErrorDetails (p, rc_40r, stats.self_test_status);
	exit_status = EXIT_FAILURE;
	goto auth_err_10;
      }

      if ((rc_40w) && (rc_40r)) { 
	if ((kMFiAuthSelfTestStatus_CertificateNotFound|kMFiAuthSelfTestStatus_PrivateKeyNotFound) != stats.self_test_status) {
	p=diag_StrTabIndex2Str(authErrorStrTab,sizeof(authErrorStrTab),NOK_SELFTST);
	diag_verbose (VVVERBOSE,verbose,p, 0, rc_40w, stats.self_test_status);
	if (mofMode.Get()) mofBuffer.SetErrorCode (NOK_SELFTST);
	if (mofMode.Get()) mofBuffer.SetErrorDetails (p, rc_40w, stats.self_test_status);
	exit_status = EXIT_FAILURE;
	goto auth_err_10;
	}
      }

    auth_err_10:      
      //!<<<< this interval PASS FAIL status
      stats.intervalStatus = (EXIT_FAILURE==exit_status) ? 0 : !0;

      
      if (!mofMode.Get()) {
#ifndef NCURSES_ON
	if (first_time) {
#endif
	  //!<<<< do register contents display here >>>>
#ifndef AUTH_CLI_OFF
	  //!<<<< output >>>>
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%6s ",	    "");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s ",	    "");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	    "");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%35s ",	    "Versions");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	    "");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	    "");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%24s ",	    "");
#ifdef I2C_RETRY_DISPLAY
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%17s ",	    "I2C Retrys");
#endif
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%23s",	    "Self Test");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s",	    "Interval");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%24s\n",	    "");
	  
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%6s ",	"int");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s ",	"Bus");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	"Device");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	"Chip");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	"FW");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	"Maj");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	"Min");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	"DevID");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",   "Len");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%24s ",  "Cert []");
#ifdef I2C_RETRY_DISPLAY
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	"Read");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	"Write");
#endif
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%4s ",	"Stat");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	"x509Cert");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	"PrivKey");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s",	"Status");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%24s\n",	"Failure Reason");
	  
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%6s+",	"------");    
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s+",	"------------");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s+",	"--------");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%35s+",	"-----------------------------------");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s+",	"--------");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s+",	"--------");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%24s+",	"------------------------");
#ifdef I2C_RETRY_DISPLAY
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%17s+",	"-----------------");
#endif      
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%22s+",	"----------------------");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s",	"------------");
	  diag_log_ncurses((first_time) ? pLogFile : NULL,"%24s\n",	"-----------------------");
	  
#ifndef NCURSES_ON
	}
#endif
      } //!<<<< if ! mofMode

      if (!mofMode.Get()) {
	//!<<<< log entry
	diag_log_ncurses(pLogFile,"%6d ",	 interval );
	diag_log_ncurses(pLogFile,"%12s ", adapter_str );
	diag_log_ncurses(pLogFile,"%8d ",	 bus_address );
	diag_log_ncurses(pLogFile,"%8x ",	 stats.version);
	diag_log_ncurses(pLogFile,"%8x ",	 stats.firmware_version );
	diag_log_ncurses(pLogFile,"%8x ",	 stats.major_version );
	diag_log_ncurses(pLogFile,"%8x ",	 stats.minor_version );
	diag_log_ncurses(pLogFile,"%8x ",	 stats.device_id );
	diag_log_ncurses(pLogFile,"%8x ",	 stats.cert_length );
	diag_log_ncurses(pLogFile,"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x ",	 
			 stats.cert_data[0],
			 stats.cert_data[1],
			 stats.cert_data[2],
			 stats.cert_data[3],
			 stats.cert_data[4],
			 stats.cert_data[5],
			 stats.cert_data[6],
			 stats.cert_data[7],
			 stats.cert_data[8],
			 stats.cert_data[9],
			 stats.cert_data[10],
			 stats.cert_data[11] );
#ifdef I2C_RETRY_DISPLAY
	diag_log_ncurses(pLogFile,"%8d ",	 I2c_GetReadRetries() );
	diag_log_ncurses(pLogFile,"%8d ",	 I2c_GetWriteRetries() );
#endif
	diag_log_ncurses(pLogFile,"%4x ",	 stats.self_test_status );
	diag_log_ncurses(pLogFile,"%8s ",	 (stats.self_test_status&kMFiAuthSelfTestStatus_CertificateNotFound) ? "Pass" : "Fail" );
	diag_log_ncurses(pLogFile,"%8s ", (stats.self_test_status&kMFiAuthSelfTestStatus_PrivateKeyNotFound) ? "Pass" : "Fail" );
	diag_log_ncurses(pLogFile,"%12s ", (stats.intervalStatus) ? "Pass" : "Fail" );
	diag_log_ncurses(pLogFile,"%24s\n", (stats.intervalStatus) ? "" : p);
	diag_log_ncurses_refresh ();
      } //!<<<< if !mofMode
#endif


      //!<<<< ----------------------------------------------------------------
      //!<<<< JSON output lines (array of intervals)
      //!<<<< ----------------------------------------------------------------
      if (first_time) {
	p_buffer = (char *)malloc (buflen);
	if (NULL==p_buffer) {
	  diag_verbose(VERBOSE, verbosity, "error allocating memory for json buffer buflen=%d\n", buflen);    
	  exit (EXIT_FAILURE);
	}
	diag_verbose(VVERBOSE, verbosity, "allocated memory for json buffer buflen=%d\n", buflen);    
	if (mofMode.Get()) {
	  mofBuffer.Reset ();
	}
	jwOpen( &jwc, p_buffer, buflen, JW_OBJECT, JW_PRETTY );
	jwObj_array(&jwc, (char *)"intervals");
      }

      jwArr_object(&jwc);

      jwObj_string( &jwc, (char *)diag_StrTabIndex2Str(diag_mofRespStrTab,sizeof(STR_TAB_ENT ),MOF_RESP_STATUS), (char *)mofBuffer.GetStatus(stats.intervalStatus));
      snprintf (gp_str3, GP_STR_LEN, "%d", mofBuffer.GetErrorCode());
      jwObj_string( &jwc, (char *)diag_StrTabIndex2Str(diag_mofRespStrTab,sizeof(STR_TAB_ENT ),MOF_RESP_CODE), gp_str3);
      jwObj_string( &jwc, (char *)diag_StrTabIndex2Str(diag_mofRespStrTab,sizeof(STR_TAB_ENT ),MOF_RESP_DETAILS),  mofBuffer.GetErrorDetails());

      snprintf (gp_str, GP_STR_LEN, "%d", interval);
      jwObj_string( &jwc, (char *)"interval", gp_str);
      jwObj_string( &jwc, (char *)"adapter", adapter_str );
      snprintf (gp_str, GP_STR_LEN, "%d", bus_address);
      jwObj_string( &jwc, (char *)"bus",	 gp_str );
      snprintf (gp_str, GP_STR_LEN, "%x", stats.version);
      jwObj_string( &jwc, (char *)"version", gp_str);
      snprintf (gp_str, GP_STR_LEN, "%x", stats.firmware_version );
      jwObj_string( &jwc, (char *)"fwVersion", gp_str);
      snprintf (gp_str, GP_STR_LEN, "%x", stats.major_version );
      jwObj_string( &jwc, (char *)"majorVersion", gp_str);
      snprintf (gp_str, GP_STR_LEN, "%x", stats.minor_version );
      jwObj_string( &jwc, (char *)"minorVersion", gp_str);
      snprintf (gp_str, GP_STR_LEN, "%x", stats.device_id );
      jwObj_string( &jwc, (char *)"deviceId", gp_str);
      snprintf (gp_str, GP_STR_LEN, "%x", stats.cert_length );
      jwObj_string( &jwc, (char *)"certLen",  gp_str);
      snprintf (gp_str, GP_STR_LEN, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x ", 
		stats.cert_data[0],
		stats.cert_data[1],
		stats.cert_data[2],
		stats.cert_data[3],
		stats.cert_data[4],
		stats.cert_data[5],
		stats.cert_data[6],
		stats.cert_data[7],
		stats.cert_data[8],
		stats.cert_data[9],
		stats.cert_data[10],
		stats.cert_data[11] );
      jwObj_string( &jwc, (char *)"cert",  gp_str);

#ifdef I2C_RETRY_DISPLAY
      snprintf (gp_str, GP_STR_LEN, "%d",I2c_GetReadRetries());
      jwObj_string( &jwc, (char *)"readRetries", gp_str );
      snprintf (gp_str, GP_STR_LEN, "%d",I2c_GetWriteRetries());
      jwObj_string( &jwc, (char *)"writeRetries", gp_str );
#endif
      snprintf (gp_str, GP_STR_LEN, "%4x", stats.self_test_status );
      jwObj_string( &jwc, (char *)"selfTestStatus",  gp_str);
      jwObj_string( &jwc, (char *)"x409Cert", (stats.self_test_status&kMFiAuthSelfTestStatus_CertificateNotFound) ? (char *)"Pass" : (char *)"Fail" );
      jwObj_string( &jwc, (char *)"privKey",  (stats.self_test_status&kMFiAuthSelfTestStatus_PrivateKeyNotFound) ? (char *)"Pass" : (char *)"Fail" );
      
      diag_verbose(VVERBOSE, verbosity, "ending %d\n", 0);
      jwEnd(&jwc);

      if (mofMode.Get()) {
	mofBuffer.Write (jwc.buffer, (jwc.bufp-jwc.buffer));
	mofBuffer.Print();
	mofBuffer.Reset();
      }
      
      jWriteFile (&jwc, pJsonFile);
      
      //!<<<< loop
      if (first_time) first_time=0;
      interval++;
    } //!<<<< for (l=0; l<iterations; l++) {    


    //!<<<< ----------------------------------------------------------------
    //!<<<< JSON close & cleanup
    //!<<<< ----------------------------------------------------------------
    diag_verbose(VVERBOSE, verbosity, "ending %d\n", 0);
    jwEnd(&jwc);
    jwClose(&jwc);
    if (mofMode.Get()) {
      mofBuffer.Write (jwc.buffer, (jwc.bufp-jwc.buffer));
      mofBuffer.Print();
      mofBuffer.Reset();
    }
    jWriteFile (&jwc, pJsonFile);
    if (p_buffer) free (p_buffer);
    
    diag_log_ncurses_done ();
  } //!<<<< if (loop_mode)


 test_exit:
  if (EXIT_SUCCESS==exit_status) {
#if 0
    if (mofMode.Get()) {
#else
      if (0) {
#endif
      mofBuffer.Print();
    } else {
      diag_printf ("Device ID %d register reads succeeded\n", l);
      diag_printf ("Test Result Status: %s\n", "Pass");
    }
  } else {
    if (mofMode.Get()) {
      mofBuffer.Print();
    } else {
      diag_printf ("Device ID %d register reads failed\n", l);
      diag_printf ("Test Result Status: %s\n", "Fail");
    }
  }
  if (args) free (args);  
  if (0<mHandle) i2c_transport_channel_close(mHandle);
  exit (exit_status);
  
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< main()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int main(int argc, char *argv[]) {
  int c;
  int option_index=0;
  char optstr[GP_STR_LEN] = {DIAG_COMMON_OPTS "b:a:l:iT:"};
  char gp_str [GP_STR_LEN];
  char *p=0;

  //!<<<< getopt stuff
  static struct option long_options[] = {
    {"verbose",      no_argument,        0, 'v'},
    {"version",      no_argument,        0, 'V'},
    {"help",         no_argument,        0, 'h'},
    {"help-short",   no_argument,        0, 'H'},
    {"logfile",      required_argument,  0, 'L'},
    {"adapter",      required_argument,  0, 'a'},
    {"bus-address",  required_argument,  0, 'b'},
    {"loop",         required_argument,  0, 'l'},
    {"info",         no_argument,        0, 'i'},
    {"resetlogfile", no_argument,        0, 'R'},
    {"mof",          no_argument,        0, 'M'},
    {"test",         required_argument,  0, 'T'},
    {0,0,0,0},
  };

  //!<<<< ----------------------------------------------------------------
  //!<<<< disallow for unsupported platforms
  //!<<<< ----------------------------------------------------------------
  #if 0
  if (!diag_PlatformValid (valid_platforms_tab)) {
    diag_verbose (VERBOSE, verbosity,"command not supported for this platform: %s\n", diag_GetPlatform());
    exit (EXIT_SUCCESS);
  }
  #endif

  //!<<<< defaults
  memset ((void *)&tid_auth, 0, (sizeof(pthread_t)));
  memset ((void *)&stats, 0, (sizeof(STATS)));
  show_info=!0;
  loop_mode=0;
  iterations  = atoi(AUTH_ITERATIONS_DEFAULT);
  adapter     = atoi(AUTH_ADAPTER_DEFAULT);
  bus_address = atoi(AUTH_BUS_DEFAULT);
  test_flag=0;
  snprintf (adapter_str, GP_STR_LEN, "/dev/i2c-%d", adapter); 


  diag_setLogfile ((char *)DIAG_NAME);
  diag_verbose(VVERBOSE, verbosity, "setting logfile to: %s.log\n", DIAG_NAME);
  diag_verbose(VVERBOSE, verbosity, "setting jsonfile to: %s.json\n", DIAG_NAME);
	
  //!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
  //!<<<< process command line args
  //!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
  for (optind=0,opterr=0; (c = getopt_long (argc, argv, optstr ,long_options,&option_index)); ) {
    switch (c) {

      //!<<<< command specific options >>>>
      //!<<<< loop N times
    case 'l': 
      iterations = (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) ? atoi(optarg) : atoi(AUTH_ITERATIONS_DEFAULT);
      loop_mode=!0;
      show_info=0;
      diag_verbose(VVERBOSE, verbosity, "loop mode enabled: %d\n", loop_mode);
      diag_verbose(VVERBOSE, verbosity, "iterations: %d\n", iterations);
      break;
      //!<<<< info only
    case 'i': 
      show_info=!0;
      loop_mode=0;
      diag_verbose(VVERBOSE, verbosity, "show_info=%d\n", show_info);
      break;
      //!<<<< adapter
    case 'a': 
      adapter = (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) ? atoi(optarg) : atoi(AUTH_ADAPTER_DEFAULT);
      snprintf (adapter_str, GP_STR_LEN, "/dev/i2c-%d", adapter); 
      diag_verbose(VVERBOSE, verbosity, "adapter: %d %s\n", adapter, adapter_str);
      break;
      //!<<<< bus
    case 'b': 
      bus_address = (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) ? atoi(optarg) : atoi(AUTH_BUS_DEFAULT);
      diag_verbose(VVERBOSE, verbosity, "bus address: %d\n", bus_address);
      break;
    case 'M':
      mofMode.Set(!0);
      diag_verbose(VERBOSE, verbosity, "MOF mode set to %d\n", mofMode.Get());
      break;
    case 'T':
      test_flag=(diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) ? atoi(optarg) : atoi(AUTH_TEST_DEFAULT);
      diag_verbose(VVERBOSE, verbosity, "test arg: %d\n", test_flag);
      break;
      
      //!<<<< common options >>>>
    case 'V':
      diag_printVersion();
      exit (EXIT_SUCCESS);
    case 'L':
      if (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) {
	p = strstr (optarg, ".");
	if (p) *p=0;
	diag_setLogfile (optarg);
	diag_verbose(VVERBOSE, verbosity, "setting logfile to: %s.log\n", optarg);
	diag_verbose(VVERBOSE, verbosity, "setting jsonfile to: %s.json\n", optarg);
      } else {
	diag_setLogfile ((char *)DIAG_NAME);
	diag_verbose(VVERBOSE, verbosity, "setting logfile to: %s.log\n", DIAG_NAME);
	diag_verbose(VVERBOSE, verbosity, "setting jsonfile to: %s.json\n", DIAG_NAME);
      }
      break;
    case 'R':
      diag_resetLogFile ();
      break;
    case 'h':
      diag_help(auth_usage);
      exit (EXIT_SUCCESS);
    case 'H':
      diag_help(auth_usage_short);
      exit (EXIT_SUCCESS);
    case 'v':
      Verbosity.Inc();
      diag_verbose_x (ALWAYS, "Verbosity=%d\n", Verbosity.Get());
      break;
      
    case '?':
      diag_verbose (VERBOSE, verbosity, "getopt case ? %d\n", 0);
      if (NULL==strchr(optstr, optopt)) {
	diag_verbose(VVVERBOSE, verbosity, "optind = %d\n", optind);	
	diag_verbose(VVVERBOSE, verbosity, "optarg = %s\n", optarg);	
	diag_verbose(VVVERBOSE, verbosity, "optopt = %c\n", optopt);	
	diag_verbose(ALWAYS, verbosity, "invalid option c=%c argv=%s, optstr=%s\n", c, argv[optind-1], optstr);	
	exit (EXIT_FAILURE);
      }
      //!<<<< valid option, missing arg, apply defaults
      diag_verbose(VVVERBOSE, verbosity, "valid option, missing arg, applying defaults for opt = %c\n", optopt);	
      switch (optopt) {
      case 'l':
	iterations = atoi(AUTH_ITERATIONS_DEFAULT);
	loop_mode=!0;
	show_info=0;
	diag_verbose(VVERBOSE, verbosity, "loop mode enabled: %d\n", loop_mode);
	diag_verbose(VVERBOSE, verbosity, "iterations: %d\n", iterations);
	break;
      case 'a':
	adapter = atoi(AUTH_ADAPTER_DEFAULT);
	snprintf (adapter_str, GP_STR_LEN, "/dev/i2c-%d", adapter); 
	diag_verbose(VVERBOSE, verbosity, "adapter: %d %s\n", adapter, adapter_str);
	break;
      case 'b':
	bus_address = atoi(AUTH_BUS_DEFAULT);
	diag_verbose(VVERBOSE, verbosity, "bus address: %d\n", bus_address);
	break;
      case 'T':
	test_flag = atoi(AUTH_TEST_DEFAULT);
	diag_verbose(VVERBOSE, verbosity, "test flag: %d\n", test_flag);
	break;
      case 'L':
	diag_setLogfile ((char *)DIAG_NAME);
	diag_verbose(VVERBOSE, verbosity, "setting logfile to: %s.log\n", DIAG_NAME);
	diag_verbose(VVERBOSE, verbosity, "setting jsonfile to: %s.json\n", DIAG_NAME);
	break;
      }
      break;
    case ':':
      diag_verbose (ALWAYS, verbosity,"missing option arg option_index=%d argv[option_index]=%s\n", option_index, argv[option_index+1]);
      auth_usage ();
      exit (EXIT_SUCCESS);
      break;
    } 
    if (-1==c) break;
  } //!<<<< for

  //!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
  //!<<<< run the test
  //!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
  diag_log_hdr (LogFile.Get(), DIAG_NAME, argc, argv);
  diag_verbose(VVERBOSE, verbosity,"bus_address: %d\n", bus_address);
  diag_verbose(VVERBOSE, verbosity,"iterations: %d\n", iterations);
  diag_verbose(VVERBOSE, verbosity,"adapter: %d\n", adapter);
  
  //!<<<< start auth thread   >>>>
  diag_verbose (VVVERBOSE, verbosity, "%s\n", "starting auth thread");
  t_auth_args = (pAUTH_THREAD_ARGS)malloc (sizeof(AUTH_THREAD_ARGS));
  if (t_auth_args) {
    t_auth_args->secs=(1*SECONDS);
    if (0==pthread_create(&tid_auth, NULL, &auth_thread, t_auth_args)) {
      diag_verbose (VVVERBOSE, verbosity, "thread created %d\n", 0);
      //!<<<< give thread a name, use ps -T to view thread names
      pthread_setname_np(tid_auth, "auth_thread");
    } else {
      diag_verbose (VVVERBOSE, verbosity, "error creating thread %d\n", 0);
      diag_verbose (VVVERBOSE, verbosity, "freeing args=%p\n", (void *)t_auth_args);
      if (t_auth_args) free (t_auth_args);
      exit (EXIT_FAILURE);
    }
  }
  

  //!<<<< wait for end of test >>>>
  diag_verbose (VVERBOSE, verbosity, "stopping threads=%d\n", 0);
  auth_thread_stop=!0;
  
  void* ret = NULL;
  //!<<<< wait here until all threads are complete...
  if (tid_auth) {
    pthread_join (tid_auth,&ret);
    diag_verbose (VVERBOSE, verbosity, "auth thread returned status=%s\n", (char *)ret);
  }
  
  exit (EXIT_SUCCESS);
}	


//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< auth_GetDeviceVersion()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int auth_GetDeviceVersion(unsigned char *version) {
  int rc;
  int j,k;
  
  rc = i2c_transport_channel_read(mHandle, j=kMFiAuthReg_DeviceVersion, version, k=sizeof(*version));
  if (-1==rc) {
    diag_verbose (VERBOSE, verbosity, "error reading i2c, rc=%d reg=%d, len=%d\n", rc, j,k);
    return (0);
  }
  return (!0);
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< auth_GetFirmwareVersion()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int auth_GetFirmwareVersion(unsigned char *firmware_version) {
  int rc;
  int j,k;
  
  rc = i2c_transport_channel_read(mHandle, j=kMFiAuthReg_FirmwareVersion, firmware_version, k=sizeof(*firmware_version));
  if (-1==rc) {
    diag_verbose (VERBOSE, verbosity, "error reading i2c, rc=%d reg=%d, len=%d\n", rc, j,k);
    return (0);
  }
  return (!0);
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< auth_GetProtocolMajorVersion()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int auth_GetProtocolMajorVersion(unsigned char *major_version) {
  int rc;
  int j,k;
  
  rc = i2c_transport_channel_read(mHandle, j=kMFiAuthReg_ProtocolMajorVersion, major_version, k=sizeof(*major_version));
  if (-1==rc) {
    diag_verbose (VERBOSE, verbosity, "error reading i2c, rc=%d reg=%d, len=%d\n", rc, j,k);
    return (0);
  }
  return (!0);
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< auth_GetProtocolMinorVersion()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int auth_GetProtocolMinorVersion(unsigned char *minor_version) {
  int rc;
  int j,k;
  
  rc = i2c_transport_channel_read(mHandle, j=kMFiAuthReg_ProtocolMinorVersion, minor_version, k=sizeof(*minor_version));
  if (-1==rc) {
    diag_verbose (VERBOSE, verbosity, "error reading i2c, rc=%d reg=%d, len=%d\n", rc, j,k);
    return (0);
  }
  return (!0);
}
 //!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< auth_GetReg()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 int auth_GetReg(unsigned int reg, char *data) {
  int rc;
  int j,k;
  
  rc = i2c_transport_channel_read(mHandle, j=reg, data, k=1/*sizeof(unsigned int)*/);
  if (-1==rc) {
    diag_verbose (VERBOSE, verbosity, "error reading i2c, rc=%d reg=%d, len=%d\n", rc, j,k);
    return (0);
  }
  diag_verbose (VERBOSE, verbosity, "read i2c, rc=%d reg=%d, len=%d data=%x\n", rc, j,k, *data);
  //  *device_id = __bswap_32(*device_id);
  return (!0);
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< auth_GetDeviceId()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int auth_GetDeviceId(unsigned int *device_id) {
  int rc;
  int j,k;
  
  rc = i2c_transport_channel_read(mHandle, j=kMFiAuthReg_DeviceID, device_id, k=sizeof(*device_id));
  if (-1==rc) {
    diag_verbose (VERBOSE, verbosity, "error reading i2c, rc=%d reg=%d, len=%d\n", rc, j,k);
    return (0);
  }
  *device_id = __bswap_32(*device_id);
  return (!0);
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< auth_GetErrorCode()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int auth_GetErrorCode(unsigned char *error_code) {
  int rc;
  int j,k;
  
  rc = i2c_transport_channel_read(mHandle, j=kMFiAuthReg_ErrorCode, error_code, k=sizeof(*error_code));
  if (-1==rc) {
    diag_verbose (VERBOSE, verbosity, "error reading i2c, rc=%d reg=%d, len=%d\n", rc, j,k);
    return (0);
  }
  return (!0);
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< auth_GetSystemEventCounter()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int auth_GetSystemEventCounter(unsigned char *system_event_counter) {
  int rc;
  int j,k;
  
  rc = i2c_transport_channel_read(mHandle, j=kMFiAuthReg_SystemEventCounter, system_event_counter, k=sizeof(*system_event_counter));
  if (-1==rc) {
    diag_verbose (VERBOSE, verbosity, "error reading i2c, rc=%d reg=%d, len=%d\n", rc, j,k);
    return (0);
  }
  return (!0);
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< auth_GetAuthStatus()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int auth_GetAuthStatus(unsigned char *auth_status) {
  int rc;
  int j,k;
  
  rc = i2c_transport_channel_read(mHandle, j=kMFiAuthReg_Control_Status, auth_status, k=sizeof(*auth_status));
  if (-1==rc) {
    diag_verbose (VERBOSE, verbosity, "error reading i2c, rc=%d reg=%d, len=%d\n", rc, j,k);
    return (0);
  }
  return (!0);
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< auth_GetSelfTestStatus()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int auth_GetSelfTestStatus(unsigned char *self_test_status) {
  int rc;
  int j,k;
  
  rc = i2c_transport_channel_read(mHandle, j=kMFiAuthReg_SelfTest_Control_Status, self_test_status, k=sizeof(*self_test_status));
  if (-1==rc) {
    diag_verbose (VERBOSE, verbosity, "error reading i2c, rc=%d reg=%d, len=%d\n", rc, j,k);
    return (0);
  }
  return (!0);
}

int auth_SetSelfTestStatus(unsigned char *self_test_status) {
  int rc;
  int j,k;
  
  rc = i2c_transport_channel_write(mHandle, j=kMFiAuthReg_SelfTest_Control_Status, self_test_status, k=sizeof(*self_test_status));
  if (-1==rc) {
    diag_verbose (VERBOSE, verbosity, "error writing i2c, rc=%d reg=%d, len=%d\n", rc, j,k);
    return (0);
  }
  return (!0);
}



//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< auth_GetCertLength()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int auth_GetCertLength(unsigned short *cert_length) {
  int rc;
  int j,k;
  unsigned char buf [2];
  rc = i2c_transport_channel_read(mHandle, j=kMFiAuthReg_AccessoryDeviceCertificateSize, buf, k=sizeof(buf));
  if (-1==rc) {
    diag_verbose (VERBOSE, verbosity, "error reading i2c, rc=%d reg=%d, len=%d\n", rc, j,k);
    return (0);
  }
  *cert_length = ((buf[0] << 8) | buf[1]);
  return (!0);
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< auth_GetCertData()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int auth_GetCertData(unsigned char *cert_data, unsigned short cert_len) {
  int rc;
  int j,k;
  unsigned char buf [CERT_STR_MAX];
  if (NULL==cert_data) {
    diag_verbose (VERBOSE, verbosity, "error cert data ptr = %p\n", cert_data);
    return (0);
  }
  if (0==cert_len) {
    diag_verbose (VERBOSE, verbosity, "error cert len = %d\n", cert_len);
    return (0);
  }
  if (CERT_STR_MAX<=cert_len) {
    diag_verbose (VERBOSE, verbosity, "error cert len exceeds %d= %d\n", cert_len, CERT_STR_MAX);
    return (0);
  }
  rc = i2c_transport_channel_read(mHandle, j=kMFiAuthReg_AccessoryDeviceCertificateData1, buf, k=cert_len);
  if (-1==rc) {
    diag_verbose (VERBOSE, verbosity, "error reading i2c, rc=%d reg=%d, len=%d\n", rc, j,k);
    return (0);
  }
  memcpy (cert_data, buf, cert_len);
  return (!0);
}





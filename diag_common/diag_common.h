#ifndef __DIAG_H__
#define __DIAG_H__

//!<<<< definitions
#define K                 (1000)
#define KB                (1024)
#define M                 (K*K)
#define SECONDS           (1) //!<<<< i.e. sleep(2*SECONDS)
#define ALWAYS            (0)
#define VERBOSE           (1)
#define VVERBOSE          (2)
#define VVVERBOSE         (3)
#define MORE_VERBOSE      VVERBOSE
#define MORE_MORE_VERBOSE VVVERBOSE
#define RDW_EXE_DIR       "/opt/Bose/exe/"
#define RDW_TMP_DIR       "/opt/Bose/tmp/"
#define PROC_NET_DEV      "/proc/net/dev"
#define PROC_NET_SNMP     "/proc/net/snmp"
#ifdef MOF_INT
#define DIAG_COMMON_OPTS  "L:RHhVvM"
#else
#define DIAG_COMMON_OPTS  "L:RHhVv"
#endif

typedef enum {
  _ALWAYS    = ALWAYS,
  _VERBOSE   = VERBOSE,
  _VVERBOSE  = VVERBOSE,
  _VVVERBOSE = VVVERBOSE,
} VERBOSITY;

//!<<<< handle case where build tool is not passing in version string
#ifndef MODULE_VERSION
#define MODULE_VERSION ""
#endif
#ifndef MODULE_VERSION2
#define MODULE_VERSION2 ""
#endif
#ifndef MODULE_BLTWHEN
#define MODULE_BLTWHEN ""
#endif

#ifndef COMPONENT_VERSION_CASTLE_CLI
#define COMPONENT_VERSION_CASTLE_CLI ""
#endif
#ifndef COMPONENT_VERSION_CASTLE_LPM_MFG_TOOL
#define COMPONENT_VERSION_CASTLE_LPM_MFG_TOOL ""
#endif
#ifndef COMPONENT_VERSION_PROTOBUF
#define COMPONENT_VERSION_PROTOBUF ""
#endif
#ifndef COMPONENT_VERSION_RIVIERA_HSP
#define COMPONENT_VERSION_RIVIERA_HSP ""
#endif
#ifndef COMPONENT_VERSION_RIVIERA_LPM_SERVICE
#define COMPONENT_VERSION_RIVIERA_LPM_SERVICE ""
#endif
#ifndef COMPONENT_VERSION_RIVIERA_LPM_UPDATER
#define COMPONENT_VERSION_RIVIERA_LPM_UPDATER ""
#endif


#define GP_STR_LEN        (1024)
#define MAX_CORES         (16)
#define NewLineStrip(_LINE_) { \
  char *pos;		       \
  pos=strchr((_LINE_),'\n');   \
  if (pos) *pos='\0';	       \
  }

//!<<<< dump flags
typedef enum {
  INCLUDE_ASCII=1,
  DONT_INCLUDE_ASCII=2,
} DUMP_FLAGS;

typedef enum {
  R,
  W,
} RW;


typedef struct {
  unsigned int Index;
  const char *Str;
} STR_TAB_ENT, *p_STR_TAB_ENT;

typedef enum {
  PASS=0,
  FAIL=1,
} PASSFAIL;

#define DIAG_PASS "PASS"
#define DIAG_FAIL "FAIL"

//!<<<< platforms
#define DIAG_PLATFORM_EDDIE "Eddie"
#define DIAG_PLATFORM_PROFESSOR "Professor"
#define DIAG_PLATFORM_GINGERCHEEVERS "ginger-cheevers"
#define DIAG_PLATFORM_FLIPPER "Flipper"
#define DIAG_PLATFORM_TAYLOR "Taylor"
#define DIAG_PLATFORM_RIVIERA "Riviera"
#define DIAG_PLATFORM_QUALCOMM "Qualcomm"
#define DIAG_PLATFORM_SANDIEGO "San Diego"

//!<<<< for platform type table
typedef struct {
  const char *model;
} DIAG_PLATFORM_TAB_ENT, *pDIAG_PLATFORM_TAB_ENT;

typedef struct {
  const char *pPlatformStr;
} DIAG_VALID_PLATFORMS_ENT, *pDIAG_VALID_PLATFORMS_ENT;

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< debug stuff
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#define diag_ticks_2_usecs(_TICKS_) (((1.0/sysconf(_SC_CLK_TCK))*K*K)*(_TICKS_))
#define diag_printf(args,...) printf(args,__VA_ARGS__)
#define diag_printf_f(_FP_,args,...) {fprintf((_FP_), args,__VA_ARGS__);fflush((_FP_));}
#define diag_debug(args,...) printf("[%s:%s:%d] " args,__FILE__,__FUNCTION__,__LINE__,__VA_ARGS__)
#define diag_debug_f(_FP_,args,...) {fprintf((_FP_),"[%s:%s:%d] " args,__FILE__,__FUNCTION__,__LINE__,__VA_ARGS__);fflush((_FP_));}

#define diag_verbose(_VERBOSE_LEVEL_, _VERBOSITY_, args,...) diag_verbose_x((_VERBOSE_LEVEL_),args,__VA_ARGS__)

#define diag_verbose_x(_VERBOSE_LEVEL_, args,...) {			\
    if ((_VERBOSE_LEVEL_)<=Verbosity.Get()) {	\
      char _p[GP_STR_LEN];						\
      snprintf (_p, GP_STR_LEN, "%s%s", "[%s:%s:%d] ", args);		\
      diag_log_x (_p, __FILE__,__FUNCTION__,__LINE__,__VA_ARGS__);	\
    }									\
  }

#define diag_verbose_f(_FP_,_VERBOSE_LEVEL_, _VERBOSITY_, args,...) if ((_VERBOSE_LEVEL_)<=(_VERBOSITY_)) {fprintf((_FP_),"[%s:%s:%d] " args,__FILE__,__FUNCTION__,__LINE__,__VA_ARGS__);fflush((_FP_));}
#define diag_verbosity(_VERBOSE_LEVEL_,_VERBOSITY_) ((_VERBOSE_LEVEL_)<=(_VERBOSITY_))
#define diag_system(_VERBOSE_LEVEL_,_VERBOSITY_,_CMD_) {		\
    int ret=0;								\
    if (NULL==(_CMD_)) {						\
      diag_debug_f(stdout,"system(%s) command NULL, exit\n", (_CMD_));	\
      exit (EXIT_FAILURE);						\
    }									\
    ret=system((_CMD_));						\
    if (-1==ret) {							\
      diag_debug_f(stdout,"system(%s) command error, exit\n", (_CMD_));	\
      exit (EXIT_FAILURE);						\
    }									\
    if (127==WEXITSTATUS(ret)) {					\
      diag_debug_f(stdout,"system(%s) command not found, exit\n", (_CMD_)); \
      exit (EXIT_FAILURE);						\
    }									\
  } 

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< MOF BUFFER variable & methods
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
typedef enum {
  MOF_RESP_STATUS=0,
  MOF_RESP_CODE=1,
  MOF_RESP_DETAILS=2,
} MOF_RESP;
#define MOF_BUFFER_LEN (GP_STR_LEN*4)

class MOF_BUFFER {
 private:
  char mofBuf [MOF_BUFFER_LEN];
  char *p_mofBuf;
  int mofBytes;
  char ErrStr[GP_STR_LEN];
  unsigned int ErrCode;
 public:
  MOF_BUFFER (void);
  void Write (char *_p, unsigned int _len);
  void Reset (void);
  void Print (void);
  void SetErrorCode (unsigned int _err);
  void SetErrorDetails (const char *_pFmt, ...);
  unsigned int GetErrorCode (void);
  char *GetErrorDetails (void);
  const char *GetStatus(int f) { return ((f) ? DIAG_PASS : DIAG_FAIL );};
  char *Get (void) { return (mofBuf);};
};

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< FLAG variable & methods
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
class FLAG {
 private:
  unsigned int Val;
 public:
  FLAG (void)                 { this->Val=0; };
  FLAG (unsigned int Val)     { this->Val=Val; };
  void Set (unsigned int Val) { this->Val=Val; };
  void Inc (void)             { this->Val+=1; };
  unsigned int Get (void)     { return (this->Val); };
  void Toggle (void)          { this->Val=(this->Val) ? 0 : !0; };
};

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< FILE variable & methods
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
class LOGFILE {
 private:
  char filename [GP_STR_LEN];
 public:
  LOGFILE (void)           { filename[0]=0; };
  void Set (char *filename) { strncpy (this->filename, filename, GP_STR_LEN); };
  char *Get (void)          { return (this->filename); };
  int Exists (void)         { return ((0==this->filename[0]) ? 0 : !0); };
};

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< ethernet stats
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#define ETH_STATS_FILE           "/proc/net/dev"

typedef struct ETH_STATS {
  char interface [GP_STR_LEN];
  int rx_bytes;
  int rx_packets;
  int rx_errs;
  int rx_drop;
  int rx_fifo;
  int rx_frame;
  int rx_compressed;
  int rx_multicast;
  int tx_bytes;
  int tx_packets;
  int tx_errs;
  int tx_drop;
  int tx_fifo;
  int tx_colls;
  int tx_carrier;
  int tx_compressed;
} ETH_STATS, *pETH_STATS;

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< prototypes
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int file_exists (char *filename);
int diag_ScrapeFileLine (char *pFileName, const char *pFmt, ...);
int diag_ScrapeFileLinePat (char *pFileName, const char *pPattern, int occurences, const char *pFmt, ...);
int diag_ScrapeFileLinePatPtr (char *pFileName, const char *pPattern, int occurences, char *pLine, int len);
void diag_log_hdr (char *pFileName, const char *pTest, int argc, char **argv);
void diag_log_new (char *pFileName);
void diag_log (char *pFileName, const char *pFmt, ...);
void diag_log_ncurses_done ();
void diag_log_ncurses_init (int _x, int _y);
void diag_log_ncurses_refresh ();
void diag_log_ncurses (char *pFileName, const char *pFmt, ...);
void diag_log_and_print (char *pFileName, const char *pFmt, ...);
void diag_printVersion ();
void diag_printComponentVersions ();
char *diag_StrTrunc (char *str, int len);
void Dump (const char *Heading, uint8_t *pBuff, uint32_t Len, DUMP_FLAGS Flags);
void diag_splitComponents (char *str, char **pName, char **pSelected, char **pLatest);
int diag_IsPlatform (const char *pStr);
const char *diag_GetPlatform (void);
char *diag_GetPlatform (char *pStr);
int diag_PlatformValid (pDIAG_VALID_PLATFORMS_ENT pTab);
void diag_setVerbosity (int *_verbosity);
void diag_resetLogfile (int *_resetLogfile);
void diag_resetLogfile (char *pLogFile, const char *pDiagName);
void diag_resetLogFile (void);
void diag_setLogfile (char *_logFilename, int logFileLen, char *_logFileArg);
void diag_setLogfile (char *_logFilename);
void diag_help (void (*fp)(void));
void diag_usage_short (const char *s1, const char *s2);
void diag_getopt_ArgsDisplay (int argc, char **argv);
int diag_getopt_OptArgExists (int argc, char **argv, char opt, char *arg, int *optind);
void NewLine(int lines);
void NewLine(void);
void diag_log_x (const char *pFmt, ...);
char *diag_GlobalLogfileAccess(RW rw, char *pLogfile);
char *diag_GlobalLogfileGet (void);
void diag_GlobalLogfileSet (char *_pLogFile);
const char *diag_StrTabIndex2Str (p_STR_TAB_ENT pEnt, size_t maxIndex, unsigned int Index);
int diag_StrTabStr2Index (p_STR_TAB_ENT pEnt, size_t tabSize, const char *str);
void diag_SubStrRemove (char *pSrc, const char *pPat, const char *pPad);
#ifdef JSON_READ
struct FileBuffer{
  unsigned long length;			// length in bytes
  unsigned char *data;			// malloc'd data, free with freeFileBuffer()
};
#define FILE_BUFFER_MAXLEN (1024*2048)

unsigned long readFileBuffer( char *filename, struct FileBuffer *pbuf, unsigned long maxlen );
void freeFileBuffer( struct FileBuffer *buf );
#endif
int diag_validProductType (char *productType);
int random_int (int min, int max);
void srandom_int (int seed);
int eth_GetStats (char *interface, ETH_STATS *stats);
int eth_RecStats (pETH_STATS r_stats, ETH_STATS b_stats, ETH_STATS e_stats);
float diag_MIN (float _A_, float _B_);
double diag_MIN (double _A_, double _B_);
int diag_MIN (int _A_, int _B_);
float diag_MAX (float _A_, float _B_);
double diag_MAX (double _A_, double _B_);
int diag_MAX (int _A_, int _B_);
#endif

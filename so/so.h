#ifndef __SO_H__
#define __SO_H__

//!<<<< definitions
#define MODULE_NAME            "so.c"
#define DIAG_NAME              "dt_so"

#define SO_SLIMBUS_CHANNEL_DEFAULT     "DARRS"
#define SO_MI2S2BUS_CHANNEL_DEFAULT    "BW"
#define SO_TESTBYTES_DEFAULT   "64"
#define SO_DURATION_DEFAULT    "86400" //!<<<< 24 hours
#define SO_FREQUENCY_DEFAULT   "1000"
#define SO_AMPLITUDE_DEFAULT   "-40.0"
#define SO_NUM_SLIMBUS_CHANNELS  (10)
#define SO_NUM_MI2S2BUS_CHANNELS (0)
#define SO_NUM_MAX_CHANNELS      SO_NUM_SLIMBUS_CHANNELS
#define SO_FIFO_PATH           "/opt/Bose/tmp/so_fifo.raw"
#define SO_OUTFILE_DEFAULT     "/opt/Bose/tmp/so_out.raw"
#define SO_PCM_FORMAT          "S16_LE"

// use "alpay -l" command to list all devices
#define SO_DEVICE              "hw:0,0"
#define SO_DEVICE_12           "hw:0,12"

#define SO_CMD_OD              "( od -vtx2 %s)&"
//#define SO_CMD_APLAY           "( aplay --quiet -t raw -D %s %s -c%d -r%d -f%s)&"
#define SO_CMD_APLAY           "( aplay -t raw -D %s %s -c%d -r%d -f%s)&"

#define SO_HI_THRESH           (32*KB)
#define SO_LO_THRESH           (16*KB)
#define SO_OPT_STR             "c:C:f:a:d:to:"

typedef enum {
  SLIMBUS,
  MI2S2BUS,
  NONE,
} WHICH_BUS;

//!<<<< AMIXER command table
#define USR_BIN "/usr/bin/"
typedef struct {
  const char *pAmixerStr;
} AMIXER_CMD_TAB_ENT, *pAMIXER_CMD_TAB_ENT;

//!<<<< thread parameters
typedef struct {
  char cmd [GP_STR_LEN];
} SO_THREAD_ARGS, *pSO_THREAD_ARGS;

//!<<<<SO_ channel index & strings
//!<<<< SLIMBUS
#define SO_DARR_Left   (2)
#define SO_DARR_Right  (3)
#define SO_DARR_Sub    (0)
#define SO_DARR_NOP    (1)
#define SO_Bass        (4)
#define SO_Left_Outer  (5)
#define SO_Left_Atmos  (6)
#define SO_Center      (7)
#define SO_Right_Atmos (8)
#define SO_Right_Outer (9)


#define SO_DARR_Left_STR   "DARRL"
#define SO_DARR_Right_STR  "DARRR"
#define SO_DARR_Sub_STR    "DARRS"
#define SO_DARR_NOP_STR    "DARRNOP"
#define SO_Bass_STR        "Bass"
#define SO_Left_Outer_STR  "Left"
#define SO_Left_Atmos_STR  "AtmosL"
#define SO_Center_STR      "Center"
#define SO_Right_Atmos_STR "AtmosR"
#define SO_Right_Outer_STR "Right"

//!<<<< so config struct, indexed by channel index
typedef enum {
  SO_SWEEP_TYPE_NONE=0,
  SO_SWEEP_TYPE_LINEAR=1,
  SO_SWEEP_TYPE_LOG=2,
} SO_SWEEP_TYPE;

typedef struct {
  int enable;
  int channel;
  SO_SWEEP_TYPE sweep;
  int frequency;
  int frequency_min;
  int frequency_max;
  double amplitude;
} SO_CFG_ENT,*pSO_CFG_ENT;

typedef struct {
  int chn;
  int freq_man;
  int freq_min;
  float amp_max;
} SO_CHN_PARAMS_ENT, *pSO_CHN_PARAMS_ENT;

//!<<<< prototypes
int main (int argc, char *argv[]);
void so_usage (void);
void so_usage_short (void);
void *so_thread (void *args);
double undb20(double gainDb);
double db20(double gainLinear);
void so_splitFrequency (char *str, const char *delim, char **pL, char **pR);
void signal_handler (int signum);
float so_ChnParamsChn2MaxDb (int chn);
int so_ChnParamsChn2MinFreq (int chn);
#endif

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
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <byteswap.h>
#include "diag_common.h"
#include "so.h"
#include "jWrite.h"
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/times.h>

#ifdef MOF_INT
#pragma message("==> MOF_INT defined")
#endif
#ifdef AUTH_CLI_OFF
#pragma message("==> SO_CLI_OFF defined")
#endif


//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< largely ported from SO python script to allow for continuous
//!<<<< output not limited to file size
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< globals
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
struct globals {
  FILE *fd;
  FILE *fd_out;
  int duration;
  MOF_BUFFER mofBuffer;
  FLAG mofMode;
  int so_thread_stop;
  pSO_THREAD_ARGS t_so_args;
  pthread_t tid_so;
  SO_CFG_ENT so_cfg [SO_NUM_MAX_CHANNELS];
  int numGenerators;
  int numFreqs;
  int numAmps;
  int test_cmd;
  int sampleRate;
  int freqMax;
  char afifo [GP_STR_LEN];
  char out_file [GP_STR_LEN];
  int got_sigint;
  int exit_status;
  int numChannels;
  WHICH_BUS bus;
} globals, *p_globals=&globals;
#define G (*p_globals)

extern FLAG Verbosity;
extern LOGFILE LogFile;
extern LOGFILE JsonFile;
#define verbosity Verbosity.Get()
#define pLogFile LogFile.Get()
#define pJsonFile JsonFile.Get()
extern STR_TAB_ENT diag_mofRespStrTab[];

char optstr[GP_STR_LEN] = {DIAG_COMMON_OPTS SO_OPT_STR};

//!<<<< error strings
//STR_TAB_ENT soErrorStrTab[] = {
//  {PASS,DIAG_PASS},
//  {FAIL,DIAG_FAIL},
//};

STR_TAB_ENT SLIMBUS_channelTab [] = {
  {SO_DARR_Sub,    SO_DARR_Sub_STR    },
  {SO_DARR_NOP,    SO_DARR_NOP_STR    },
  {SO_DARR_Left,   SO_DARR_Left_STR   },
  {SO_DARR_Right,  SO_DARR_Right_STR  },
  {SO_Bass,        SO_Bass_STR        },
  {SO_Left_Outer,  SO_Left_Outer_STR  },
  {SO_Left_Atmos,  SO_Left_Atmos_STR  },
  {SO_Center,      SO_Center_STR      },
  {SO_Right_Atmos, SO_Right_Atmos_STR },
  {SO_Right_Outer, SO_Right_Outer_STR }
};

SO_CHN_PARAMS_ENT paramsTab [] ={
  // ch        freq_max freq_min, amp_max
  {SO_DARR_Sub,     10,        0,  -0.0},
  {SO_DARR_NOP,     10,        0,  -0.0},
  {SO_DARR_Left,    10,        0,  -0.0},
  {SO_DARR_Right,   10,        0,  -0.0},
  {SO_Bass,         10,        0,  -0.0},
  {SO_Left_Outer,   10,        0,  -0.0},
  {SO_Left_Atmos,   10,        0,  -0.0},
  {SO_Center,     2000,        0,  -0.0},
  {SO_Right_Atmos,  10,        0,  -0.0},
  {SO_Right_Outer,  10,        0,  -0.0}
};

AMIXER_CMD_TAB_ENT tinymix_cmd_tab [] = {
  //Set the mixer control path for C0 board
  {  USR_BIN "tinymix set \'PRI_MI2S_RX Audio Mixer MultiMedia1\' \'1\'\n"},
  {  USR_BIN "tinymix set \'PRIM_MI2S_RX Format\' \'S32_LE\'\n"           },
  {  USR_BIN "tinymix set \'PRIM_MI2S_RX Channels\' \'Fourteen\'\n"       },
  {  NULL}
};

//Reset the mixer paths
AMIXER_CMD_TAB_ENT reset_tinymix_cmd_tab [] = {
  //"Resetting Mixer control for C0 Board"
  {  USR_BIN "tinymix set \'PRI_MI2S_RX Audio Mixer MultiMedia1\' \'0\'\n"},
  {  NULL}
};     

//!<<<< ................................................................
//!<<<< signal_handler()
//!<<<< ................................................................
void signal_handler (int signum) {
  if (SIGINT==signum) {
    diag_verbose(VVVERBOSE,verbosity,"SIGINT (CNTL C) received. %d\n",signum);
    fprintf (stdout, "Test Complete 1\n");
    fflush (stdout);
    G.got_sigint=!0;
  } else {
    diag_verbose(VVVERBOSE,verbosity,"SIGX received. %d\n",signum);
  }
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< usage()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void so_usage_short (void) {
  diag_usage_short (DIAG_NAME, "SO Test");
  exit (EXIT_SUCCESS);
}
void so_usage (void) {
  fprintf (stdout, "usage:  %s %s\n", DIAG_NAME, optstr);
  fprintf (stdout, " -c,--channel list:\n {%s|%s|%s|%s|%s|%s|%s|%s|%s|%s}\n\n",
	   SO_DARR_Sub_STR,
	   SO_DARR_NOP_STR,
	   SO_DARR_Left_STR,   
	   SO_DARR_Right_STR,
	   SO_Bass_STR,
	   SO_Left_Outer_STR,
	   SO_Left_Atmos_STR,
	   SO_Center_STR,
	   SO_Right_Atmos_STR,
	   SO_Right_Outer_STR);

  fprintf (stdout, " -f,--frequency        Frequency (must follow -c), default=%s\n", SO_FREQUENCY_DEFAULT);
  fprintf (stdout, " -f x:y,--frequency    Frequency log sweep (must follow -c)\n");
  fprintf (stdout, " -f x-y,--frequency    Frequency linear sweep (must follow -c)\n");
  fprintf (stdout, " -a,--amplitude        Amplitude (must follow -c), default=%s\n", SO_AMPLITUDE_DEFAULT);
  fprintf (stdout, " -d,--duration         Duration in seconds, default=%s\n", SO_DURATION_DEFAULT);
  fprintf (stdout, " -h,--help             Print this message\n");
  fprintf (stdout, " -H,--help-short       Print test summary message\n");
  fprintf (stdout, " -V,--version          show build version\n");
  fprintf (stdout, " -v[vv],--verbose      Verbose level\n");
  fprintf (stdout, " -L,--logfile          Output to logfile in /opt/Bose/tmp/, default %s.log\n", DIAG_NAME);
  fprintf (stdout, " -R,--resetlogfile     Reset logfile\n\n");

  fprintf (stdout, "Example: %s -d20 -c%s -f%s -a%s (output %shz on channel %s for 20 seconds at %sdB)\n",
	   DIAG_NAME,  SO_DARR_Left_STR, SO_FREQUENCY_DEFAULT, SO_AMPLITUDE_DEFAULT, SO_FREQUENCY_DEFAULT,  SO_DARR_Left_STR, SO_AMPLITUDE_DEFAULT);
  fprintf (stdout, "Example: %s -c%s -f%s -a%s (output %shz on channel %s for %s seconds at %sdB)\n",
	   DIAG_NAME,  SO_DARR_Left_STR, SO_FREQUENCY_DEFAULT, SO_AMPLITUDE_DEFAULT, SO_FREQUENCY_DEFAULT,  SO_DARR_Left_STR,  SO_DURATION_DEFAULT, SO_AMPLITUDE_DEFAULT);
  fprintf (stdout, "Example: %s -d20 -c%s -f%s -a%s  -c%s -f%s -a%s  -c%s -f%s -a%s (output %shz on channel %s and %s and %s for 20 seconds at %sdB)\n",
	   DIAG_NAME,  SO_Left_Outer_STR, SO_FREQUENCY_DEFAULT, SO_AMPLITUDE_DEFAULT,  SO_Left_Outer_STR, SO_FREQUENCY_DEFAULT, SO_AMPLITUDE_DEFAULT,
	   SO_Left_Outer_STR, SO_FREQUENCY_DEFAULT, SO_AMPLITUDE_DEFAULT, SO_FREQUENCY_DEFAULT,  SO_Left_Outer_STR, SO_Left_Outer_STR, SO_Left_Outer_STR, SO_AMPLITUDE_DEFAULT);
  fprintf (stdout, "Example: %s -d20 -c%s -f100:4000 -a%s (output log sweep on channel %s for 20 seconds at %sdB)\n",
	   DIAG_NAME,  SO_Right_Outer_STR, SO_AMPLITUDE_DEFAULT,  SO_Right_Outer_STR, SO_AMPLITUDE_DEFAULT);
  fprintf (stdout, "Example: %s -d20 -c%s -f100-4000 -a%s (output linear sweep on channel %s for 20 seconds at %sdB)\n",
	   DIAG_NAME,  SO_Right_Outer_STR,  SO_AMPLITUDE_DEFAULT, SO_Right_Outer_STR, SO_AMPLITUDE_DEFAULT);
  
  fprintf (stdout, "\nThe following are for wireless speakers, which are paired to this system\n");
  fprintf (stdout, "Example: %s %2s %5s %5s %s\n", DIAG_NAME, "-d5", "-cDARRL", "-f400", "-a-40.0");
  fprintf (stdout, "Example: %s %2s %5s %5s %s\n", DIAG_NAME, "-d5", "-cDARRR", "-f500", "-a-40.0");
  fprintf (stdout, "Example: %s %2s %5s %5s %s\n", DIAG_NAME, "-d5", "-cDARRS", "-f50",  "-a-20.0");

  fprintf (stdout, "\nPaired wireless speakers will display a solid white led.\n");
  fprintf (stdout, " Wireless speaker pairing procedure:\n");
  fprintf (stdout, "  At the LPM console prompt enter command  >>  'wa j' then power on speakers.\n");
  fprintf (stdout, "  The speakers should pair(blinking amber led changes to white) within 30 seconds.\n");
  fprintf (stdout, "  A blinking amber led indicates the speaker is in pairing mode.\n");
  fprintf (stdout, "  The speaker must be in pairing mode for the pairing procedure to succeed.\n");
   
  NewLine ();
  NewLine ();
  exit (EXIT_SUCCESS);
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< so thread
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void *so_thread (void *args) {
  pSO_THREAD_ARGS p_args = (pSO_THREAD_ARGS)args;
  
  diag_verbose (VVERBOSE, verbosity, "at SO thread cmd=%s\n", p_args->cmd);
  
  //!<<<< start background command
  diag_verbose (VVERBOSE, verbosity, "starting background command=%s\n", p_args->cmd);
  system (p_args->cmd);
  
  //!<<<< open FIFO file for writing
  //!<<<< NOTE: when you open a FIFO file for writing it is blocked until
  //!<<<< there is a reader (aplay) (which was just started w/system(aplay)
  G.fd = fopen(G.afifo, "wb");
  if (NULL==G.fd) {
    diag_verbose (VVERBOSE, verbosity, "error during file open for binary write=%p\n", G.fd);
    G.exit_status=!0;
  }
  diag_verbose (VVERBOSE, verbosity, "file %s open for binary write=%p\n", G.afifo, G.fd);
  G.fd_out = fopen(G.out_file, "wb");
  if (NULL==G.fd_out) {
    diag_verbose (VVERBOSE, verbosity, "error during file open for binary write=%p\n", G.fd_out);
    G.exit_status=!0;
  }
  diag_verbose (VVERBOSE, verbosity, "file %s open for binary write=%p\n", G.out_file, G.fd_out);
  diag_verbose (VVERBOSE, verbosity, "aplay started and file open\n", 0);

  while (!G.so_thread_stop) {
    sleep (1*SECONDS);
    diag_verbose (VVERBOSE, verbosity, "SO thread loop\n", 0);
  }
  //!<<<< killall aplay, it takes a second to stop even thought he thread gets killed.
  diag_verbose (VVVERBOSE, verbosity, "kill aplay\n", 0);
  system ("killall aplay");
  diag_verbose (VVERBOSE, verbosity, "SO thread stopped\n", 0);

  //!<<<< done
  if (args) free (args);
  return((void *)NULL);
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< main()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int main(int argc, char *argv[]) {
  int c;
  int option_index=0;
  char gp_str [GP_STR_LEN];
  char *p=0;
  pAMIXER_CMD_TAB_ENT q=NULL;
  int i;
  int chn;
  int gen;
  char *pL;
  char *pR;
  float freqMinLimit=0;
  int freqMinLimitCh=0;
  float ampDbMaxLimit=0.0;
  int ampDbMaxLimitCh=0;
  pSO_CFG_ENT pCfg;
  float ampLinear=0.0;
  int freqMax=0;
  int done;
  double fadeDuration = 0.100;
  double muteLevelDb = -100.0;
  double fadeInRateDbPerSec = abs(muteLevelDb) / fadeDuration;
  double fadeOutRateDbPerSec = -1.0 * fadeInRateDbPerSec;
  double fadeInGainPerSample = 0.0;
  double fadeOutGainPerSample = 0.0;
  //  int numSamples = G.duration * G.sampleRate;
  double t = 0;
  int sampleCount=0;
  double fadeWindow=0.0;
  int bitsPerSample = 16;
  double fullScale = pow(2,(bitsPerSample-1)) - 1;
  clock_t last,now;
  float usecs=0;
  tms time_info;
  int secs=0;
  
  //!<<<< getopt stuf
  static struct option long_options[] = {
    {"verbose",      no_argument,        0, 'v'},
    {"version",      no_argument,        0, 'V'},
    {"help",         no_argument,        0, 'h'},
    {"help-short",   no_argument,        0, 'H'},
    {"logfile",      required_argument,  0, 'L'},
    {"resetlogfile", no_argument,        0, 'R'},
    {"mof",          no_argument,        0, 'M'},
    {"test",         no_argument,        0, 't'},
    {"slimbus_channel",  required_argument,  0, 'c'},
    {"mi2s2bus_channel", required_argument,  0, 'C'},
    {"frequency",    required_argument,  0, 'f'},
    {"amplitude",    required_argument,  0, 'a'},
    {"duration",     required_argument,  0, 'd'},
    {"output",       required_argument,  0, 'o'},
    {0,0,0,0},
  };


  //!<<<< ................................................................
  //!<<<< defaults/initialization
  //!<<<< ................................................................
  //!<<<< catch CNTL C from tty
  diag_verbose(VVERBOSE, verbosity, "CNTL C to stop\n", 0);
  signal (SIGINT, signal_handler);
  
  diag_verbose(VVERBOSE, verbosity, "Initializing...\n", 0);
  memset ((void *)&G.tid_so, 0, (sizeof(pthread_t)));
  for (i=0; i<SO_NUM_MAX_CHANNELS; i++) {
    G.so_cfg[i].enable=0;
    G.so_cfg[i].channel=0;
    G.so_cfg[i].sweep=SO_SWEEP_TYPE_NONE;
    G.so_cfg[i].frequency=0;     //atoi(SO_FREQUENCY_DEFAULT);
    G.so_cfg[i].frequency_min=0; //atoi(SO_FREQUENCY_DEFAULT);
    G.so_cfg[i].frequency_max=0; //atoi(SO_FREQUENCY_DEFAULT);	
    G.so_cfg[i].amplitude=0.0;   //atof(SO_AMPLITUDE_DEFAULT);	
  }
  G.duration=atoi(SO_DURATION_DEFAULT);
  G.fd=0;
  G.got_sigint=0;
  G.numGenerators=0;
  G.numFreqs=0;
  G.numAmps=0;
  G.test_cmd=0;
  G.freqMax=0;
  G.bus=NONE;
  G.sampleRate=8000;
    fadeInGainPerSample = undb20(fadeInRateDbPerSec / G.sampleRate);
  fadeOutGainPerSample = undb20(fadeOutRateDbPerSec / G.sampleRate);
  strncpy (G.afifo, SO_FIFO_PATH, GP_STR_LEN);
  strncpy (G.out_file, SO_OUTFILE_DEFAULT, GP_STR_LEN);
  chn=-1;
  gen=-1;
  diag_setLogfile ((char *)DIAG_NAME);
  diag_verbose(VVERBOSE, verbosity, "setting logfile to: %s.log\n", DIAG_NAME);
  diag_verbose(VVERBOSE, verbosity, "setting jsonfile to: %s.json\n", DIAG_NAME);
  last=times(&time_info);
  secs=0;
	    
  //!<<<< ................................................................
  //!<<<< process command line args
  //!<<<< ................................................................
  for (optind=0,opterr=0; (c = getopt_long (argc, argv, optstr ,long_options,&option_index)); ) {
    diag_verbose(VERBOSE, verbosity, "optind=%d opterr=%d option_index=%d", optind,opterr, option_index)
    diag_verbose(VERBOSE, verbosity, "c=0x%x %c   argc = %d argv = %s\n", c,c, argc,argv[0])
    switch (c) {

      //!<<<< command specific options >>>>
    case 'o':
      if (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) {
	strncpy (G.out_file, optarg, GP_STR_LEN);
      }
      else {
	strncpy (G.out_file, SO_OUTFILE_DEFAULT, GP_STR_LEN);
      }
      diag_verbose(VVERBOSE, verbosity, "using outfile %s\n", G.out_file);
      break;
    case 't':
      G.test_cmd=!0;
      diag_verbose(VVERBOSE, verbosity, "test command enable %s\n", optarg);
      break;
    case 'c':
      //!<<<< map channel str to index
      diag_verbose(VERBOSE, verbosity, "checking SLIMBUS channel parameter: %s size=%d\n", optarg, sizeof(SLIMBUS_channelTab));
      chn = diag_StrTabStr2Index (SLIMBUS_channelTab, sizeof(SLIMBUS_channelTab), optarg);
      if (-1 == chn) {
	diag_verbose(ALWAYS, verbosity, "SLIMBUS channel parameter not found: %s\n", optarg);
	G.exit_status=0;
	goto so_exit_10;
      }
      //!<<<< do not allow mix and match of bus
      if ((SLIMBUS!=G.bus) && (NONE!=G.bus)) {
	diag_verbose(ALWAYS, verbosity, "can't mix SLIMBUS and MI2S2BUS bus=%d\n", G.bus);
	G.exit_status=0;
	goto so_exit_10;
      }
      diag_verbose(VVERBOSE, verbosity, "using SLIMBUS=%d\n", G.bus);
      G.bus=SLIMBUS;
      gen = chn;

      //!<<<< using 'chn' for subsequent freq and amplitude parameters, default them here
      G.so_cfg[gen].enable=!0;
      G.so_cfg[gen].channel=chn;
      G.so_cfg[gen].sweep=SO_SWEEP_TYPE_NONE;
      G.so_cfg[gen].frequency = atoi(SO_FREQUENCY_DEFAULT);
      G.so_cfg[gen].amplitude = atoi(SO_AMPLITUDE_DEFAULT);
      G.numGenerators++;
      break;
      
    case 'f':
      if (-1 == chn) {
	diag_verbose(ALWAYS, verbosity, "channel parametr needs to be specified first: %s\n", optarg);
	G.exit_status=0;
	goto so_exit_10;
      }
      G.numFreqs++;
      //!<<<< if '-' then linear sweep
      //!<<<< if ':' then logarithmic sweep
      //!<<<< neither, just a single int arg, then single frequency
      if (strstr (optarg,"-")) {
	so_splitFrequency (optarg, "-", &pL, &pR);
	G.so_cfg[gen].sweep=SO_SWEEP_TYPE_LINEAR; // Linear
	G.so_cfg[gen].frequency=0;
	G.so_cfg[gen].frequency_min=atoi(pL);
	G.so_cfg[gen].frequency_max=atoi(pR);
	if (so_ChnParamsChn2MinFreq(chn) > G.so_cfg[gen].frequency_min) {
	  diag_verbose(VVERBOSE, verbosity, "frequency MIN error: chn=%d min=%d entered=%d\n", chn, so_ChnParamsChn2MinFreq(chn), G.so_cfg[gen].frequency_min);
	  G.exit_status=0;
	  goto so_exit_10;		       
	  break;	  
	}
	diag_verbose(VVERBOSE, verbosity, "frequency linear sweep: %d & %d\n", atoi(pL), atoi(pR));
	break;
      } 
      if (strstr (optarg,":")) {
	so_splitFrequency (optarg, ":", &pL, &pR);
	G.so_cfg[gen].sweep=SO_SWEEP_TYPE_LOG; // Log
	G.so_cfg[gen].frequency=0;
	G.so_cfg[gen].frequency_min=atoi(pL);
	G.so_cfg[gen].frequency_max=atoi(pR);
	if (so_ChnParamsChn2MinFreq(chn) > G.so_cfg[gen].frequency_min) {
	  diag_verbose(VVERBOSE, verbosity, "frequency MIN error: chn=%d min=%d entered=%d\n", chn, so_ChnParamsChn2MinFreq(chn), G.so_cfg[gen].frequency_min);
	  G.exit_status=0;
	  goto so_exit_10;		       
	  break;	  
	}
	diag_verbose(VVERBOSE, verbosity, "frequency log sweep: %d & %d\n", atoi(pL), atoi(pR));
	break;
      } 
      G.so_cfg[gen].frequency = (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) ? atoi(optarg) : atoi(SO_FREQUENCY_DEFAULT);
      G.so_cfg[gen].frequency_min = G.so_cfg[gen].frequency;
      G.so_cfg[gen].frequency_max = G.so_cfg[gen].frequency;
      if (so_ChnParamsChn2MinFreq(chn) > G.so_cfg[gen].frequency_min) {
	diag_verbose(VVERBOSE, verbosity, "frequency MIN error: chn=%d min=%d entered=%d\n", chn, so_ChnParamsChn2MinFreq(chn), G.so_cfg[gen].frequency_min);
	G.exit_status=0;
	goto so_exit_10;		       
	break;	  
      }
      diag_verbose(VVERBOSE, verbosity, "frequency: %d\n", G.so_cfg[gen].frequency);
      break;

    case 'a':
      if (-1 == chn) {
	diag_verbose(ALWAYS, verbosity, "channel parametr needs to be specified first: %s\n", optarg);
	G.exit_status=0;
	goto so_exit_10;
      }
      G.numAmps++;
      G.so_cfg[gen].amplitude = (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) ? atof(optarg) : atoi(SO_AMPLITUDE_DEFAULT);
      if (so_ChnParamsChn2MaxDb(chn) < G.so_cfg[gen].amplitude) {
	diag_verbose(VVERBOSE, verbosity, "amplitude max error: chn=%d max=%f entered=%f\n", chn, so_ChnParamsChn2MaxDb(chn), G.so_cfg[gen].amplitude);
	G.exit_status=0;
	goto so_exit_10;		       
	break;	  
      }
      diag_verbose(VVERBOSE, verbosity, "amplitude: %d\n", G.so_cfg[gen].amplitude);
      break;
    case 'd':
      G.duration = (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) ? atoi(optarg) : atoi(SO_DURATION_DEFAULT);
      diag_verbose(VVERBOSE, verbosity, "duration: %d\n", G.duration);
      break;

      //!<<<< common options >>>>
    case 'M':
      G.mofMode.Set(!0);
      diag_verbose(VERBOSE, verbosity, "MOF mode set to %d\n", G.mofMode.Get());
      break;

    case 'V':
      diag_printVersion();
      G.exit_status=!0;
      goto so_exit_10;
      
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
      diag_verbose(VERBOSE, verbosity, "line %d\n\n", __LINE__ );
      diag_help(so_usage);
      G.exit_status=!0;
      goto so_exit_10;

    case 'H':
      diag_verbose(VERBOSE, verbosity, "line %d", 541);
      diag_help(so_usage_short);
      G.exit_status=!0;
      goto so_exit_10;

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
	G.exit_status=0;
	goto so_exit_10;
      }
      //!<<<< valid option, missing arg, apply defaults
      diag_verbose(VVVERBOSE, verbosity, "valid option, missing arg, applying defaults for opt = %c\n", optopt);	
      switch (optopt) {

      case 'd':
	G.duration = atoi(SO_DURATION_DEFAULT);
	diag_verbose(VVERBOSE, verbosity, "no duration arg found, default duration is %s\n", G.duration);
	break;

      case 'a':
	if (-1 == chn) {
	  diag_verbose(VVERBOSE, verbosity, "channel parametr needs to be specified first: %s\n", optarg);
	  G.exit_status=0;
	  goto so_exit_10;
	}
	G.numAmps++;
	G.so_cfg[gen].amplitude = atoi(SO_AMPLITUDE_DEFAULT);
	if (so_ChnParamsChn2MaxDb(chn) < G.so_cfg[gen].amplitude) {
	  diag_verbose(VVERBOSE, verbosity, "amplitude max error: chn=%d max=%f entered=%f\n", chn, so_ChnParamsChn2MaxDb(chn), G.so_cfg[gen].amplitude);
	  G.exit_status=0;
	  goto so_exit_10;		       
	  break;	  
	}
	diag_verbose(VVERBOSE, verbosity, "no amplitude arg specified, default apmplitude is %s:\n", SO_AMPLITUDE_DEFAULT);
	break;
      
      case 'f':
	if (-1 == chn) {
	  diag_verbose(VVERBOSE, verbosity, "channel parametr needs to be specified first: %s\n", optarg);
	  G.exit_status=0;
	  goto so_exit_10;
	}
	G.numFreqs++;
	G.so_cfg[gen].frequency = atoi(SO_FREQUENCY_DEFAULT);
	G.so_cfg[gen].frequency_min = G.so_cfg[gen].frequency;
	G.so_cfg[gen].frequency_max = G.so_cfg[gen].frequency;
	if (so_ChnParamsChn2MinFreq(chn) > G.so_cfg[gen].frequency_min) {
	  diag_verbose(VVERBOSE, verbosity, "frequency MIN error: chn=%d min=%d entered=%d\n", chn, so_ChnParamsChn2MinFreq(chn), G.so_cfg[gen].frequency_min);
	  G.exit_status=0;
	  goto so_exit_10;		       
	  break;	  
	}
	diag_verbose(VVERBOSE, verbosity, "no frequency arg specified, default frequency is %s: %s\n", SO_FREQUENCY_DEFAULT);
	break;

      case 'c':
	chn = diag_StrTabStr2Index (SLIMBUS_channelTab, sizeof(SLIMBUS_channelTab), SO_SLIMBUS_CHANNEL_DEFAULT);
	if (-1 == chn) {
	  diag_verbose(VVERBOSE, verbosity, "channel parameter not found: %s\n", optarg);
	  G.exit_status=0;
	  goto so_exit_10;
	}
	gen = chn;

	//!<<<< using 'chn' for subsequent freq and amplitude parameters, default them here
	G.so_cfg[gen].enable=!0;
	G.so_cfg[gen].channel=chn;
	G.so_cfg[gen].sweep=SO_SWEEP_TYPE_NONE;
	G.so_cfg[gen].frequency = atoi(SO_FREQUENCY_DEFAULT);
	G.so_cfg[gen].amplitude = atoi(SO_AMPLITUDE_DEFAULT);
	G.numGenerators++;
	diag_verbose(VVERBOSE, verbosity, "no channel arg specified, default channel is %s\n", SO_SLIMBUS_CHANNEL_DEFAULT);
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
      so_usage ();
      G.exit_status=!0;
      goto so_exit_10;
      break;
    } 
    if (-1==c) break;
    //init channels here
    if(G.bus == SLIMBUS){
      G.numChannels=SO_NUM_SLIMBUS_CHANNELS;
    }else{
      G.numChannels=SO_NUM_MI2S2BUS_CHANNELS;
    }

  } //!<<<< for
    //!<<<< ................................................................
    //!<<<< ................................................................
  //!<<<< Run the command
  //!<<<< ................................................................
  //!<<<< ................................................................
  diag_printf ("%12s: %8d\n", "bus", G.bus);
  diag_printf ("%12s: %8d\n", "numChannels", G.numChannels);
  diag_printf ("%12s: %8d\n", "duration", G.duration);
  diag_printf ("%12s: %8d\n", "sampleRate", G.sampleRate);
  diag_printf ("%12s %12s %12s %12s %12s %12s %12s %12s %12s\n", "generator", "channel", "channel", "enable", "sweep", "freq", "freq min", "freq max", "amplitude");
  diag_printf ("%12s %12s %12s %12s %12s %12s %12s %12s %12s\n", "------------", "------------", "-----------", "------------", "------------", "-----------", "------------", "-----------", "------------");
  for (gen=0; gen<G.numChannels; gen++) {
    diag_printf ("%12d ", gen);
    if(G.bus == SLIMBUS){
      diag_printf ("%12s ", diag_StrTabIndex2Str (SLIMBUS_channelTab, sizeof(SLIMBUS_channelTab), gen));
    }
    diag_printf ("%12d ", G.so_cfg[gen].channel);
    diag_printf ("%12d ", G.so_cfg[gen].enable);
    diag_printf ("%12d ", G.so_cfg[gen].sweep);
    diag_printf ("%12d ", G.so_cfg[gen].frequency);
    diag_printf ("%12d ", G.so_cfg[gen].frequency_min);
    diag_printf ("%12d ", G.so_cfg[gen].frequency_max);
    diag_printf ("%12.2f\n",  G.so_cfg[gen].amplitude);
  }

  //!<<<< error checking
  if ((G.numGenerators != G.numFreqs) ||
      (G.numGenerators != G.numAmps) ||
      (0 == G.numGenerators) ||
      (G.numChannels < G.numGenerators)) {
    diag_verbose (VVVERBOSE, verbosity, "Arg error numGenerators=%d numFreqs=%d numAmps=%d\n", G.numGenerators, G.numFreqs, G.numAmps);
    G.exit_status=0;
    goto so_exit_10;
  }
  
  //!<<<< ................................................................
  //!<<<< tinymix setup
  //!<<<< ................................................................
  diag_verbose (VVVERBOSE, verbosity, "Amixer config...\n", 0);    
  for (q=tinymix_cmd_tab,done=0; !done; q++) {
    if (NULL==q->pAmixerStr) {
      done=!0;
      break;
    }
    diag_verbose (VVVERBOSE, verbosity, q->pAmixerStr, 0);    
    system((char *)q->pAmixerStr);
  }

  //!<<<< ................................................................
  //!<<<< create FIFO SO --> FIFO --> aplay
  //!<<<< ................................................................
  //!<<<< remove if already exists
  if (file_exists ((char *)G.afifo)) {
    diag_verbose (VVVERBOSE, verbosity, "%s file already exists, removing\n", G.afifo,0);
    unlink (G.afifo);
  }
  diag_verbose (VVVERBOSE, verbosity, "Creating FIFO %s\n", G.afifo,0);
  if (-1==mkfifo (G.afifo, 0777)) {
    diag_verbose (VVVERBOSE, verbosity, "Error creating FIFO %s errno=%d errstr=%s\n", G.afifo,errno,strerror(errno));
    G.exit_status=!0;
    goto so_exit_10;
  }
  diag_verbose (VVVERBOSE, verbosity, "Success creating FIFO %s\n", G.afifo,0);
  //!<<<< don't open file yet, it will block waiting for thread to start


  //!<<<< ................................................................
  //!<<<< create thread
  //!<<<< ................................................................
  //!<<<< start a thread for APLAY command which we will feed from this main program
  //!<<<< create thread context
  diag_verbose (VVVERBOSE, verbosity, "%s\n", "starting SO thread",0);
  G.t_so_args = (pSO_THREAD_ARGS)malloc (sizeof(SO_THREAD_ARGS));
  if (NULL==G.t_so_args) {
    diag_verbose (VVVERBOSE, verbosity, "Error allocating thread args=%p\n", G.t_so_args);
    G.exit_status=!0;
    goto so_exit_10;
  } else {
    //!<<<< command to run
    if (G.test_cmd) {
      snprintf (G.t_so_args->cmd, GP_STR_LEN, SO_CMD_OD, G.afifo);
    } else {
      if(G.bus == SLIMBUS){
	snprintf (G.t_so_args->cmd, GP_STR_LEN, SO_CMD_APLAY,
		  SO_DEVICE,
		  G.afifo,
		  G.numChannels,
		  G.sampleRate, 
		  SO_PCM_FORMAT);
      }else{
	snprintf (G.t_so_args->cmd, GP_STR_LEN, SO_CMD_APLAY,
		  SO_DEVICE_12,
		  G.afifo,
		  G.numChannels,
		  G.sampleRate, 
		  SO_PCM_FORMAT);
      }
    }
    diag_verbose (VVVERBOSE,verbosity,"cmd string is %s\n", G.t_so_args->cmd);
  }

      
  //!<<<< create thread
  diag_verbose (VVVERBOSE, verbosity, "using %s for thread cmd\n", G.t_so_args->cmd);
  if (0==pthread_create(&G.tid_so, NULL, &so_thread, G.t_so_args)) {
    diag_verbose (VVVERBOSE, verbosity, "thread created %d\n", 0);
    //!<<<< give thread a name, use ps -T to view thread names
    pthread_setname_np(G.tid_so, "so_thread");
  } else {
    diag_verbose (VVVERBOSE, verbosity, "error creating thread %d\n", 0);
    diag_verbose (VVVERBOSE, verbosity, "freeing args=%p\n", (void *)G.t_so_args);
    if (G.t_so_args) free (G.t_so_args);
    G.exit_status=!0;
    goto so_exit_10;
  }
  
  
  diag_log_hdr (LogFile.Get(), DIAG_NAME, argc, argv);

  //!<<<< ................................................................
  //!<<<< some preprocessing and range checking
  //!<<<< ................................................................
  for (i=0; i<G.numChannels; i++) {
    chn = G.so_cfg[i].channel;
    pCfg=&G.so_cfg[i];
    
    freqMinLimit = 0.0;  // # DC 
    ampDbMaxLimit = 0.0; // # full-scale
    diag_verbose (VVVERBOSE, verbosity, "channel generator=%d channel=%d \n", i, chn);
    
    //!<<<< freq and amp max
    if (pCfg->frequency_min > pCfg->frequency_max) {
      freqMinLimit=pCfg->frequency_min;
      freqMinLimitCh=pCfg->channel;
      diag_verbose (VVVERBOSE, verbosity, "freq max=%d ch=%d\n", freqMinLimit, freqMinLimitCh);
    }
    if (pCfg->amplitude > ampDbMaxLimit) {
      ampDbMaxLimit=pCfg->amplitude;
      ampDbMaxLimitCh=pCfg->amplitude;
      diag_verbose (VVVERBOSE, verbosity, "amp max=%f ch=%d\n", ampDbMaxLimit, ampDbMaxLimitCh);
    }
    //!<<<< signal amplitude
    if (pCfg->amplitude > ampDbMaxLimit) {
      diag_verbose (VVVERBOSE, verbosity,"Amplitude must be %f dB or less for channel %d\n", ampDbMaxLimit, ampDbMaxLimitCh);
      G.exit_status=0;
      goto so_exit_10;
    }
    ampLinear = undb20(pCfg->amplitude);
    diag_verbose (VVVERBOSE, verbosity,"ampLinear=%f\n", ampLinear);
    
    //!<<<< signal frequency
    if ((pCfg->frequency_min < freqMinLimit) ||
	(pCfg->frequency_max < freqMinLimit)) {
      diag_verbose (VVVERBOSE, verbosity,"Frequencies must be %d Hz or greater for channel %d\n", freqMinLimit, freqMinLimitCh);
      G.exit_status=0;
      goto so_exit_10;
    }
    freqMax = diag_MAX(freqMax, pCfg->frequency_min);
    freqMax = diag_MAX(freqMax, pCfg->frequency_max);
    
  } //!<<<< channel loop
  
  //!<<<< ................................................................
  //!<<<< forever loop....
  //!<<<< ................................................................
  //!<<<< Spin loop witing for thread to open file
  for (i=0;;) {
    if (G.fd) break;
    sleep (1*SECONDS);
    if (++i > 10) {
      diag_verbose (VVERBOSE, verbosity, "timeout waiting for file to open kille thread and exit fd=%p, i=%d\n", G.fd, i);
      G.exit_status=0;
      goto so_exit_10;
    }
  }

  
  fadeWindow = undb20(muteLevelDb);
  t=0.0;
  for (done=0; !done; ) {
    
    //!<<<< check for cntl C
    if (G.got_sigint) {
      diag_verbose (VVERBOSE, verbosity, "got sigint\n", 0);
      G.exit_status=!0;
      goto so_exit_10;
      break;
    }

    //!<<<< ................................................................
    //!<<<< check fifo level and avoid overrun, although I've not seen it happen.
    //!<<<< ................................................................
    int nbytes=0;
    int fifo_size=0;
    static int hold_off=0;
    fifo_size = fcntl (fileno(G.fd), F_GETPIPE_SZ, 0);
    ioctl (fileno(G.fd), FIONREAD, &nbytes);
    if ((hold_off==0) && (SO_HI_THRESH<nbytes)) {
      hold_off=!0;
      diag_verbose (VERBOSE, verbosity, "holdoff==TRUE, file len=%d bytes fifo capacity=%d\n", nbytes, fifo_size);      
    } else if ((hold_off==!0) && (SO_LO_THRESH>nbytes)) {
      hold_off=0;
      diag_verbose (VERBOSE, verbosity, "holdoff==FALSE, file len=%d bytes fifo capacity=%d\n", nbytes, fifo_size);      
    }

    if (hold_off) {
      continue;
    }

    t += 1.0/G.sampleRate;
    if (t>=(float)G.duration) {
      done=!0;
    }
    
    sampleCount++;
    if (sampleCount>G.sampleRate) {
      sampleCount=0;
    }
    
    //!<<<< Fade-in & fade-out
    int fade_calc=0;
    if (t < fadeDuration) {
      fade_calc=0;
      fadeWindow *= fadeInGainPerSample;
      fadeWindow = diag_MIN(fadeWindow, 1.0);
    } else {
      if (t < (G.duration - fadeDuration)) {
	fade_calc=1;
	fadeWindow = 1.0;
      } else {
	fade_calc=2;
	fadeWindow *= fadeOutGainPerSample;
	fadeWindow = diag_MAX(fadeWindow, 0.0);
      }
    }
    
    //!<<<< Interleave channels
    for (i=0; i<G.numChannels; i++) {
      chn = G.so_cfg[i].channel;
      
      {
	double f0 = (double)G.so_cfg[i].frequency_min;
	double f1 = (double)G.so_cfg[i].frequency_max;
	double B = (f0 - f1);
	double T = (double)G.duration;
	double k = pow ((f1/f0),(1/T));
	double lnK = log(k);
	double phase = 0.0;
	double tone = 0.0;
	unsigned short x=0;

	//!<<<< amp
	ampLinear = undb20(G.so_cfg[i].amplitude);
	
	//!<<<< Generate tone
	if (G.so_cfg[i].sweep == SO_SWEEP_TYPE_LOG) {
	  phase = 2*M_PI*(f0*( (((pow(k,t)) - 1))/lnK ));
	}
	if (G.so_cfg[i].sweep == SO_SWEEP_TYPE_LINEAR) {
	  phase = 2*M_PI*((B/T/2)*(pow(t,2)) + f0*t);
	}
	if (G.so_cfg[i].sweep == SO_SWEEP_TYPE_NONE) {
	  phase = 2*M_PI*f0*t;
	}
	
	tone = sin(phase);
	
	//!<<<< Put it all together
	double x1 = tone *  ampLinear * fadeWindow;
	x1 = x1 * fullScale;	
	x=(short int)x1;
	
	{
	  //!<<<< some debug stuff
	  static int first_time=!0;
	  if (first_time) {
	    first_time=0;
	    diag_verbose(VVVERBOSE,verbosity, "%4s %8s %4s %8s %8s %8s %8s %8s %8s %8s %8s %8s %8s %8s %4s %8s\n", "chn", "ampLin", "swp", "fo", "f1", "B",  "t", "k", "lnK", "phase", "tone", "fadecalc", "fadeWin", "full", "x", "x1");
	    diag_verbose(VVVERBOSE,verbosity, "%4s %8s %4s %8s %8s %8s %8s %8s %8s %8s %8s %8s %8s %8s %4s %8s\n", "----", "--------", "----", "--------", "--------", "--------",  "--------", "--------", "--------", "--------", "--------", "--------", "--------", "--------", "----", "--------");	    
	  }
	  if (4==chn) {
	    if (0!=x) {
	      diag_verbose(VVVERBOSE,verbosity, "%4d %8.2g %4d %8.2f %8.2f %8.2f %2.6f %8.2f %8.2f %8.2g %8.2g %8d %8.2g %8.2g %4x %8.2g\n", chn, ampLinear, G.so_cfg[i].sweep, f0,f1,B,t,k,lnK, phase, tone, fade_calc, fadeWindow, fullScale, x, x1);
	    }
	  }
	}

	//!<<<< write to FIFO
	unsigned char b = 0;
	b= (unsigned char)(x & 0xff);
	fwrite( (void *)&b, 1, 1, G.fd );
	if (NULL!=G.fd_out) fwrite( (void *)&b, 1, 1, G.fd_out );	
	b = (unsigned char)(((x >> 8) & 0xff));
	fwrite( (void *)&b, 1, 1, G.fd );
	if (NULL!=G.fd_out) fwrite( (void *)&b, 1, 1, G.fd_out );
	now=times(&time_info);
	usecs=diag_ticks_2_usecs(now-last);
	if (usecs/(float)(K*K) > 1.0) {
	  diag_verbose (VVVERBOSE, verbosity, "elapsed time=%g\n", usecs);
	  fprintf (stdout, ".");
	  fflush (stdout);
	  last=times(&time_info);
	  if (secs++ > 64) {
	    secs=0;
	    NewLine(1);
	  }
	}
      }
    } //!<<<< channel loop
  } //!<<<< while !done

  
  //!<<<< ................................................................
  //!<<<< common exit 
  //!<<<< ................................................................0
  G.exit_status=!0;
  so_exit_10:
    diag_verbose (VVERBOSE, verbosity, "exiting SO, status=%d\n", G.exit_status);
    //!<<<< signal thread to stop,
    //!<<<< wait for it to stop
    diag_verbose (VVERBOSE, verbosity, "stopping threads=%d\n", 0);
    G.so_thread_stop=!0;
    void* ret = NULL;
    if (G.tid_so) {
      pthread_join (G.tid_so,&ret);
      diag_verbose (VVERBOSE, verbosity, "SO thread returned status=%s\n", (char *)ret);
    }

    //!<<<< close files
    if (NULL!=G.fd) fclose (G.fd);
    if (NULL!=G.fd_out) fclose (G.fd_out);
    
    //!<<<< remove files
    if (file_exists ((char *)G.afifo)) {
      diag_verbose (VVVERBOSE, verbosity, "%s file exists, removing\n", G.afifo,0);
      unlink (G.afifo);
    }
    
    fprintf (stdout, "Test Complete 2\n");
    fflush (stdout);


    exit ((G.exit_status) ? EXIT_SUCCESS : EXIT_FAILURE);
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
float so_ChnParamsChn2MaxDb (int chn) {
  size_t i=0;
  pSO_CHN_PARAMS_ENT p=NULL;
  for (i=0,p=paramsTab; i<(sizeof(paramsTab)/sizeof(SO_CHN_PARAMS_ENT)); i++,p++) {
    if (chn==p->chn) {
      return (p->amp_max);
    }
  }
  return (0.0);
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int so_ChnParamsChn2MinFreq (int chn) {
  size_t i=0;
  pSO_CHN_PARAMS_ENT p=NULL;
  for (i=0,p=paramsTab; i<(sizeof(paramsTab)/sizeof(SO_CHN_PARAMS_ENT)); i++,p++) {
    if (chn==p->chn) {
      return (p->freq_min);
    }
  }
  return (0);
}
	
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void so_splitFrequency (char *str, const char *delim, char **pL, char **pR) {
  *pL = strtok (str,delim);
  *pR = strtok (NULL,delim);
  return;
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
double undb20(double gainDb) {
  double logTenOverTwenty=0.0;
  double gainLinear = 0.0;
  logTenOverTwenty = 0.11512925464;
  logTenOverTwenty = log(10.0)/20.0;
  gainLinear = exp(gainDb * logTenOverTwenty);
  return gainLinear;
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
double db20(double gainLinear) {
  double twentyOverLogTen = 0.0;
  double gainDb=0.0;
  twentyOverLogTen = 8.685889638065035;
  twentyOverLogTen = 20.0/log(10.0);
  gainDb = twentyOverLogTen * log(abs(gainLinear));
  return gainDb;
}

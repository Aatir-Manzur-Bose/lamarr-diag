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
#include <semaphore.h>
#include <getopt.h>
#include <ncurses.h>
#include <time.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "diag_common.h"
#include "jWrite.h"
#include "wifi.h"


#ifdef MOF_INT
#pragma message("==> MOF_INT defined")
#endif

//!<<<< globals
extern FLAG Verbosity;
extern LOGFILE LogFile;
extern LOGFILE JsonFile;
extern STR_TAB_ENT diag_mofRespStrTab[];
#define verbosity Verbosity.Get()
#define pLogFile LogFile.Get()
#define pJsonFile JsonFile.Get()
extern STR_TAB_ENT diag_mofRespStrTab[];
struct globals {
  MOF_BUFFER mofBuffer;
  FLAG mofMode;
  FLAG flags_C;
  FLAG flags_S;
  FLAG flags_T;
  FLAG flags_R;
  FLAG flags_B;
  FLAG flags_D;
  FLAG flags_J;
  char pingbackaddr [GP_STR_LEN];
  char interface [GP_STR_LEN];
  int iterations;
  STATS stats;
  char ssid [GP_STR_LEN];
  char passphrase [GP_STR_LEN];
  int count;
  int length;
  float delay_thresh;
  float jitter_thresh;
  FLAG status;
  FLAG ssid_specified;
  FLAG pass_specified;
  ETH_STATS b_stats, e_stats, r_stats;;
} globals, *p_globals=&globals;
#define G (*p_globals)  

//!<<<< error strings
STR_TAB_ENT wifiErrorStrTab[] = {
  {PASS,DIAG_PASS},
  {FAIL,DIAG_FAIL},
  //!<<<< add error strings here when limits and test pass/fail is applicable
  {NOK_PING_DROPS,      "packet throughput less than 100 percent"},
  {NOK_OPERSTATUS,      "oper status is down"},
  {NOK_CARRIER,         "link state is 0"},
  {NOK_TXPACKETCOUNT,   "tx packet count error"},
  {NOK_RXPACKETCOUNT,   "rx packet count error"},
  {NOK_RXBYTECOUNT,     "rx byte count error"},
  {NOK_DELAYTHRESH,     "delay threshold exceeded error"},
  {NOK_JITTERTHRESH,    "jitter threshold exceeded error"},
};

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< wifi_scan()()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void wifi_scan (void) {
    char gp_str[GP_STR_LEN];
    if (0==G.interface[0]) {
      diag_printf("%s\n","error: interface not specified, use -I option");
      return;
    }
    snprintf (gp_str, GP_STR_LEN, WIFI_SCAN, G.interface);
    diag_system (0,0,gp_str);  
    return;
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< usage()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void wifi_usage_short (void) {
  diag_usage_short (DIAG_NAME, "Wifi Authentication & Connectivity Test");   
}
void wifi_usage (void) {
  fprintf (stdout, "usage:  %s <-Vhvd:s:p:i:n:STPL:R>\n", DIAG_NAME); 
  fprintf (stdout, " -h,--help             Print this message\n"); 
  fprintf (stdout, " -H,--help-short       Print test summary message\n"); 
  fprintf (stdout, " -V,--version          show build version\n"); 
  fprintf (stdout, " -v[vv],--verbose      Verbose level\n"); 
  fprintf (stdout, " -s,--ssid             The SSID of the network to connect to, default %s\n", WIFI_DEFAULT_SSID); 
  fprintf (stdout, " -p,--passphrase       The PASSPHRASE to use, skip for open network \n"); 
  fprintf (stdout, " -d,--iterations       Iterations, default %sd\n", WIFI_DEFAULT_ITERATIONS);
  fprintf (stdout, " -c,--count            The number of pings per interation, default %s \n", WIFI_DEFAULT_COUNT); 
  fprintf (stdout, " -i,--ip-address       The IP address to ping from the DUT, default %s\n", WIFI_DEFAULT_PINGBACKADDR);
  fprintf (stdout, " -I,--interface        Interface name, default %s\n", WIFI_DEFAULT_INTERFACE); 
  fprintf (stdout, " -l,--length           The length of each ping message, default %s, 0=random\n", WIFI_DEFAULT_LENGTH);
  fprintf (stdout, " -a,--scan             Scan (i.e. wifi -Iwlan0 -a) \n");
  fprintf (stdout, " -D,--delay_thresh     Max delay error threshold (in milliseconds), default %s mS\n", WIFI_DEFAULT_DELAY_THRESH);
  fprintf (stdout, " -J,--jitter_thresh    Max jitter error threshold (in milliseconds), default %s mS\n", WIFI_DEFAULT_JITTER_THRESH); 
  fprintf (stdout, " -L,--logfile          Output to logfile in /opt/Bose/tmp/, default %s.log\n", DIAG_NAME); 
  fprintf (stdout, " -R,--resetlogfile     Reset logfile\n"); 
  fprintf (stdout, " -S,--do-setup-skip    Skip wifi setup, NOT default\n"); 
  fprintf (stdout, " -P,--do-ping-skip     Skip ping, NOT default\n"); 
  fprintf (stdout, " -T,--do-teardown-skip Skip wifi teardown, NOT default\n");
  fprintf (stdout, " -F,--flag             Flags: {C|S} (i.e. -FCS)\n");
  fprintf (stdout, "                         T=error on TX packet count!=packets sent, defualt ON\n");
  fprintf (stdout, "                         R=error on RX packet count!=packets sent, default ON\n");
  fprintf (stdout, "                         C=error on No Carrier\n");
  fprintf (stdout, "                         S=error on Status DOWN\n");
  fprintf (stdout, "                         B=error on rx bytes != tx bytes\n");
  fprintf (stdout, "                         D=error on delay greater than max (see -D)\n");
  fprintf (stdout, "                         J=error on jitter greater than max (see -J)\n");

  fprintf (stdout, "Example: Default open network, random length packats: %s -i8.8.8.8 -l0\n", DIAG_NAME);
  fprintf (stdout, "Example: ssid & pass:  %s -sxxxxxxxxx -pxxxxxxxxxx -Iwlan0 -iwww.bose.com\n", DIAG_NAME);
  fprintf (stdout, "Example: open network: %s -sxxxxxxxxx iIwlan0 -iwww.bose.com\n", DIAG_NAME);
  fprintf (stdout, "Example: localhost, 10 iterations:      %s -d10 -sxxxxxxxxx iIwlan0 -ilocalhost\n", DIAG_NAME); 
  fprintf (stdout, "Example: setup wifi connection only:    %s -T -P\n", DIAG_NAME); 
  fprintf (stdout, "Example: teardown wifi connection only: %s -S -P\n", DIAG_NAME); 
  NewLine ();
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< ping thread
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void *ping_thread (void *args) {
  int i;
  int j;
  int first_time=1;
  float max,min,avg,jitter;
  int tx,rx,loss,time;
  int rx_bytes=0;
  pPING_THREAD_ARGS p_args = (pPING_THREAD_ARGS)args;
  char *p_buffer=0;
  jWriteControl jwc, *p_jwc=&jwc;
  //  long long int last_in_msgs=0,last_in_errors=0,last_in_dst_unreach=0,last_out_msgs=0,last_out_errors=0,last_out_dst_unreach=0;
  unsigned int buflen=GP_STR_LEN*4;

  //!<<<< -M MOF mode
  if (G.mofMode.Get()) {
    p_buffer = (char *)malloc (buflen);
    if (NULL==p_buffer) {
      diag_verbose(VERBOSE,verbosity, "error allocating memory for json buffer buflen=%d\n", buflen);    
      exit (EXIT_FAILURE);
    }
    G.mofBuffer.Reset ();
    jwOpen( &jwc, p_buffer, buflen, JW_OBJECT, JW_PRETTY );
    jwObj_array(&jwc, (char *)"intervals");
    diag_verbose(VVERBOSE,verbosity, "allocated memory for json buffer buflen=%d\n", buflen);    
  }
      
  diag_verbose(VVERBOSE,verbosity, "starting ping thread secs=%d cmd=%s\n", p_args->secs, p_args->cmd);
  for (i=0; i<G.iterations; i++) {
    for (j=0; j<G.count; j++) {
      
      diag_verbose(VVVERBOSE,verbosity, "doing ping time=%d cmd=%s\n", p_args->secs, p_args->cmd);
      diag_system (0,0, p_args->cmd);
      sleep (2*SECONDS);
      iperf_parse_ping_output (&max, &min, &avg, &jitter, &tx, &rx, &loss, &time, &rx_bytes);
      diag_verbose(VVVERBOSE,verbosity, "next ping int=%d\n", i);
      
      G.stats.ping_samples=i;
      //!<<<< time
      if ((max>G.stats.max_time) || (G.stats.max_time==0)) G.stats.max_time=max;
      if ((min<G.stats.min_time) || (G.stats.min_time==0)) G.stats.min_time=min;
      G.stats.sum_time+=avg;
      if (G.stats.ping_samples) G.stats.avg_time = G.stats.sum_time/G.stats.ping_samples;
      //!<<<< jitter
      if ((jitter>G.stats.max_jitter) || (G.stats.max_jitter==0)) G.stats.max_jitter=jitter;
      if ((jitter<G.stats.min_jitter) || (G.stats.min_jitter==0)) G.stats.min_jitter=jitter;
      G.stats.sum_jitter+=jitter;
      if (G.stats.ping_samples) G.stats.avg_jitter = G.stats.sum_jitter/G.stats.ping_samples;
      sleep (p_args->secs);
      
      G.stats.tx=tx;
      G.stats.rx=rx;
      G.stats.loss=loss;
      G.stats.time=time;
      
      diag_verbose(VVVERBOSE,verbosity,"stats: %d %d %d %d\n", G.stats.tx,G.stats.rx,G.stats.loss,G.stats.time);
    }
    
    //!<<<< do output
    out_thread (first_time,i,p_jwc,p_buffer,buflen,0, rx_bytes, G.length);
    if (first_time) first_time=0;
  }
  
  if (args) free (args);

  //!<<<< for -M MOF mode, finish up
  if (G.mofMode.Get()) {
    diag_verbose(VVERBOSE,verbosity, "ending %d\n", 0);
    jwEnd(&jwc);
    jwClose(&jwc);
    if (G.mofMode.Get()) {
      G.mofBuffer.Write (jwc.buffer, (jwc.bufp-jwc.buffer));
      G.mofBuffer.Print();
      G.mofBuffer.Reset();
    }
    jWriteFile (&jwc, pJsonFile);
    if (p_buffer) free (p_buffer);
  }

  return (0);
}
  
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< out thread
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void *out_thread (int first_time, int interval, jWriteControl *p_jwc, char *p_buffer, int buflen, int len, int rx_bytes, int tx_bytes) {


  char gp_str[GP_STR_LEN];
  int interval_status=0;
  const char *p=NULL;

  //!<<<< get oper status of interface >>>>
  if (0!=G.interface[0]) {
    snprintf (gp_str, GP_STR_LEN, "%s/%s/operstate",  "/sys/class/net", G.interface);
    diag_verbose (VVVERBOSE, verbosity, "looking in %s for oper status for %s\n", gp_str, G.interface);
    if (diag_ScrapeFileLine (gp_str, "%s", &G.stats.operstatus)) {
    }
    snprintf (gp_str, GP_STR_LEN, "%s/%s/carrier",  "/sys/class/net", G.interface);
    diag_verbose (VVVERBOSE, verbosity, "looking in %s for carrier status for %s\n", gp_str, G.interface);
    if (diag_ScrapeFileLine (gp_str, "%d", &G.stats.carrier)) {
    }
  }

  //!<<< snapshot signal level >>>>
  iperf_parse_iwlink_output (G.interface, G.stats.ssid, &G.stats.freq, &G.stats.signal);
      
  //!<<<< evaluate error situation and flags
  G.status.Set(PASS);
  if (G.flags_C.Get()) {
    if (!G.stats.carrier) {
      G.status.Set(FAIL);
      p=diag_StrTabIndex2Str(wifiErrorStrTab,sizeof(wifiErrorStrTab),NOK_CARRIER);
      diag_verbose (VERBOSE, verbosity, "failing on %s reason=%s\n", G.interface,p);
    }
  }
  if (G.flags_C.Get()) {
    if (0!=strcmp("up",G.stats.operstatus)) {
      G.status.Set(FAIL);
      p=diag_StrTabIndex2Str(wifiErrorStrTab,sizeof(wifiErrorStrTab),NOK_OPERSTATUS);
      diag_verbose (VERBOSE, verbosity, "failing on %s reason=%s\n", G.interface,p);
    }
  }
  if (G.flags_T.Get()) {
    if (G.stats.tx != G.count) {
      G.status.Set(FAIL);
      p=diag_StrTabIndex2Str(wifiErrorStrTab,sizeof(wifiErrorStrTab),NOK_TXPACKETCOUNT);
      diag_verbose (VERBOSE, verbosity, "failing on %s reason=%s\n", G.interface,p);
    }
  }
  if (G.flags_R.Get()) {
    if (G.stats.rx != G.count) {
      G.status.Set(FAIL);
      p=diag_StrTabIndex2Str(wifiErrorStrTab,sizeof(wifiErrorStrTab),NOK_RXPACKETCOUNT);
      diag_verbose (VERBOSE, verbosity, "failing on %s reason=%s\n", G.interface,p);
    }
  }
  if (G.flags_B.Get()) {
    if (rx_bytes != tx_bytes) {
      G.status.Set(FAIL);
      //!<<<< Note: Not all hosts return a ping response with the same enumber of bytes in the request, google.com for example
      p=diag_StrTabIndex2Str(wifiErrorStrTab,sizeof(wifiErrorStrTab),NOK_RXBYTECOUNT);
      diag_verbose (VERBOSE, verbosity, "failing on %s reason=%s rx bytes=%d\n", G.interface, p, rx_bytes);
    }
  }

  if (G.flags_D.Get()) {
    if (G.stats.avg_time > G.delay_thresh) {
      G.status.Set(FAIL);
      p=diag_StrTabIndex2Str(wifiErrorStrTab,sizeof(wifiErrorStrTab),NOK_DELAYTHRESH);
      diag_verbose (VERBOSE, verbosity, "failing on %s reason=%s delay=%5.2f exceeds threshold=%5.2f\n", G.interface, p, G.stats.avg_time, G.delay_thresh);
    }
  }
  if (G.flags_J.Get() && (G.count > 1)) {
    if (G.stats.avg_jitter > G.jitter_thresh) {
      G.status.Set(FAIL);
      p=diag_StrTabIndex2Str(wifiErrorStrTab,sizeof(wifiErrorStrTab),NOK_JITTERTHRESH);
      diag_verbose (VERBOSE, verbosity, "failing on %s reason=%s, jitter=%5.2f exceeds threshold=%5.2f\n", G.interface, p, G.stats.avg_jitter, G.jitter_thresh);
    }
  }

  interval_status = (PASS==G.status.Get()) ? !0 : 0;


  //!<<<< overal PASS FAIL status
  if (G.mofMode.Get()) {
    if (interval_status) {
      p=diag_StrTabIndex2Str(wifiErrorStrTab,sizeof(wifiErrorStrTab),PASS);
      G.mofBuffer.SetErrorCode (PASS);
      G.mofBuffer.SetErrorDetails (p);
    } else {
      G.mofBuffer.SetErrorCode (FAIL);
      G.mofBuffer.SetErrorDetails (p);
    }
  }
  
  if (!G.mofMode.Get()) { 
    if (first_time) {
      
      //!<<<< output >>>>
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%6s ",	    "");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s ",	    "");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s ",	    "");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s ",	    "");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%18s ",	    "");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	    "");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	    "");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%26s ",    "delay(mS)");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s ",    "");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%66s\n",   "");
      
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%6s ",	"int");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s ",	"SSID");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s ",	"freq(MHZ)");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s ",	"signal(dBm)");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%18s ",	"interface");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	"status");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	"carrier");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%26s ",    "min/avg/max");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s",     "jitter(mS)");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",     "ICMP Rx");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",     "ICMP Tx");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",     "Bytes Rx");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",     "Bytes Tx");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",    "Status");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s\n",   "Failure Reason");
      
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%6s ",	"------");    
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s ",	"------------");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s ",	"------------");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s ",	"------------");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s ",	"------------------");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	"--------");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	"--------");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%26s ",    "------------------------");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%12s ",    "------------");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",    "--------");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",    "--------");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",    "--------");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",    "--------");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%8s ",	"--------");
      diag_log_ncurses((first_time) ? pLogFile : NULL,"%24s\n", "------------------------");
    }
    
    //!<<<< log entry
    diag_log_ncurses(pLogFile,"%6d ",	     interval );
    diag_log_ncurses(pLogFile,"%12s ",	     diag_StrTrunc(G.stats.ssid,12));
    diag_log_ncurses(pLogFile,"%12.2f ",     G.stats.freq );
    diag_log_ncurses(pLogFile,"%12.2f ",     G.stats.signal );
    diag_log_ncurses(pLogFile,"%18s ",	     diag_StrTrunc(G.pingbackaddr,18));
    diag_log_ncurses(pLogFile,"%8s ",	     G.stats.operstatus );
    diag_log_ncurses(pLogFile,"%8d ",	     G.stats.carrier );
    diag_log_ncurses(pLogFile,"%8.2f/%8.2f/%8.2f ",  G.stats.min_time,G.stats.avg_time,G.stats.max_time);
    diag_log_ncurses(pLogFile,"%12.2f ",  G.stats.avg_jitter );
    diag_log_ncurses(pLogFile,"%8d ",              G.stats.rx);
    diag_log_ncurses(pLogFile,"%8d ",              G.stats.tx);
    diag_log_ncurses(pLogFile,"%8d ",              rx_bytes);
    diag_log_ncurses(pLogFile,"%8d ",              tx_bytes);
    diag_log_ncurses(pLogFile,"%8s ",             (interval_status) ? "PASS" : "FAIL");
    diag_log_ncurses(pLogFile,"%24s\n",           (interval_status) ? "" : p);
    diag_log_ncurses_refresh ();
  }
  
  //!<<<< ----------------------------------------------------------------
  //!<<<< JSON output lines (array of intervals)
  //!<<<< ----------------------------------------------------------------
  if (G.mofMode.Get()) {
    if (first_time) {
      diag_verbose(VVERBOSE,verbosity, "MOF mode first time resetting %d\n", 0);
      G.mofBuffer.Reset ();
      jwOpen( p_jwc, p_buffer, buflen, JW_OBJECT, JW_PRETTY );
      jwObj_array(p_jwc, (char *)"intervals");
    }
    
    jwArr_object(p_jwc);
    jwObj_int( p_jwc, (char *)"interval", interval);
    
    //!<<<< standard MOF status, enum and detail fields
    jwObj_string( p_jwc, (char *)diag_StrTabIndex2Str(diag_mofRespStrTab,sizeof(STR_TAB_ENT ),MOF_RESP_STATUS), (char *)G.mofBuffer.GetStatus(interval_status));
    jwObj_int( p_jwc, (char *)diag_StrTabIndex2Str(diag_mofRespStrTab,sizeof(STR_TAB_ENT ),MOF_RESP_CODE), G.mofBuffer.GetErrorCode());
    jwObj_string( p_jwc, (char *)diag_StrTabIndex2Str(diag_mofRespStrTab,sizeof(STR_TAB_ENT ),MOF_RESP_DETAILS),  G.mofBuffer.GetErrorDetails());
    
    
    jwObj_string( p_jwc, (char *)"adapter", G.stats.ssid );
    jwObj_double(p_jwc, (char *)"freq", G.stats.freq );
    jwObj_double(p_jwc, (char *)"signal", G.stats.signal );
    jwObj_string(p_jwc, (char *)"operStatus", G.stats.operstatus );
    jwObj_int(p_jwc, (char *)"carrier", G.stats.carrier );
    jwObj_double(p_jwc, (char *)"minDelay", G.stats.min_time);
    jwObj_double(p_jwc, (char *)"avgDelay", G.stats.avg_time);
    jwObj_double(p_jwc, (char *)"maxDelay", G.stats.max_time);
    jwObj_double(p_jwc, (char *)"minJitter", G.stats.min_jitter);
    jwObj_double(p_jwc, (char *)"avgJitter", G.stats.avg_jitter);
    jwObj_double(p_jwc, (char *)"maxJitter", G.stats.max_jitter);
    jwObj_int(p_jwc, (char *)"IcmpInMsg", G.stats.rx);
    jwObj_int(p_jwc, (char *)"IcmpOutMsg", G.stats.tx);
    jwObj_int(p_jwc, (char *)"rxBytes", rx_bytes);
    jwObj_int(p_jwc, (char *)"txBytes", tx_bytes);
    diag_verbose(VVERBOSE,verbosity, "ending %d\n", 0);
    jwEnd(p_jwc);
    
    if (G.mofMode.Get()) {
      G.mofBuffer.Write (p_jwc->buffer, (p_jwc->bufp-p_jwc->buffer));
      G.mofBuffer.Print();
      G.mofBuffer.Reset();
    }
    
    jWriteFile (p_jwc, pJsonFile);
  }
  

  diag_log_ncurses_done ();
  return (0);
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< main()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int main(int argc, char *argv[]) {
  int c;
  int do_setup_skip=0;
  int do_ping_skip=0;
  int do_teardown_skip=0;
  int option_index=0;
  char gp_str[GP_STR_LEN];
  char optstr[GP_STR_LEN] = {DIAG_COMMON_OPTS "D:J:c:ad:s:p:i:l:STPI:F:"};
  static struct option long_options[] = {
    {"verbose",     no_argument, 0, 'v'},
    {"version",     no_argument, 0, 'V'},
    {"help",        no_argument, 0, 'h'},
    {"help-short",  no_argument, 0, 'H'},
    {"scan",        no_argument, 0, 'a'},
    {"mof",         no_argument,       0, 'M'},
    {"iterations",  required_argument, 0, 'd'},
    {"ssid",        required_argument, 0, 's'},
    {"passphrase",  required_argument, 0, 'p'},
    {"interface",   required_argument, 0, 'I'},
    {"ip-address",  required_argument, 0, 'i'},
    {"flags",       required_argument, 0, 'F'},
    {"count",       required_argument, 0, 'c'},
    {"length",      required_argument, 0, 'l'},
    {"delay_thresh",  required_argument, 0, 'D'},
    {"jitter_thresh", required_argument, 0, 'J'},
    {"do-setup",    no_argument, 0, 'S'},
    {"do-ping",     no_argument, 0, 'P'},
    {"do-teardown", no_argument, 0, 'T'},
    {"logfile",     required_argument, 0, 'L'},
    {"resetlogfile",     no_argument, 0, 'R'},
    {0,0,0,0},
  };
  char *p=0;
  
  pLogFile[0]=0;

  //!<<<< defaults
  diag_setLogfile ((char *)DIAG_NAME);
  diag_verbose(VVERBOSE, verbosity, "setting logfile to: %s.log\n", DIAG_NAME);
  diag_verbose(VVERBOSE, verbosity, "setting jsonfile to: %s.json\n", DIAG_NAME);
  
  strncpy (G.interface, WIFI_DEFAULT_INTERFACE, GP_STR_LEN);
  strncpy (G.pingbackaddr, WIFI_DEFAULT_PINGBACKADDR, GP_STR_LEN);
  strncpy (G.ssid, WIFI_DEFAULT_SSID, GP_STR_LEN);
  strncpy (G.passphrase, WIFI_PASSPHRASE_DEFAULT, GP_STR_LEN);
  G.count = atoi(WIFI_DEFAULT_COUNT);
  G.length = atoi(WIFI_DEFAULT_LENGTH);
  G.delay_thresh = atof(WIFI_DEFAULT_DELAY_THRESH);
  G.jitter_thresh = atof(WIFI_DEFAULT_JITTER_THRESH);
  G.iterations = atoi(WIFI_DEFAULT_ITERATIONS);
  memset ((void *)&G.stats, 0, (sizeof(STATS)));
  G.ssid_specified.Set(0);
  G.pass_specified.Set(0);
  G.flags_T.Set(1);
  G.flags_R.Set(1);
  G.flags_C.Set(0);
  G.flags_S.Set(0);
  G.flags_D.Set(0);
  G.flags_J.Set(0);
  
  //!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
  //!<<<< process command line args
  //!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
  for (optind=0,opterr=0; (c = getopt_long (argc, argv, optstr,long_options,&option_index)); ) {
    switch (c) {
      //!<<<< common options >>>>
    case 'V':
      diag_printVersion();
      exit (EXIT_SUCCESS);
      break;
    case 'M':
      G.mofMode.Set(!0);
      diag_verbose(VERBOSE, verbosity, "MOF mode set to %d\n", G.mofMode.Get());
      break;
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
    case 'F':
      if (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) {
	if (strstr(gp_str, "T")) G.flags_T.Set(1);
 	if (strstr(gp_str, "R")) G.flags_R.Set(1);
	if (strstr(gp_str, "C")) G.flags_C.Set(1);
	if (strstr(gp_str, "S")) G.flags_S.Set(1);
	if (strstr(gp_str, "B")) G.flags_B.Set(1);
	if (strstr(gp_str, "D")) G.flags_D.Set(1);
	if (strstr(gp_str, "J")) G.flags_J.Set(1);
	diag_verbose(VVERBOSE,verbosity, "flag set =%s\n", optarg);
      }
      break;
    case 'R':
      diag_resetLogFile ();
      break;
    case 'h':
      diag_help(wifi_usage);
      exit (EXIT_SUCCESS);
    case 'H':
      diag_help(wifi_usage_short);
      exit (EXIT_SUCCESS);
    case 'v': 
      Verbosity.Inc();
      if (diag_verbosity(VVERBOSE,verbosity)) diag_debug ("verbosity: %d\n", verbosity);
      break;

      //!<<<< command specific options
    case 'I':
      strncpy (G.interface, (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) ? optarg : WIFI_DEFAULT_INTERFACE, GP_STR_LEN);
      if (diag_verbosity (VVERBOSE,verbosity)) diag_debug ("interface: %s\n", G.interface);
      break;
      //!<<<< scan
    case 'a':
      wifi_scan ();
      exit (EXIT_SUCCESS);
      break;
      //!<<<< iterations
    case 'd': 
      G.iterations = (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) ? atoi(optarg) : atoi(WIFI_DEFAULT_ITERATIONS);
      diag_verbose(VVERBOSE,verbosity, "iterations: %d\n", G.iterations);
      break;
      // !<<<< ip address
    case 'i':
      strncpy (G.pingbackaddr, (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) ? optarg : WIFI_DEFAULT_PINGBACKADDR, GP_STR_LEN);
      if (diag_verbosity (VVERBOSE,verbosity)) diag_debug ("pingbackaddr: %s\n", G.pingbackaddr);
      break;
      // !<<<< SSID
    case 's':
      G.ssid_specified.Set(!0);
      strncpy (G.ssid, (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) ? optarg : WIFI_DEFAULT_SSID, GP_STR_LEN);
      if (diag_verbosity(VVERBOSE,verbosity)) diag_debug ("ssid: %s\n", G.ssid);
      break;
      // !<<<< passphrase
    case 'p':
      G.pass_specified.Set(!0);
      strncpy (G.passphrase, (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) ? optarg : WIFI_PASSPHRASE_DEFAULT, GP_STR_LEN);
      if (diag_verbosity(VVERBOSE,verbosity)) diag_debug ("passphrase: %s\n", G.passphrase);
      break;

      //!<<<< number of pings
    case 'c': 
      G.count = (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) ? atoi(optarg) : atoi(WIFI_DEFAULT_COUNT);
      if (diag_verbosity(VVERBOSE,verbosity)) diag_debug ("count: %d\n", G.count);
      break;

    case 'D': 
      G.delay_thresh = (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) ? atof(optarg) : atof(WIFI_DEFAULT_DELAY_THRESH);
      if (diag_verbosity(VVERBOSE,verbosity)) diag_debug ("delay threshold: %5.2f\n", G.delay_thresh);
      break;

    case 'J': 
      G.jitter_thresh = (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) ? atof(optarg) : atof(WIFI_DEFAULT_JITTER_THRESH);
      if (diag_verbosity(VVERBOSE,verbosity)) diag_debug ("jitter threshold: %5.2f\n", G.jitter_thresh);
      break;
      
      //!<<<< length of each ping
    case 'l': 
      G.length = (diag_getopt_OptArgExists(argc,argv,c, gp_str,&optind)) ? atoi(optarg) : atoi(WIFI_DEFAULT_LENGTH);
      if (diag_verbosity(VVERBOSE,verbosity)) diag_debug ("length: %d\n", G.length);
      break;
    case 'S': 
      do_setup_skip=!0;
      if (diag_verbosity(VVERBOSE,verbosity)) diag_debug ("do_setup=: %d\n", do_setup_skip);
      break;
    case 'P': 
      do_ping_skip=!0;
      if (diag_verbosity(VVERBOSE,verbosity)) diag_debug ("do_ping=: %d\n", do_ping_skip);
      break;
    case 'T': 
      do_teardown_skip=!0;
      if (diag_verbosity(VVERBOSE,verbosity)) diag_debug ("do_teardown=: %d\n", do_teardown_skip);
      break;
    case '?':
      diag_printf ("getopt case ? %d\n", 0);
      if (NULL==strchr(optstr, optopt)) {
	diag_verbose(VVVERBOSE,verbosity, "optind = %d\n", optind);	
	diag_verbose(VVVERBOSE,verbosity, "optarg = %s\n", optarg);	
	diag_verbose(VVVERBOSE,verbosity, "optopt = %c\n", optopt);	
	diag_printf("invalid option c=%c argv=%s, optstr=%s\n", c, argv[optind-1], optstr);	
	exit (EXIT_FAILURE);
      }
      //!<<<< valid option, missing arg, apply defaults
      diag_verbose(VVVERBOSE,verbosity, "valid option, missing arg, applying defaults for opt = %c\n", optopt);	
      switch (optopt) {
      case 'L':
	diag_setLogfile ((char *)DIAG_NAME);
	diag_verbose(VVERBOSE, verbosity, "setting logfile to: %s.log\n", DIAG_NAME);
	diag_verbose(VVERBOSE, verbosity, "setting jsonfile to: %s.json\n", DIAG_NAME);	
	break;
      case 'I': 
	strncpy (G.interface, WIFI_DEFAULT_INTERFACE, GP_STR_LEN); 
	break;
      case 'd': 
	G.iterations = atoi(WIFI_DEFAULT_ITERATIONS);
	break;
      case 'i': 
	strncpy (G.pingbackaddr, WIFI_DEFAULT_PINGBACKADDR, GP_STR_LEN);
	break;
      case 's': 
	strncpy (G.ssid, WIFI_DEFAULT_SSID, GP_STR_LEN);
	break;
      case 'p': 
	strncpy (G.passphrase, WIFI_PASSPHRASE_DEFAULT, GP_STR_LEN);
	break;
      case 'c': 
	G.count = atoi(WIFI_DEFAULT_COUNT);
	break;
      case 'l': 
	G.length = atoi(WIFI_DEFAULT_LENGTH);
	break;
      case 'D': 
	G.delay_thresh = atof(WIFI_DEFAULT_DELAY_THRESH);
	break;
      case 'J': 
	G.jitter_thresh = atof(WIFI_DEFAULT_JITTER_THRESH);
	break;
      }
      break;
    case ':':
      diag_printf ("missing option arg option_index=%d argv[option_index]=%s\n", option_index, argv[option_index+1]);
      wifi_usage ();
      exit (EXIT_SUCCESS);
      break;
    }
    if (-1==c) break;
  }

  diag_verbose (VVERBOSE, verbosity, "testing %s\n", MODULE_NAME);




  //!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
  //!<<<< run the test
  //!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
  diag_log_hdr (pLogFile, DIAG_NAME, argc, argv);
  snprintf (gp_str,GP_STR_LEN,WIFI_UNLINK_STR1, RDW_TMP_DIR, WIFI_PING_FILE);
  diag_system (VVERBOSE,verbosity, gp_str);

  //!<<<< Do Setup >>>>
  if (!do_setup_skip) {
    diag_verbose (VVERBOSE, verbosity, "Skip setup flag not set\n",0);
    //!<<<< not skipping wifi setup...
    //!<<<< kill wpa_supplicant and dhcpcd if running
    diag_system (VVERBOSE,verbosity, WIFI_KILLALL_STR1);
    diag_system (VVERBOSE,verbosity, WIFI_KILLALL_STR2);  
    sleep (1*SECONDS);
    diag_verbose (VVERBOSE, verbosity, "killed processes\n",0);
    //!<<<< setup wifi parameters
    //!<<<< start wpa_supplicant
    if ((!G.ssid_specified.Get()) || (!G.pass_specified.Get())) {
      diag_verbose(VVERBOSE,verbosity, "SSID and or passphrase not specified, using default OPEN credentials for %s ssid=%s\n", G.interface, G.ssid);
      snprintf (gp_str,GP_STR_LEN, WIFI_OPENSETUP_STR1);
      diag_system (VVERBOSE,verbosity, gp_str);
      diag_verbose(VVERBOSE,verbosity, "copying open CONF_OPEN file CONF for %s\n", G.ssid);
      if (!G.ssid_specified.Get()) {
	snprintf (gp_str,GP_STR_LEN, WIFI_OPENSETUP_STR2, "B-Guest");
      } else {
	snprintf (gp_str,GP_STR_LEN, WIFI_OPENSETUP_STR2, G.ssid);
      }
      diag_system (VVERBOSE,verbosity, gp_str);
      diag_verbose(VVERBOSE,verbosity, "replacing ANY with %s, keygen NONE\n", G.ssid);
    } else {
      diag_verbose (VVERBOSE, verbosity, "pasphrase specified %s\n",G.passphrase);
      snprintf (gp_str,GP_STR_LEN, WIFI_SETUP_STR1, G.ssid, G.passphrase);
      diag_system (VVERBOSE,verbosity, gp_str);
      diag_verbose(VVERBOSE,verbosity, "trying this passphrase %s for %s %s\n", gp_str, G.ssid, G.passphrase);
    }

    snprintf (gp_str,GP_STR_LEN, WIFI_SETUP_STR2, RDW_TMP_DIR);
    diag_system (VVERBOSE,verbosity, gp_str);
    diag_verbose(VVERBOSE,verbosity, "trying this setup %s \n", gp_str);
    sleep (3*SECONDS);
    //!<<<< start dhcp daemon
    if (Verbosity.Get()) {
      snprintf (gp_str,GP_STR_LEN, WIFI_SETUP_STR3_VERBOSE, G.interface);
    } else {
      snprintf (gp_str,GP_STR_LEN, WIFI_SETUP_STR3, G.interface);
    }
    diag_system (VVERBOSE,verbosity, gp_str);
    //!<<<< first ping usually fails, possibly because the wpa_supp() script is still executing.
    //!<<<< so send a scrificial PING first
    //!<<<< the -U option supresses the time of day warning 
    snprintf (gp_str,GP_STR_LEN, WIFI_PING_STR1, 1, 64, G.pingbackaddr);
    diag_system (VVERBOSE,verbosity,gp_str);
  } else {
    diag_verbose(VVERBOSE,verbosity, "%s\n", "wifi setup skipped");
  }


  if (!do_ping_skip) {
      
    char *p_buffer=0;
    unsigned int buflen=GP_STR_LEN*4;
    int i;
    int j;
    int first_time=1;
    float max,min,avg,jitter;
    int tx,rx,loss,_time;
    int rx_bytes=0;
    int tx_bytes=0;
    int k;
    jWriteControl jwc, *p_jwc=&jwc;
    char cmd [GP_STR_LEN];
    int len=0;


    //!<<<< just a warning, using localhost so the test will probably pass w/o verifying any wifi hw
    if (0==strncmp(G.pingbackaddr, WIFI_DEFAULT_PINGBACKADDR, GP_STR_LEN)) {
      diag_verbose(ALWAYS,verbosity, "==> WARNING: using %s\n", WIFI_DEFAULT_PINGBACKADDR);
    }
    
    p_buffer = (char *)malloc (buflen);
    if (NULL==p_buffer) {
      diag_verbose(VERBOSE,verbosity, "error allocating memory for json buffer buflen=%d\n", buflen);    
      exit (EXIT_FAILURE);
    }
    diag_verbose(VVERBOSE,verbosity, "allocated memory for json buffer buflen=%d\n", buflen);    
    
    for (i=0; i<G.iterations; i++) {
      rx_bytes=0;
      tx_bytes=0;
      G.stats.avg_jitter =0;
      G.stats.max_jitter =0;
      G.stats.min_jitter =0;
      G.stats.avg_time   =0;
      G.stats.max_time   =0;
      G.stats.min_time   =0;
      G.stats.sum_jitter =0;
      for (j=0; j<1; j++) {
	
	eth_GetStats (G.interface, &G.b_stats);
	
	//!<<<< if random length then seed random # first time thru
	if ((0==i) && (0==j) && (0==G.length) ) {
	  diag_verbose(VVERBOSE, verbosity, "seeding random on %s\n", G.interface);
	  srandom_int (time(NULL));
	}
	len =  (0==G.length) ? random_int (WIFI_PACKET_SIZE_MIN,WIFI_PACKET_SIZE_MAX) : G.length;
	snprintf (cmd, GP_STR_LEN,  WIFI_PING_STR2, G.count, len, G.pingbackaddr, RDW_TMP_DIR, WIFI_PING_FILE);
	diag_system (0,0, cmd);
	//	sleep (4*SECONDS);
	//	sleep (1*SECONDS);
	iperf_parse_ping_output (&max, &min, &avg, &jitter, &tx, &rx, &loss, &_time, &k);
	rx_bytes+=k*rx;
	tx_bytes+=(len+8)*G.count;
	diag_verbose(VVVERBOSE,verbosity, "next ping int=%d\n", i);
	
	G.stats.ping_samples=G.count;
	//!<<<< time
	if ((max>G.stats.max_time) || (G.stats.max_time==0)) G.stats.max_time=max;
	if ((min<G.stats.min_time) || (G.stats.min_time==0)) G.stats.min_time=min;
	G.stats.avg_time = avg;
	//!<<<< jitter
	G.stats.avg_jitter = jitter;
	sleep (1*SECONDS);
	
	G.stats.tx=tx;
	G.stats.rx=rx;
	G.stats.loss=loss;
	G.stats.time=_time;
	
	diag_verbose(VVVERBOSE,verbosity,"stats: %d %d %d %d\n", G.stats.tx,G.stats.rx,G.stats.loss,G.stats.time);
	
      } //!<<<< for j...
      //!<<<< do output
      
      //!<<<< raw/reconciled ethernet stats, currently no longer used here
      eth_GetStats (G.interface, &G.e_stats);
      eth_RecStats (&G.r_stats, G.b_stats, G.e_stats);
      
      out_thread (first_time,i,p_jwc,p_buffer,buflen, len, rx_bytes, tx_bytes);
      if (first_time) first_time=0;
    } //!<<<< for i...
    
    if (p_buffer) free (p_buffer);
  } //!<<<< if (!do_ping_skip)

  //!<<<< Tear Down Connection >>>>
  if (!do_teardown_skip) {
    diag_verbose(VVERBOSE,verbosity, "%s\n", "doing teardown");    
    //!<<<< not skipping teardown
    diag_verbose(VVERBOSE,verbosity, "%s\n", "killing wifi connection");
    diag_system (VVERBOSE,verbosity,"killall wpa_supplicant > /dev/null");
    diag_system (VVERBOSE,verbosity,"killall dhcpcd > /dev/null");  
  } else {
    diag_verbose(VVERBOSE,verbosity, "%s\n", "wifi teardown skipped");
  }
  
  exit (EXIT_SUCCESS);
}	

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int wifi_getIpAddr (char *interface, char *ip_address_str) {

  unsigned int i;
  int sock;
  struct ifreq ifreqs[20];
  struct ifconf ic;
  
  ic.ifc_len = sizeof ifreqs;
  ic.ifc_req = ifreqs;
  
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) return (0);
  if (ioctl(sock, SIOCGIFCONF, &ic) < 0) return (0);
 
  //!<<<< look for interface (i.e. eth0, wlan0...) 
  for (i = 0; i < ic.ifc_len/sizeof(struct ifreq); ++i) {
    if (0==strcmp(interface, ifreqs[i].ifr_name)) {
      printf("Found IP for interface: %s: %s\n", ifreqs[i].ifr_name,inet_ntoa(((struct sockaddr_in*)&ifreqs[i].ifr_addr)->sin_addr));
      return (!0);
    }
  }
  return 0;
}	

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< parse 
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void iperf_parse_procnetdev_output (long long int *rx_bytes, long long int *rx_packets, long long int *tx_bytes, long long int *tx_packets) {
  char gp_str [GP_STR_LEN];
  int g=0;

  //!<<<< open proc/net/dev, look for stats corresponding to the interface we're
  //!<<<< using (i.e. -Iwlan0) and update stats
  if (0==G.interface[0]) {
    diag_verbose(VERBOSE,verbosity, "no interface specified, cannot read %s stats, use -I option\n", PROC_NET_DEV);
    return;
  }
  snprintf (gp_str, GP_STR_LEN, "%s", PROC_NET_DEV);
  if (!diag_ScrapeFileLinePat(gp_str, 
			      G.interface,
			      1, 
			      "%s %lld %lld %d %d %d %d %d %d %lld %lld %d %d %d %d %d %d",
			      gp_str, rx_bytes,rx_packets,&g,&g,&g,&g,&g,&g, tx_bytes,tx_packets,&g,&g,&g,&g,&g,&g)) {
    diag_verbose(VERBOSE,verbosity, "error reading file %s\n", gp_str);
    return ;
  }
  diag_verbose(VERBOSE,verbosity, "read rx_bytes=%lld rx_packets=%lld tx_bytes=%lld tx_packets=%lld\n", *rx_bytes, *rx_packets, *tx_bytes, *tx_packets);
}		

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< parse 
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void iperf_parse_procnetsnmp_output (long long int *in_msgs, long long int *in_errors, long long int *in_dst_unreach, long long int *out_msgs, long long int *out_errors, long long int *out_dst_unreach) {
  char gp_str [GP_STR_LEN];
  int g=0;

  snprintf (gp_str, GP_STR_LEN, "%s", PROC_NET_SNMP);
  if (!diag_ScrapeFileLinePat(gp_str, 
			      "Icmp: ", 
			      2,
			      "%s %lld %d %d %d %d %d %d %d %d %d %d %d %d %d %lld %d %d %d %d %d %d %d %d %d %d %d %d",
			      gp_str, in_msgs, in_errors,&g,in_dst_unreach,&g,&g,&g,&g,&g,&g,&g,&g,&g,&g,out_msgs,out_errors,out_dst_unreach,&g,&g,&g,&g,&g,&g,&g,&g,&g,&g)) {
    diag_verbose(VERBOSE,verbosity, "error reading file %s\n", gp_str);
    return ;
  }
  diag_verbose(VERBOSE,verbosity, "read in_msgs=%lld out_msgs=%lld\n", *in_msgs, *out_msgs);
}		

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< parse 
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void iperf_parse_ping_output (float *max, float *min, float *avg, float *jitter, int *tx, int *rx, int *loss, int *time, int *rx_bytes) {
  
  char gp_str [GP_STR_LEN];
  char gp_str2 [GP_STR_LEN];
  char gp_str3 [GP_STR_LEN];
  char gp_str4 [GP_STR_LEN];
  char gp_str5 [GP_STR_LEN];
  char gp_str6 [GP_STR_LEN];

  //!<<<< open file generated by ping test and process output

  //!<<<< first the avg/min/max part
  snprintf (gp_str, GP_STR_LEN, "%s/%s",  RDW_TMP_DIR, WIFI_PING_FILE);
  diag_verbose(VVVERBOSE,verbosity, "scraping from %s stats\n", gp_str);
  if (!diag_ScrapeFileLinePat(gp_str, PING_OUTPUT_PATTERN1, 1, "%s %s %s %s %s", gp_str2,gp_str2,gp_str2,gp_str3,gp_str2)) {
    diag_verbose(VERBOSE,verbosity, "error reading file %s\n", gp_str);
    return ;
  }
  if (NULL!=gp_str3) {
    sscanf (strtok(gp_str3,"/"), "%f", min);
    sscanf (strtok(NULL,"/"), "%f", avg);
    sscanf (strtok(NULL,"/"), "%f", max);
    sscanf (strtok(NULL,"/"), "%f", jitter);
  }

  //!<<<< then the rx bytes count part
  snprintf (gp_str, GP_STR_LEN, "%s/%s",  RDW_TMP_DIR, WIFI_PING_FILE);
  diag_verbose(VVVERBOSE,verbosity, "scraping from %s stats\n", gp_str);
  if (!diag_ScrapeFileLinePat(gp_str, PING_OUTPUT_PATTERN3, 1, "%s %s %s %s %s %s %s %s %s", gp_str3,gp_str2,gp_str2,gp_str2,gp_str2,gp_str2,gp_str2,gp_str2,gp_str2)) {
    diag_verbose(VERBOSE,verbosity, "error reading file %s\n", gp_str);
    return ;
  }
  if (NULL!=gp_str3) {
    sscanf (gp_str3, "%d", rx_bytes);    
  }
  
  //!<<<< then the tx/rx counts
  snprintf (gp_str, GP_STR_LEN, "%s/%s",  RDW_TMP_DIR, WIFI_PING_FILE);
  diag_verbose(VVVERBOSE,verbosity, "scraping from %s stats\n", gp_str);
  if (!diag_ScrapeFileLinePat(gp_str, PING_OUTPUT_PATTERN2, 1, "%s %s %s %s %s %s %s %s %s %s", gp_str3,gp_str2,gp_str2,gp_str4,gp_str2,gp_str5,gp_str2,gp_str2,gp_str2,gp_str6)) {
    diag_verbose(VERBOSE,verbosity, "error reading file %s\n", gp_str);
    return ;
  }
  diag_verbose(VVVERBOSE,verbosity, "scraped from ping stats %s %s %s %s\n", gp_str3, gp_str4, gp_str5, gp_str6);
  if (NULL!=gp_str3) {
    sscanf (gp_str3, "%d", tx);    
  }
  if (NULL!=gp_str4) {
    sscanf (gp_str4, "%d", rx);    
  }
  if (NULL!=gp_str5) {
    sscanf (gp_str5, "%d", loss);    
  }
  if (NULL!=gp_str6) {
    sscanf (gp_str6, "%d", time);    
  }

}		

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< parse 
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void iperf_parse_iwlink_output (char *interface, char *ssid, float *freq, float *signal) {
  
  char gp_str [GP_STR_LEN];
  char gp_str2 [GP_STR_LEN];

  //!<<<<< open file generated by ping test and process output
  snprintf (gp_str, GP_STR_LEN, WIFI_LINK_STATS, interface, RDW_TMP_DIR, WIFI_IWLINK_FILE);
  diag_system (0,0,gp_str);  
  snprintf (gp_str, GP_STR_LEN, "%s/%s",  RDW_TMP_DIR, WIFI_IWLINK_FILE);
  if (!diag_ScrapeFileLinePat(gp_str, "SSID", 1, "%s %s", gp_str2,ssid)) {
    diag_verbose(VERBOSE,verbosity, "error reading file %s\n", gp_str);
    return ;
  }
  if (!diag_ScrapeFileLinePat(gp_str, "freq", 1, "%s %f", gp_str2,freq)) {
    diag_verbose(VERBOSE,verbosity, "error reading file %s\n", gp_str);
    return ;
  }
  if (!diag_ScrapeFileLinePat(gp_str, "signal", 1, "%s %f %s", gp_str2,signal,gp_str2)) {
    diag_verbose(VERBOSE,verbosity, "error reading file %s\n", gp_str);
    return ;
  }
}		




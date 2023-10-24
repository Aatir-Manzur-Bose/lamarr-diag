#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h> 
#include <termios.h> 
#include <pthread.h>
#include <time.h>
#include <ncurses.h>
#include <getopt.h>
#include "diag_common.h"
#include "jWrite.h"
#include <sys/stat.h>

#ifdef JSON_READ
#pragma message("==> JSON_READ defined")
#endif

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< globals
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
extern int verbosity;
FLAG Verbosity;
FLAG csvMode;
#define verbosity Verbosity.Get()
LOGFILE LogFile;
LOGFILE JsonFile;
DIAG_PLATFORM_TAB_ENT DiagPlatformTab[] = {
  { DIAG_PLATFORM_EDDIE},
  { DIAG_PLATFORM_PROFESSOR},
  { DIAG_PLATFORM_GINGERCHEEVERS},
  { DIAG_PLATFORM_FLIPPER},
  { DIAG_PLATFORM_TAYLOR},
  { DIAG_PLATFORM_RIVIERA},
  { DIAG_PLATFORM_QUALCOMM},
  { DIAG_PLATFORM_SANDIEGO},
  {NULL},
};

//!<<<< MOF Response strings
STR_TAB_ENT diag_mofRespStrTab[] = {
  {MOF_RESP_STATUS,"mofRspStatus"},
  {MOF_RESP_CODE,"mofRspCode"},
  {MOF_RESP_DETAILS,"mofRspDetails"},
};

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< char *diag_StrTrunc (char *str, int)
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
char *diag_StrTrunc (char *str, int len) {
  if (NULL!=str) {
    str[len]=0;
    return (str);
  }
  return (NULL);
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< remove substring from string
//!<<<< dPtr must be large enough for string
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void diag_SubStrRemove (char *pSrc, const char *pPat, const char *pPad) { 

  char *r=NULL;
  char *p_s=pSrc;
  int first=0;
  int done=0;

  //!<<<< arg check
  if (NULL==pSrc) {
    diag_verbose (VERBOSE, verbosity, "error NULL pointer pSrc=%p\n", pSrc);
    return;
  }

  for (first=!0,done=0,r=p_s; !done; ) { 
    r=strstr (p_s, pPat);
    if (r) {
      p_s=r;
      r+=strlen(pPat);
      if (!first) {
	memmove (p_s, pPad, strlen(pPad));
	p_s+=strlen(pPad);
	memmove (p_s, r, strlen(r)+1);
      } else {
	memmove (p_s, r, strlen(r)+1);
      }
      if (first) first=0;
    } else done=!0;
  }
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< scrape formatted input from file
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int diag_ScrapeFileLinePat (char *pFileName, const char *pPattern, int occurences, const char *pFmt, ...) {
  int ret=0;
  FILE *p_file=0;
  char *line=NULL;
  size_t n=0;
  va_list args;
  va_start(args,pFmt);
  
  //!<<<< open file
  p_file = fopen (pFileName,"r");
  if (NULL==p_file) {
    diag_verbose(VERBOSE,Verbosity.Get(), "error opening file: %s, exiting\n", pFileName);
    return(0);
  }
  diag_verbose(VERBOSE,Verbosity.Get(), "processing file %s str=%s occur=%d\n", pFileName,pPattern,occurences);
  //!<<<< until EOF
  //!<<<< get line
  while (1) {
    if (-1==getline (&line, &n, p_file)) {
      diag_verbose(VERBOSE,Verbosity.Get(), "error OR done readln from line=%s\n", line);
      ret=0;
      break;
    }
    //    diag_verbose(VVVERBOSE,Verbosity.Get(), "reading line=%s \n", line);
    if (strstr (line, pPattern)) {
      if (--occurences) {
	diag_verbose(VVVERBOSE,Verbosity.Get(), "reading line=%s fmt=%s, skiping occurence=%d\n", line, pFmt, occurences+1);
	continue;
      }
      //!<<<< sscanf line  
      diag_verbose(VVVERBOSE,Verbosity.Get(), "reading line=%s fmt=%s\n", line, pFmt);
      vsscanf (line, pFmt, args);
      va_end(args);
      if (line) free (line);
      ret=!0;
      break;
    }
  }
  
  if (p_file) fclose (p_file);
  return (ret);
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< scrape formatted input from file, return line
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int diag_ScrapeFileLinePatPtr (char *pFileName, const char *pPattern, int occurences, char *pLine, int len) {
  int ret=0;
  FILE *p_file=0;
  char *line=NULL;
  size_t n=0;

  //!<<<< arg checking
  if (NULL==pLine) {
    diag_verbose(VERBOSE,Verbosity.Get(), "error line bufer is null %p\n", pLine);
    return (0);
  }
  
  //!<<<< open file
  p_file = fopen (pFileName,"r");
  if (NULL==p_file) {
    diag_verbose(VERBOSE,Verbosity.Get(), "error opening file: %s, exiting\n", pFileName);
    return(0);
  }
  diag_verbose(VERBOSE,Verbosity.Get(), "processing file %s str=%s occur=%d\n", pFileName,pPattern,occurences);
  //!<<<< until EOF
  //!<<<< get line
  while (1) {
    if (-1==getline (&line, &n, p_file)) {
      diag_verbose(VERBOSE,Verbosity.Get(), "error OR done readln from line=%s\n", line);
      diag_verbose (VVVERBOSE, verbosity, "Error getline errno=%d errstr=%s\n", errno,strerror(errno));
      ret=0;
      break;
    }

    if (strstr (line, pPattern)) {
      if (--occurences) {
	diag_verbose(VVVERBOSE,Verbosity.Get(), "reading line=%s  skiping occurence=%d\n", line,  occurences+1);
	continue;
      }
      diag_verbose(VVVERBOSE,Verbosity.Get(), "returnig line=%s \n", line);
      strncpy (pLine, line, len);
      if (line) free (line);
      ret=!0;
      break;
    }
  }
  
  if (p_file) fclose (p_file);
  return (ret);
}  
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< scrape formatted input from file/line
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int diag_ScrapeFileLine (char *pFileName, const char *pFmt, ...) {
  FILE *p_file=0;
  char *line=NULL;
  size_t n=0;
  va_list args;
  va_start(args,pFmt);
  
  //!<<<< open file
  p_file = fopen (pFileName,"r");
  if (NULL==p_file) {
    diag_verbose(VERBOSE,Verbosity.Get(), "error opening file: %s, exiting\n", pFileName);
    return(0);;
  }
  //!<<<< get line
  if (-1==getline (&line, &n, p_file)) {
    diag_verbose(VERBOSE,Verbosity.Get(), "error readln from line=%s\n", line);
    return(0);
  } 
  //!<<<< sscanf line  
  vsscanf (line, pFmt, args);
  va_end(args);

  //!<<<< free memory & close file
  if (line) {
    free (line);
    line=NULL;
  }
  if (p_file) fclose (p_file);
  return (!0);
}


//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< diag_log_new ()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void diag_log_new (char *pFileName) {
  char gp_str [GP_STR_LEN];
  
  //!<<<< return, no log file specified
  if (0==pFileName[0]) return;
  
  //!<<<< open file, assumes file is located in TMP dir
  snprintf (gp_str, GP_STR_LEN, "%s/%s",  RDW_TMP_DIR,pFileName);
  unlink (gp_str);
  diag_verbose(VERBOSE,Verbosity.Get(), "deleting %s\n", gp_str);
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< diag_log_hdr ()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void diag_log_hdr (char *pFileName, const char *pTest, int argc, char **argv) {
  char gp_str [GP_STR_LEN];
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  int i;

  //!<<<< return, no log file specified
  if (0==pFileName[0]) return;

  for (i=0,gp_str[0]=0; i<argc; i++) {
    strncat (gp_str, argv[i], GP_STR_LEN);
    strncat (gp_str, " ", GP_STR_LEN);
  }
  diag_log (pFileName, "Running: %s Command: %s At: %s\n", pTest, gp_str, asctime(tm));
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< diag_log ()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void diag_log (char *pFileName, const char *pFmt, ...) {
  FILE *p_file=0;
  va_list args;
  char gp_str [GP_STR_LEN];

  //!<<<< return, no log file specified
  if (0==pFileName[0]) return;

  //!<<<< open file, assumes file is located in TMP dir
  snprintf (gp_str, GP_STR_LEN, "%s/%s",  RDW_TMP_DIR,pFileName);
  diag_verbose(VERBOSE,Verbosity.Get(), "writing to log file %s\n", gp_str);
  p_file = fopen (gp_str,"a");
  if (NULL==p_file) {
    diag_verbose(VERBOSE,Verbosity.Get(), "error opening file: %s, exiting\n", gp_str);
    return;
  }

  //!<<<< vfprintf line  
  va_start(args,pFmt);
  vfprintf (p_file, pFmt, args);
  va_end(args);

  //!<<<< close file
  if (p_file) fclose (p_file);
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void diag_log_ncurses_done () {
#ifdef NCURSES_ON
  endwin();
#endif
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void diag_log_ncurses_init (int _x, int _y) {
#ifdef NCURSES_ON
  int x=0,y=0;
  //!<<<< init ncurses stuff
  initscr();
  clear();  
  //!<<<< resize ncurses window to handle long lines of output
  getmaxyx(stdscr,x,y);
  diag_verbose( VVVERBOSE, Verbosity.Get(),"ncurses x=%d y=%d\n", x, y);
  resize_term(_y,_x);
  getmaxyx(stdscr,x,y);
  diag_verbose (VVVERBOSE, Verbosity.Get(), "resized ncurses x=%d y=%d\n", x, y);
#endif
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void diag_log_ncurses_refresh () {
#ifdef NCURSES_ON
  refresh();
  clear();
#endif
}


//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< diag_access/get/setGlobalLogfile()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
char *diag_GlobalLogfileAccess(RW rw, char *pLogfile) {
  static char diag_Logfile [GP_STR_LEN];
  if (R==rw) return (diag_Logfile);
  else {
    strncpy (diag_Logfile, pLogfile, GP_STR_LEN);
    return (0);
  }
}
char *diag_GlobalLogfileGet (void) {
  return (diag_GlobalLogfileAccess(R, NULL));
}
void diag_GlobalLogfileSet (char *pLogfile) {
 
  if (NULL!=pLogfile) {
    if (0!=pLogfile[0]) {
      diag_GlobalLogfileAccess (W, pLogfile);
    }
  }
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< diag_log_x() - used by diag_verbose_x to cause verbose messages 
//!<<<< to be included in logs
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void diag_log_x (const char *pFmt, ...) {
  FILE *p_file=0;
  va_list args;
  char *p_str=0;
  char gp_str [GP_STR_LEN];
  
  //!<<<< if filename specified then we're logging also...
  p_str = diag_GlobalLogfileGet ();
  if (NULL!=p_str) {
    if (0!=p_str[0]) {
      //!<<<< open file, assumes file is located in TMP dir
      snprintf (gp_str, GP_STR_LEN, "%s/%s",  RDW_TMP_DIR,p_str);
      p_file = fopen (gp_str,"a");
      if (NULL==p_file) {
	//!<<< can do diag_verbose macro here, calls itself
	printf("error opening file: %s, exiting\n", gp_str);
      }
    }
  }
  
  //!<<<< console
  va_start(args,pFmt);  
  vprintf(pFmt, args);  
  //!<<<< log
  if (p_file) { 
    vfprintf (p_file, pFmt, args);
    fclose (p_file);
  }
  va_end(args);
}


void diag_log_and_print (char *pFileName, const char *pFmt, ...) {
  FILE *p_file=0;
  va_list args;
  char gp_str [GP_STR_LEN];
  
  //!<<<< if filename specified then we're logging also...
  if (NULL!=pFileName) {
    if (0!=pFileName[0]) {
      //!<<<< open file, assumes file is located in TMP dir
      snprintf (gp_str, GP_STR_LEN, "%s/%s",  RDW_TMP_DIR,pFileName);
      p_file = fopen (gp_str,"a");
      if (NULL==p_file) {
      }
    }
  }
  
  va_start(args,pFmt);  
  vprintf(pFmt, args);  
  
  //!<<<< for log file
  //!<<<< vfprintf line
  if (p_file) {  
    vfprintf (p_file, pFmt, args);
    fclose (p_file);
  }
  
  va_end(args);
}
  
void diag_log_ncurses (char *pFileName, const char *pFmt, ...) {
  FILE *p_file=0;
  va_list args;
  char gp_str [GP_STR_LEN];

  //!<<<< if filename specified then we're logging also...
  if (NULL!=pFileName) {
    if (0!=pFileName[0]) {
      //!<<<< open file, assumes file is located in TMP dir
      snprintf (gp_str, GP_STR_LEN, "%s/%s",  RDW_TMP_DIR,pFileName);
      p_file = fopen (gp_str,"a");
      if (NULL==p_file) {
      }
    }
  }

  va_start(args,pFmt);  

#ifdef NCURSES_ON
  //!<<<< for ncurses
  vw_printw(stdscr, pFmt, args);
#else
  vprintf(pFmt, args);  
#endif

  //!<<<< for log file
  //!<<<< vfprintf line
  if (p_file) {  
    vfprintf (p_file, pFmt, args);
    fclose (p_file);
  }

  va_end(args);
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< void diag_printComponentsVersions () 
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void diag_printComponentVersions () {
  char gp_str [GP_STR_LEN];
  char *str1;
  char *str2;
  char *str3;
  NewLine();
  diag_printf ("Components%s\n","");
  diag_printf ("%-36s %-36s\n", "Name", "Version");
  diag_printf ("%-36s %-36s\n", "------------------------------------", "------------------------------------");
  strncpy (gp_str, COMPONENT_VERSION_CASTLE_CLI, GP_STR_LEN);
  diag_splitComponents (gp_str, &str1, &str2, &str3);
  diag_printf ("%-36s %-36s\n",  str1, str3);
  strncpy (gp_str, COMPONENT_VERSION_CASTLE_LPM_MFG_TOOL, GP_STR_LEN);
  diag_splitComponents (gp_str, &str1, &str2, &str3);
  diag_printf ("%-36s %-36s\n",   str1, str3);
  strncpy (gp_str, COMPONENT_VERSION_PROTOBUF, GP_STR_LEN);
  diag_splitComponents (gp_str, &str1, &str2, &str3);
  diag_printf ("%-36s %-36s\n",   str1, str3);
  strncpy (gp_str, COMPONENT_VERSION_RIVIERA_HSP, GP_STR_LEN);
  diag_splitComponents (gp_str, &str1, &str2, &str3);
  diag_printf ("%-36s %-36s\n",   str1, str3);
  strncpy (gp_str, COMPONENT_VERSION_RIVIERA_LPM_SERVICE, GP_STR_LEN);
  diag_splitComponents (gp_str, &str1, &str2, &str3);
  diag_printf ("%-36s %-36s\n",   str1, str3);
  strncpy (gp_str, COMPONENT_VERSION_RIVIERA_LPM_UPDATER, GP_STR_LEN);
  diag_splitComponents (gp_str, &str1, &str2, &str3);
  diag_printf ("%-36s %-36s\n",  str1, str3);
  NewLine();
}
//!<<<< ...........................................................
//!<<<< diag_splitComponentStr ()
//!<<<< ...........................................................
void diag_splitComponents (char *str, char **pName, char **pSelected, char **pLatest) {
  *pName     = strtok (str," ");
  *pSelected = strtok (NULL," ");
  *pLatest   = strtok (NULL," ");
  return;
}

//!<<<< ...........................................................
//!<<<< Dump()
//!<<<< ...........................................................
void Dump (const char *Heading, unsigned char *pBuff, unsigned int Len, DUMP_FLAGS Flags) {

  unsigned int  i=0;
  unsigned int  j=0;
  unsigned char *p_data_1=NULL;
  unsigned char *p_data_2=NULL;
  
  NewLine();
  diag_printf ("%s %p\n", Heading, pBuff);
  for (i=0,p_data_1=pBuff,p_data_2=pBuff; i<Len; i+=16) {
    diag_printf ("%04d: ", i);
    for (j=0; j<16; j++, p_data_1++) 
      diag_printf ("%s%02x ", (j==8) ? ": " : "", *p_data_1);
    if (INCLUDE_ASCII & Flags) {
      diag_printf ("%s", "- ");
      for (j=0; j<16; j++, p_data_2++) 
	diag_printf ("%c", (isalnum(*p_data_2)) ? *p_data_2 : ' ');
    } else p_data_2+=16;
    NewLine();
  }
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< int diag_IsPlatform (char *pStr) 
//!<<<< call like this: 
//!<<<<   if (diag_IsPlatform("Professor")) ...
//!<<<<   if (diag_IsPlatform("Eddie")) ...
//!<<<<   if (diag_IsPlatform("Riviera")) ...
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int diag_IsPlatform (const char *pStr) {
  char gp_str [GP_STR_LEN];
  if (diag_ScrapeFileLinePat((char *)"/proc/device-tree/model",pStr,1,"%s",gp_str)) return (!0);
  return (0); 
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< char *diag_GetPlatform (char *pStr)
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
const char *diag_GetPlatform (void) {
  pDIAG_PLATFORM_TAB_ENT p=NULL;
  int done=0;
  
  for (p=DiagPlatformTab; !done; p++) {
    if (NULL==p->model) {
      done=!0;
      break;
    }
    
    if (diag_IsPlatform(p->model)) {
      return (p->model);
    }
  }
  return (NULL);
}

char *diag_GetPlatform (char *pStr) {
  pDIAG_PLATFORM_TAB_ENT p=NULL;
  int done=0;

  if (NULL==pStr) return (NULL);

  for (p=DiagPlatformTab; !done; p++) {
    if (NULL==p->model) {
      done=!0;
      break;
    }

    if (diag_IsPlatform(p->model)) {
      strncpy (pStr, p->model, GP_STR_LEN);
      return (pStr);
    }
  }
  return (NULL);
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< diag_PlatformValid() 
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int diag_PlatformValid (pDIAG_VALID_PLATFORMS_ENT pTab) {
  pDIAG_VALID_PLATFORMS_ENT p=NULL;
  int done=0;
  
  for (p=pTab; !done; p++) {
    if (NULL==p->pPlatformStr) {
      done=!0;
      break;
    }
    
    if (diag_IsPlatform(p->pPlatformStr)) {
      return (!0);
    }
  }
  return (0);
}
  
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< common getopt() handling 
//!<<<< (common across all diags -L -R -V -v- H -h -v)
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< void diag_printVersion () 
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void diag_printVersion () {
  if (0==strcmp (MODULE_VERSION2, "")) {
    diag_printf ("%s\n", MODULE_VERSION);
  } else { 
    diag_printf ("%s\n", MODULE_VERSION2);
  }
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< void diag_setVerbosity (int *_verbosity) 
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void diag_setVerbosity (int *_verbosity) {
  (*_verbosity)++;
  diag_verbose(VVERBOSE,Verbosity.Get(),"verbosity: %d\n", Verbosity.Get());  
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< void diag_resetLogfie (int *_resetLogfile) 
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void diag_resetLogfile (char *pLogFile, const char *pDiagName) {

  if (pLogFile) {
    if (0!=pLogFile[0]) {
      diag_log_new (pLogFile);
      diag_verbose(VVERBOSE,Verbosity.Get(), "reset logfile=%s\n", pLogFile);
      char gp_str [GP_STR_LEN];
      snprintf (gp_str, GP_STR_LEN, "%s.json", pDiagName);
      jWriteFileReset(gp_str, NULL);
      diag_verbose(VVERBOSE,Verbosity.Get(), "reset json file=%s\n", gp_str);
      return;
    }
  }
  diag_verbose(VVERBOSE,Verbosity.Get(), "error resetting log file=%s %s\n", pLogFile, pDiagName);
}
void diag_resetLogfile (int *_resetLogfile) {
  (*_resetLogfile)=!0;
  diag_verbose(VVERBOSE,verbosity,"resetLogfile: %d\n", *_resetLogfile);  
}
void diag_resetLogFile (void) {
  if (LogFile.Exists()) {
    diag_verbose_x(VERBOSE, "resetting log file %s\n", LogFile.Get());
    diag_log_new(LogFile.Get());
  }
  if (JsonFile.Exists()) {
    diag_verbose_x(VERBOSE, "resetting json file %s\n", JsonFile.Get());
    diag_log_new(JsonFile.Get());
  }
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< void diag_shortHelp (void *shortHelpFn()) 
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void diag_help (void (*fp)(void)) {
  fp();
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< void diag_setLogfile (int *_Logfile, char *filename);
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void diag_setLogfile (char *_logFilename, int logFileLen, char *_logFileArg) {
  strncpy (_logFilename, _logFileArg, logFileLen);
  diag_verbose(VVERBOSE,verbosity,"Logfile: %s\n", _logFilename);  
}

//!<<<< expected logfilename to be root file name, not extension, will be added here
void diag_setLogfile (char *_logFilename) {
  char gp_str [GP_STR_LEN];
  snprintf (gp_str, GP_STR_LEN, "%s.log", _logFilename);
  LogFile.Set(gp_str);
  diag_verbose(VVERBOSE,verbosity,"set Logfile: %s\n", gp_str);  
  snprintf (gp_str, GP_STR_LEN, "%s.json", _logFilename);
  JsonFile.Set(gp_str);
  diag_verbose(VVERBOSE,verbosity,"set Jsonfile: %s\n", gp_str);  
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< void diag_usage_short (char *s1, char *s2) {
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void diag_usage_short (const char *s1, const char *s2) {
  fprintf (stdout, "%-18s: %-36s\n", s1, s2);   
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< void diag_getopt_ArgsDisplay (int argc, char **argv) 
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=-
void diag_getopt_ArgsDisplay (int argc, char **argv) {
  int i=0;
  diag_printf ("argc=%d\n", argc);
  for (i=0; i<argc; i++) {
    diag_printf ("argv[%d]=%s\n", i, argv[i]);
  }
  NewLine();
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< int diag_getopt_OptArgExists ()
//!<<<< useful for checking all the variances of the getopt string,
//!<<<< -missing option (end of string, not adjacent -x)
//!<<<< -missing option (NOT end of string, no adjacent -x)
//!<<<< -connected arg (-x1 not -x 1) 
//!<<<< -adjusting optindex where applicable
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int diag_getopt_OptArgExists (int argc, char **argv, char opt, char *arg, int *optind) {
  char gp_str [GP_STR_LEN];
  char *p=0;
  int i;

  //!<<<< display arglist
  if (diag_verbosity(VVVERBOSE,verbosity)) {
    diag_getopt_ArgsDisplay (argc, argv);
  }

  snprintf (gp_str, GP_STR_LEN, "%c%c", '-', opt);
  for (i=1; i<argc; i++) {
    p=strstr (argv[i], gp_str);
    if (p) {
      //!<<<< found -opt....
      diag_verbose(VVERBOSE,verbosity,"found %s in arg list at %d %s\n", gp_str, i, p+2);
      p+=2; //!>>>> move passed "-x"
      if (strlen(p)) {
	//!>>>> there an arg there
	strncpy (arg, p, strlen(p));
	return (!0);
      }
      if (NULL==argv[i+1]) {
	diag_verbose(VVERBOSE,verbosity,"no arg found following %s\n", gp_str);
	return (0);
      }
      //!<<<< note this may be an issue with string args containing '-' (like B-Employee), in
      //!<<<< which case you'll need to enter it as -sB-Employee.
      //      if (strstr (argv[i+1], "-")) {
      if (argv[i+1][0]=='-') {
	diag_verbose(VVERBOSE,verbosity,"option %s found following %s at optind=%d\n", argv[i+1], gp_str, *optind);
	if (NULL!=optind) {
	  (*optind)--;
	  diag_verbose(VVERBOSE,verbosity,"adjusting option index back to compensate %s %s to optind=%d\n", argv[i+1], gp_str, *optind);
	}
	return (0);	
      }
      //!<<<< found it, -opt + arg....
      strncpy (arg, argv[i+1], GP_STR_LEN);
      return (!0);
    }
  }
  diag_verbose(VVERBOSE,verbosity,"option %s not found \n", gp_str);  
  return (0);
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< Misc utilities
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void NewLine(int lines) {
  int i;
  for (i=0; i<lines; i++) {
    NewLine();
  }
}
void NewLine(void) {
  fprintf (stdout, "\n");
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
const char *diag_StrTabIndex2Str (p_STR_TAB_ENT pEnt, size_t tabSize, unsigned int Index) {
  unsigned int i;
  p_STR_TAB_ENT p;
  unsigned int max_index=tabSize/sizeof(tabSize);
  if (NULL==pEnt) return (NULL);
  if (Index>max_index) return (NULL);
  for (i=0,p=pEnt; i<=max_index; i++,p++) {
    if (p->Index==Index) return (pEnt[Index].Str);
  }
  return (NULL);
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int diag_StrTabStr2Index (p_STR_TAB_ENT pEnt, size_t tabSize, const char *str) {
  unsigned int i;
  p_STR_TAB_ENT p;
  unsigned int max_index=tabSize/sizeof(STR_TAB_ENT);
  if (NULL==pEnt) return (-1);
  for (i=0,p=pEnt; i<max_index; i++,p++) {
    if (0==strncmp (p->Str, str,GP_STR_LEN)) {
      return (p->Index);
    }
  }
  return (-1);
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< MOF BUFFER variable & methods
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
MOF_BUFFER::MOF_BUFFER (void) { 
  p_mofBuf=mofBuf; 
  mofBytes=0;
  diag_verbose_x(VVVERBOSE,"new mofBuf p_mofbuf=%p mofbuf=%p\n", p_mofBuf, mofBuf);
};
void MOF_BUFFER::Write (char *_p, unsigned int _len) { 
  diag_verbose_x(VVVERBOSE,"writing mofBuf p=%p bytes=%d total=%d\n", p_mofBuf, _len, _len+mofBytes);  
  if ((MOF_BUFFER_LEN)<=(mofBytes+_len)) {
    diag_verbose_x(VERBOSE,"mofBuf overrun current bytes=%d, adding %d, max=%d", mofBytes, _len, MOF_BUFFER_LEN);
    return;
  }
  memcpy (p_mofBuf, _p, _len);
  mofBytes+=_len;
  p_mofBuf+=_len;
};
void MOF_BUFFER::Reset (void) { 
  p_mofBuf=mofBuf; 
  mofBytes=0;
  diag_verbose_x(VVVERBOSE,"resetting mofBuf p_mofbuf=%p mofbuf=%p len=%d\n", p_mofBuf, mofBuf, GP_STR_LEN);
}; 
void MOF_BUFFER::Print (void) { 
  diag_verbose_x(VVVERBOSE,"printing mofBuf len=%d\n", mofBytes);
  mofBuf[mofBytes]=0;
  printf ("%s\n", mofBuf);
};
void MOF_BUFFER::SetErrorCode (unsigned int _err) {
  ErrCode=_err;
  diag_verbose_x(VVVERBOSE,"setting mof error code to %d\n", _err);
};
void MOF_BUFFER::SetErrorDetails (const char *_pFmt,...) {
  va_list args;
  va_start(args,_pFmt);
  diag_verbose_x(VVVERBOSE,"pFmt=%s\n", _pFmt);  
  vsprintf (ErrStr, _pFmt, args);
  diag_verbose_x(VVVERBOSE,"ErrStr=%s\n", ErrStr);  
  va_end(args);
  diag_verbose_x(VVVERBOSE,"setting mof error str to %s\n", ErrStr);
};
unsigned int MOF_BUFFER::GetErrorCode (void){
  return (ErrCode);
};
char *MOF_BUFFER::GetErrorDetails (void) {
  return (ErrStr);
};

#ifdef JSON_READ
//=================================================================
//
// Helper functions to read a JSON file into a malloc'd buffer with '\0' terminator
//


// readFileBuffer
// - reads file into a malloc'd buffer with appended '\0' terminator
// - limits malloc() to maxlen bytes
// - if file size > maxlen then the function fails (returns 0)
//
// returns: length of file data (excluding '\0' terminator)
//          0=empty/failed
//
unsigned long readFileBuffer( char *filename, struct FileBuffer *pbuf, unsigned long maxlen )
{
  FILE *fp;
  int i;

	if( (fp=fopen(filename, "rb")) == NULL )
	{
		printf("Can't open file: %s\n", filename );
		return 0;
	}
	// find file size and allocate buffer for JSON file
	fseek(fp, 0L, SEEK_END);
	pbuf->length = ftell(fp);
	if( pbuf->length >= maxlen )
	{
		fclose(fp);
		return 0;
	}
	// rewind and read file
	fseek(fp, 0L, SEEK_SET);
	pbuf->data= (unsigned char *)malloc( pbuf->length + 1 );
	memset( pbuf->data, 0, pbuf->length+1 );	// +1 guarantees trailing \0

	i= fread( pbuf->data, pbuf->length, 1, fp );	
	fclose( fp );
	if( i != 1 )
	{
		freeFileBuffer( pbuf );
		return 0;
	}
	return pbuf->length;
}

// freeFileBuffer
// - free's buffer space and zeros it
//
void freeFileBuffer( struct FileBuffer *buf )
{
	if( buf->data != NULL )
		free( buf->data );
	buf->data= 0;
	buf->length= 0;
}

#define MFG_DATA_JSON "/persist/mfg_data.json"
#define MFG_DATA_MD5  "/persist/mfg_data.md5"
#define MFG_DATA_MD5_LEN  (32)
#define MFG_DATA "/persist/mfg_data.md5"
static struct FileBuffer json;
static struct FileBuffer md5;

#if 0
int diag_mfgGet (char *key, int *value) {


  char gp_str3 [GP_STR_LEN];
  char gp_str4 [GP_STR_LEN];



  //// left off here - do i really neded to test/ store md5 or just re read file each time????


  
  //!<<<< read MD5
  snprintf (gp_str3, GP_STR_LEN, "%s", MFG_DATA_MD5);
  if (readFileBuffer( gp_str3, &md5, MFG_DATA_MD5_LEN ) == 0 ) {
    diag_verbose (ALWAYS,verbosity, "Can't open file: %s\n", gp_str3);
    return;
  }
  
  //!<<<< testing read of mfg data from json file


    snprintf (gp_str3, GP_STR_LEN, "%s", MFG_DATA_JSON);
    diag_verbose(ALWAYS,verbosity, "processing JSON file read test=%s\n", gp_str3);
    if (readFileBuffer( gp_str3, &json, FILE_BUFFER_MAXLEN ) == 0 ) {
      diag_verbose (ALWAYS,verbosity, "Can't open file: %s\n", gp_str3);
      return;
    }
    printf ("regionCode: %s\n", jsonQueryByKey( (char *)json.data, "{'regionCode'", gp_str4 ));
    printf ("productType: %s\n", jsonQueryByKey( (char *)json.data, "{'productType'", gp_str4 ));
  }
#endif


#endif

const char *validProductTypes [] = {
  "eddie",
  "professor",
  "chinger-cheevers",
  "flipper",
  NULL,
};

int diag_validProductType (char *productType) {
  int i;
  for (i=0; ; i++) {
    if (0==strncmp (validProductTypes[i],productType,GP_STR_LEN)) {
      return (!0);
    }
    if (NULL==validProductTypes[i]) {
      return (0);
    }
  }
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< randim_int () - within range of min and max inclusive
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int random_int (int min, int max) {
  return (min + rand() % (max+1 - min));
}
void srandom_int (int seed) {
  srand (seed);
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< eth_GetStats ()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int eth_GetStats (char *interface, pETH_STATS stats) {
  char gp_str [GP_STR_LEN];
  //!<<<< get stats
  if (!diag_ScrapeFileLinePat((char *)ETH_STATS_FILE, interface, 1, (char *)"%s%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d",
			   &stats->interface,
			   &stats->rx_bytes,
			   &stats->rx_packets,
			   &stats->rx_errs,
			   &stats->rx_drop,
			   &stats->rx_fifo,
			   &stats->rx_frame,
			   &stats->rx_compressed,
			   &stats->rx_multicast,
			   &stats->tx_bytes,
			   &stats->tx_packets,
			   &stats->tx_errs,
			   &stats->tx_drop,
			   &stats->tx_fifo,
			   &stats->tx_colls,
			   &stats->tx_carrier,
			   &stats->tx_compressed)) {
    diag_verbose(VERBOSE,verbosity, "error reading file %s\n", gp_str);
    return (0);
  }
  //  fprintf (stdout, "temp %d %d %d %d\n", stats->rx_bytes, stats->rx_packets, stats->rx_errs, stats->rx_drop);
  return (!0);
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int file_exists (char *filename) {
  struct stat path_stat;
  if (NULL==filename) {
    diag_verbose(VERBOSE,verbosity, "NULL filename %s\n", filename);
    return (0);
  }
  if (0==stat(filename, &path_stat)) return (!0);
  return (0);
}

//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< max & min
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
float diag_MIN (float _A_, float _B_) {
  return (((_A_)<(_B_))?(_A_):(_B_));
}
double diag_MIN (double _A_, double _B_) {
  return (((_A_)<(_B_))?(_A_):(_B_));
}
int diag_MIN (int _A_, int _B_) {
  return (((_A_)<(_B_))?(_A_):(_B_));
}
float diag_MAX (float _A_, float _B_) {
  return (((_A_)>(_B_))?(_A_):(_B_));
}
double diag_MAX (double _A_, double _B_) {
  return (((_A_)>(_B_))?(_A_):(_B_));
}
int diag_MAX (int _A_, int _B_) {
  return (((_A_)>(_B_))?(_A_):(_B_));
}
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//!<<<< eth_RecStats ()
//!<<<< =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
int eth_RecStats (pETH_STATS r_stats, ETH_STATS b_stats, ETH_STATS e_stats) {
  
  r_stats->rx_bytes      = 			   e_stats.rx_bytes - 			   b_stats.rx_bytes;
  r_stats->rx_packets    =   			   e_stats.rx_packets - 		   b_stats.rx_packets;
  r_stats->rx_errs       = 			   e_stats.rx_errs - 			   b_stats.rx_errs;
  r_stats->rx_drop       = 			   e_stats.rx_drop - 			   b_stats.rx_drop;
  r_stats->rx_fifo       = 			   e_stats.rx_fifo - 			   b_stats.rx_fifo;
  r_stats->rx_frame      = 			   e_stats.rx_frame - 			   b_stats.rx_frame;
  r_stats->rx_compressed = 			   e_stats.rx_compressed - 		   b_stats.rx_compressed;
  r_stats->rx_multicast  = 			   e_stats.rx_multicast - 		   b_stats.rx_multicast;
  r_stats->tx_bytes      = 			   e_stats.tx_bytes - 			   b_stats.tx_bytes;
  r_stats->tx_packets    = 			   e_stats.tx_packets - 		   b_stats.tx_packets;
  r_stats->tx_errs       =  			   e_stats.tx_errs - 			   b_stats.tx_errs;
  r_stats->tx_drop       = 			   e_stats.tx_drop - 			   b_stats.tx_drop;
  r_stats->tx_fifo       = 			   e_stats.tx_fifo - 			   b_stats.tx_fifo;
  r_stats->tx_colls      = 			   e_stats.tx_colls - 			   b_stats.tx_colls;
  r_stats->tx_carrier    = 			   e_stats.tx_carrier - 		   b_stats.tx_carrier;
  r_stats->tx_compressed = 			   e_stats.tx_compressed  - 		   b_stats.tx_compressed ;
  return (!0);
  }


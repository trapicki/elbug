/* serial elbug = electronic morse key. */
/* by Günther Montag dl4mge */
/* extenden by Patrick Strasser oe6pse <oe6pse@wirklich.priv.at> */

/* elbug is distributed alone and as part of the hf package. */
/* I plan too edit it also separate. */

/* Just compile me by 'gcc elbug.c -o elbug' */
/* and copy elbug to user/local/bin. */
/* Call me as root. */
/* How to cable your homebrew elbug? See below! */

/*
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#ifdef __linux__
#include <sys/io.h>
#define IOPERM ioperm
#endif
#ifdef __FreeBSD__ 
#include <machine/cpufunc.h>
#include <machine/sysarch.h>
#define IOPERM i386_set_ioperm
#endif
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/kd.h>		/* Linux, UnixWare */
#include <stdarg.h>
/*
  if kd.h not found, maybe you have to take one of these 2:
  <sys/vtkd.h> for OpenServer
  <sys/kbio.h> for FreeBSD
*/
#define PAUSE 0
#define DIT 1
#define DAH 2
#define WORDPAUSE 4
#define MAX_SIGN_SIZE 16
/* from cwdaemon/lib.c */
typedef struct {
  const unsigned char character;	/* The character represented */
  const char *representation;     /* Dot-dash shape of the character */
} cw_entry_t;

static const cw_entry_t cw_table[] = {
  /* ASCII 7bit letters */
  { 'A', ".-"     }, { 'B', "-..."   }, { 'C', "-.-."   },
  { 'D', "-.."    }, { 'E', "."      }, { 'F', "..-."   },
  { 'G', "--."    }, { 'H', "...."   }, { 'I', ".."     },
  { 'J', ".---"   }, { 'K', "-.-"    }, { 'L', ".-.."   },
  { 'M', "--"     }, { 'N', "-."     }, { 'O', "---"    },
  { 'P', ".--."   }, { 'Q', "--.-"   }, { 'R', ".-."    },
  { 'S', "..."    }, { 'T', "-"      }, { 'U', "..-"    },
  { 'V', "...-"   }, { 'W', ".--"    }, { 'X', "-..-"   },
  { 'Y', "-.--"   }, { 'Z', "--.."   },
  /* Numerals */
  { '0', "-----"  }, { '1', ".----"  }, { '2', "..---"  },
  { '3', "...--"  }, { '4', "....-"  }, { '5', "....."  },
  { '6', "-...."  }, { '7', "--..."  }, { '8', "---.."  },
  { '9', "----."  },
  /* Punctuation */
  { '"', ".-..-." }, { '\'',".----." }, { '$', "...-..-"},
  { '(', "-.--."  }, { ')', "-.--.-" }, { '+', ".-.-."  },
  { ',', "--..--" }, { '-', "-....-" }, { '.', ".-.-.-" },
  { '/', "-..-."  }, { ':', "---..." }, { ';', "-.-.-." },
  { '=', "-...-"  }, { '?', "..--.." }, { '_', "..--.-" },
  { '@', ".--.-." },
  /* Cwdaemon special characters */
  { '<', "...-.-" }, { '>', "-...-.-"}, { '!', "...-." }, 
  { '&', ".-..."  }, { '*', ".-.-."  },
  /* Error-sign */
  { 'e', "......."},
  /* ISO 8859-1 accented characters */
  { 0334,"..--"   },	/* U with diaresis */
  { 0304,".-.-"   },	/* A with diaeresis */
  { 0307,"-.-.."  },	/* C with cedilla */
  { 0326,"---."   },	/* O with diaresis */
  { 0311,"..-.."  },	/* E with acute */
  { 0310,".-..-"  },	/* E with grave */
  { 0300,".--.-"  },	/* A with grave */
  { 0321,"--.--"  },	/* N with tilde */
  /* ISO 8859-2 accented characters */
  { 0252,"----"   },	/* S with cedilla */
  { 0256,"--..-"  },	/* Z with dot above */
  /* Sentinel end of table value */
  { '\0', NULL } };

char *cable = "\n"
  "* * * elbug - electronic morse key by dl4mge. How to cable: * * *\n"
  "*  Middle pad: +9V battery via Resistor 2 k                     *\n"                           
  "*  Left  contact: -> DCD (9-pin plug: 1) (25-pin plug: 8)       *\n"
  "*  Right contact: -> CTS (9-pin plug: 8) (25-pin plug: 5)       *\n"
  "*  Ground: -pole battery (9-pin plug: 5) (25-pin plug: 7)       *\n"
  "* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n" ;

/* default parameters */
int pin = TIOCM_RTS;
char* name_ptt = "/dev/ttyS0";
char* name_spkr = "/dev/console";
int invert_ptt = 0;
int spkr = 0;
unsigned int wpm = 12, tone = 440;
int argp = 0;
int fd_ptt, fd_spkr = -1;
int dotus;
int bug_mode = 0;
int verbose = 0;
int dot_resolution = 5;

void verb(char *format, ...)
{
    va_list args;
    if (!verbose)
        return;

    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}
void decode(int sign) 
{
  static char morse[MAX_SIGN_SIZE];
  int i;
  static int loop = 0, pause = 0, pausecount = 0, bit = 0;
  unsigned char element;

  //printf("decode: ");

  if(!loop) {
    loop = sizeof(cw_table) / sizeof(cw_entry_t);
    printf("decode function: %d signs in table.\n", loop);
  }
	
  if (sign == DIT) element = '.';
  if (sign == DAH) element = '-';
  

  if (PAUSE != sign) {
    verb("%c", element);
    pause = 0;
    pausecount = 0;
    /* guard against bogus input */
    if (MAX_SIGN_SIZE <= bit ) {
      bit = 0;
      printf(" ??? ");
      return;
    }
    morse[bit] = element;
    morse[bit+1] = 0x0;
    bit++;
    //printf("%s\n", morse);
    
  } else {
    if (pause) return;
    if (!pausecount) { /* compare with morse table */
      morse[bit] = '\0';
      bit = 0;
      for (i = 0; i < loop - 1; i++) {
        //printf ("comparing sign %d...\n", i);
        if(!strcmp (cw_table[i].representation,	morse)) {
          printf("%c", cw_table[i].character);
          fflush(stdout);
          break;
        }	       	
        if (i == loop - 2) {
          printf(" ?? ");
          fflush(stdout);
        }
      }
    } 
    pausecount++;
    if (pausecount > WORDPAUSE) {
      printf(" ");
      fflush(stdout);
      pause = 1;
    }
  }
}

void wait(int us) {
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = us;
  select (0, NULL, NULL, NULL, &tv);
}

#ifdef KIOCSOUND
/* from cwlib, simplified */
console_open ()
{
  int has_KIOCSOUND = -1;
  /* Open the console device file, for write only. */
  
  if (!strcmp(name_spkr, "-")) {
    fd_spkr = 1; /* STDOUT */
    printf("using STDOUT for speaker output\n");
  } else {
    printf("using %s for speaker output\n", name_spkr);
    fd_spkr = open (name_spkr, O_WRONLY);
  }
  if (-1 == fd_spkr) {
    printf ("console %s can not be opened for speaker output\n", name_spkr);
    return 0;
  }
  has_KIOCSOUND = ioctl (fd_spkr, KIOCSOUND, 0);
  /* Check to see if the file can do console tones. */
  if (-1 == has_KIOCSOUND) {
    if(fd_spkr != 1) {
      close (fd_spkr);
    }
    printf ("console %s can not speak\n", name_spkr);
    return 0;
  }

  return 1;
}
#endif

void handle_signal(int sig) {
  printf("got signal %i, cleaning up\n");
#ifdef KIOCSOUND
  printf("stop speaker\n");
  ioctl (fd_spkr, KIOCSOUND, 0);
#endif
  exit(0);
}

void switch_tone(int state) {
  if (state) {
    if (spkr) ioctl(fd_spkr, KIOCSOUND, argp);
  } else {
    if (spkr) ioctl(fd_spkr, KIOCSOUND, 0);
  }
}

void output_elbug_serial(int ptt)
{
  int status;
	
  if (fd_ptt < 0) {
    printf("no serial port open for output.\n");
    exit (1);
  }

  /* tone */
  switch_tone(ptt);
  /* serial output */
	
  /* seems this worked for intel only. */
  /*
	if (ioctl(fd_ptt, 
    ((ptt && !invert_ptt) || (!ptt && invert_ptt)) ? 
    TIOCMBIS : TIOCMBIC, &pin)) {
    printf ("ioctl: TIOCMBI[CS]\n");
    printf ( "serial port can not be accessed!\n");
    exit (1);
	}
  */
	
  if (invert_ptt) ptt = !ptt;
  if (ioctl(fd_ptt, TIOCMGET, &status)) {
    printf ("ioctl: TIOCMGET, cannot read serial port.\n");
    if (spkr) ioctl(fd_spkr, KIOCSOUND, 0);
    exit (1);
  }
  if (ptt) {
    status &= pin;
  } else {
    status &= ~pin;
  }
  if (ioctl(fd_ptt, TIOCMSET, &status)) {
    printf ("ioctl: TIOCMSET, cannot write to serial port.\n");
    if (spkr) ioctl(fd_spkr, KIOCSOUND, 0);
    exit (1);
  }
}

void elbug_send_dit() {
  output_elbug_serial(1);
  //printf(".");
  wait(dotus);
  output_elbug_serial(0);
  wait(dotus);
}

void elbug_send_dah() {
  output_elbug_serial(1);
  //printf("_");    
  wait(dotus * 3);
  output_elbug_serial(0);
  wait(dotus);
}

int main(int argc, char *argv[]) {
  int c, status, err = 0;
  static int idlewait = 0;
  unsigned short int dcd, cts, dsr;
  
  signal(SIGINT,  handle_signal);
  signal(SIGQUIT, handle_signal);
  signal(SIGTERM, handle_signal);  
  signal(SIGKILL, handle_signal);    

  while ((c = getopt(argc, argv, "f:w:s:dio:b")) != -1) {
    switch (c) {
    case 'f':
      tone = strtoul(optarg, NULL, 0);
      if (tone && (tone < 50 || tone > 5000)) {
        printf("tone %d out of range 0 or 50...5000 Hz\n", tone);
        err++;
      }
      break;
    case 's':
      name_ptt = strdup(optarg);
      break;
    case 'd':
      pin = TIOCM_DTR;
      break;
    case 'i':
      invert_ptt = 1;
      break;
    case 'w':
      wpm = strtoul(optarg, NULL, 0);
      if (wpm < 3 || wpm > 90) {
        printf("Speed %d out of range 3...90 wpm\n", wpm);
        err++;
      }
      break;
    case 'o':
      name_spkr = strdup(optarg);
      break;
    case 'b':
      bug_mode = 1;
      break;
    case 'v':
      verbose = 1;
    default:
      err++;
      break;
    }
    if (err) {
      printf("/* dl4mge's elbug.*/\n"
             "usage: elbug [-w <speed wpm>] [-s <ptt comm port>] [-d] [-i] \n"
             "  -w: speed in wpm              (default: 12 words per minute)\n"
             "  -f: tone frequency in Hz      (default: 440 Hz, enter 0 for silent mode)\n"
             "  -s: serial port for in/output (default: /dev/ttyS0)\n"
             "  -d: output through DTR pin    (default is RTS)\n"
             "  -i: invert PTT                (default: PTT = positive)\n"
             "  -o: file/device for output    (default: %s)"     
             "  -b: bug mode - device generates dits and dahs already (only read CTS/pin 8\n"
             "%s", name_spkr, cable);
      exit (0);
	}
  }
  /*if ((err = IOPERM(port, 8, 1))) {
	printf("permission problem for serial port %04x: ioperm = %d\n", port, err);
	printf("This program has to be called with root permissions.\n");
    }*/
  if ((fd_ptt = open(name_ptt, O_RDWR, 0)) < 0) {
	printf("error in opening ptt device %s - maybe try another one?\n", 
           name_ptt);
    printf("Error: %s\n", strerror(fd_ptt));
  }
    
  dotus = 1000000 / (wpm * 50 / 60);
  if (tone) {
    argp = 1193180/tone;
  }
  /* from man console_ioctl */
    		
  printf("%s",cable);
  printf("\nelbug: %d wpm at %s, %s %s\n",
         wpm, name_ptt,
         invert_ptt ? "inverted ptt output," : "",
         pin == TIOCM_DTR ? "DTR output" : "RTS output");
  printf("A dit will last %d ms.\n", dotus/1000);
  printf("See options by elbug -h. Stop me by <strg>c.\n");
    
#ifdef KIOCSOUND
  if (tone > 0) {
    spkr = console_open();
  }
#else
  printf ("KIOCSOUND not found, no sound. Sorry.\n");
#endif        

  output_elbug_serial(0);

  if (bug_mode) {
    int col_counter = 0;
    int tick_counter = 0;
    int high = 0;
    int waiting = 0;
    printf("entering bug mode");
    
    for (;;) {
      status = 0;
      ioctl(fd_ptt, TIOCMGET, &status);
      cts = status & TIOCM_CTS;
      //printf("%s", cts? "*": ".");
      if (high) {
        if (cts) {
          verb("°");
        } 
        else { /* low edge */
          verb("\\");
          switch_tone(0);
          if (tick_counter < 1*dot_resolution) {
            // fast dit
            decode(DIT);
            /* TODO: inc WPM */
            verb("f");
          }
          else if (tick_counter < 2*dot_resolution) {
            // normal dit (1 or 1.5 dots)
            decode(DIT);
          }
          else if (tick_counter < 4.5*dot_resolution) {
            // normal dah (2 to 4 dots) 
            decode(DAH);
          }
          else {
            // slow dah
            decode(DAH);
            /* TODO; dec WPM */
            verb("s");
          }
          high = 0;
          tick_counter = 0;
        }
      } // if (high)
      else {
        // !high
        if (cts) {
          // high edge
          verb("/");
          switch_tone(1);
          if (2 == waiting) {
            printf("\n");
          }
          tick_counter = 0;
          waiting = 0;
          high = 1;
        } 
        else { /* staying low */
          verb("_");
          if (tick_counter < 2.5*dot_resolution) {
            // inter-character
            //nop
            ;           
          }
          else if (tick_counter < 3010*dot_resolution) {
            // inter-word space
            if (!waiting) {
              decode(PAUSE);
              decode(PAUSE);
            }
            waiting = 1;
          }
          else {
            // long space
            waiting = 2;
          }
        }
      } // if (high)
      //col_counter++;
      //printf(".");
      fflush(stdout);
      if (col_counter > 80) {
        printf("\n");
        col_counter = 0;
      }  
      wait(dotus/dot_resolution);
      tick_counter++;
    }
  }
  else {
    printf("entering key mode");
    /* main loop */
    for (;;) {
      status = 0;
      ioctl(fd_ptt, TIOCMGET, &status);
      cts = status & TIOCM_CTS;
      dcd = status & TIOCM_CAR;
      dsr = status & TIOCM_DSR;
      verb(" status: %x, cts: %x, dcd: %x, dsr: %x\n", status, cts, dcd, dsr);
      if (cts) {
        printf("dit ");
        //elbug_send_dit();
        //decode(DIT);
      }
      if (dcd | dsr) {
        printf("dah ");
        //elbug_send_dah();
        //decode(DAH);
      }
      /*wait(3000);
        idlewait += 3000;
        if (idlewait > dotus) {
        printf("_ ");
        idlewait = 0;
        decode(PAUSE);
        }*/
      wait (dotus/4);
    }
  }
    
}

/*
* Copyright (c) 1	987, 1993, 1994
*        The Regents of the University of California.  All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
* 3. All advertising materials mentioning features or use of this software
*    must display the following acknowledgement:
*        This product includes software developed by the University of
*        California, Berkeley and its contributors.
* 4. Neither the name of the University nor the names of its contributors
*    may be used to endorse or promote products derived from this software
*    without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
           * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*/

#if defined(LIBC_SCCS) && !defined(lint)
/* static char sccsid[] = "from: @(#)getopt.c        8.2 (Berkeley) 4/2/94"; */
static char *rcsid = "$Id: getopt.c,v 1.2 1998/01/21 22:27:05 billm Exp $";
#endif /* LIBC_SCCS and not lint */

#include "getopt.h"

int        opterr = 1,                /* if error message should be printed */
optind = 1,                /* index into parent argv vector */
optopt,                        /* character checked for validity */
optreset;                /* reset getopt */
char *optarg;                /* argument associated with option */

static const char *usage = "%s: -i <file> [-n]\n";
static char *program;

void print_usage() {
    printf(usage, program);
    printf("     -a : disable AU\n");
    printf("     -c : no check md5\n");
    printf("     -f <thread type> (1: frame, 2: slice, 4: frameslice)\n");
    printf("     -i <input file>\n");
    printf("     -n : no display\n");
    printf("     -o <output file>\n");
    printf("     -p <number of threads> \n");
    printf("     -t <temporal layer id>\n");
    printf("     -w : Do not apply cropping windows\n");
    printf("     -l <Quality layer id> \n");
    printf("     -s <num> Stop after num frames \n");
    printf("     -r <num> Frame rate (FPS) \n");
}

/*
 * getopt --
 *        Parse argc/argv argument vector.
 */
int getopt(int nargc, char * const *nargv, const char *ostr) {
    static char *place = EMSG;                /* option letter processing */
    char *oli;                                /* option letter list index */
    
    if (nargc == 1)
        return BADARG;
    
    if (optreset || !*place) {                /* update scanning pointer */
        optreset = 0;
        if (optind >= nargc || *(place = nargv[optind]) != '-') {
            place = EMSG;
            return (-1);
        }
        if (place[1] && *++place == '-') {        /* found "--" */
            ++optind;
            place = EMSG;
            return (-1);
        }
    }                                        /* option letter okay? */
    if ((optopt = (int)*place++) == (int)':' ||
        !(oli = strchr(ostr, optopt))) {
        /*
         * if the user didn't specify '-' as an option,
         * assume it means -1.
         */
        if (optopt == (int)'-')
            return (-1);
        if (!*place)
            ++optind;
        if (opterr && *ostr != ':')
            (void)fprintf(stderr,
                          "%s: illegal option -- %c\n", nargv[0], optopt);
        return (BADCH);
    }
    if (*++oli != ':') {                        /* don't need argument */
        optarg = NULL;
        if (!*place)
            ++optind;
    }
    else {                                        /* need an argument */
        if (*place)                        /* no white space */
            optarg = place;
        else if (nargc <= ++optind) {        /* no arg */
            place = EMSG;
            if (*ostr == ':')
                return (BADARG);
            if (opterr)
                (void)fprintf(stderr,
                              "%s: option requires an argument -- %c\n",
                              nargv[0], optopt);
            return (BADCH);
        }
        else                                /* white space */
            optarg = nargv[optind];
        place = EMSG;
        ++optind;
    }
    return (optopt);                        /* dump back option letter */
}

///////////////////////////////////////////////////////////////////////////////
// initializes APR and parses options
void init_main(int argc, char *argv[]) {
    // every command line option must be followed by ':' if it takes an
    // argument, and '::' if this argument is optional
    const char *ostr = "achi:no:p:f:s:t:wl:r:";

    int c;
    check_md5_flags   = ENABLE;
    thread_type       = 1;
    input_file        = NULL;
    display_flags     = ENABLE;
    output_file       = NULL;
    nb_pthreads       = 1;
    temporal_layer_id = 7;
    no_cropping       = DISABLE;
    quality_layer_id  = 0; // Base layer
    num_frames        = 0;
    frame_rate        = 0;

    program           = argv[0];
    
    c = getopt(argc, argv, ostr);
    
    while (c != -1) {
        switch (c) {
        case 'c':
            check_md5_flags = DISABLE;
            break;
        case 'f':
            thread_type = atoi(optarg);
            if (thread_type!=1 && thread_type!=2 && thread_type!=4) {
                print_usage();
                exit(1);
            }
            break;
        case 'i':
            input_file = strdup(optarg);
            break;
        case 'n':
            display_flags = DISABLE;
            break;
        case 'o':
            output_file = strdup(optarg);
            if(output_file[strlen(output_file)-4] == '.')
                output_file[strlen(output_file)-4] = '\0';
            break;
        case 'p':
            nb_pthreads = atoi(optarg);
            break;
        case 't':
            temporal_layer_id = atoi(optarg);
            break;
        case 'w':
            no_cropping = ENABLE;
            break;
        case 'l':
            quality_layer_id = atoi(optarg);
            break;
        case 's':
            num_frames = atoi(optarg);
            break;
        case 'r':
            frame_rate = atoi(optarg);
            break;
        default:
            print_usage();
            exit(1);
            break;
        }

        c = getopt(argc, argv, ostr);
    }
}

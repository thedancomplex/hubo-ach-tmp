/*
Copyright (c) 2012, Daniel M. Lofaro
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its contributors may 
      be used to endorse or promote products derived from this software 
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, 
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hubo.h"


// for ach
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>
#include <inttypes.h>
#include "ach.h"


#include <iostream>
#include <sstream>
#include <string>
using namespace std;


// ach message type
//typedef struct hubo h[1];

// ach channels
ach_channel_t chan_hubo_ref;      // hubo-ach
ach_channel_t chan_hubo_init_cmd; // hubo-ach-console
ach_channel_t chan_hubo_state;    // hubo-ach-state
//ach_channel_t chan_hubo_param;    // hubo-ach-param



static char** my_completion(const char*, int ,int);
char* my_generator(const char*,int);
char * dupstr (char*);
void *xmalloc (int);
void parse(char *s);
int test(char *s , struct hubo *h);
char* getArg(string s, int argNum);
void hubo_update(struct hubo_ref *h_ref, struct hubo_state *h_state);
int name2mot(char*s, struct hubo_param *h);
double hubo_get(char*s, struct hubo_ref *h, struct hubo_param *p);
void hubo_jmc_beep(struct hubo_param *h, struct hubo_init_cmd *c, char* buff);
void hubo_jmc_home(struct hubo_param *h, struct hubo_init_cmd *c, char* buff);
//char* cmd [] ={ "test","hello", "world", "hell" ,"word", "quit", " " };
void hubo_jmc_home_all(struct hubo_param *h, struct hubo_init_cmd *c, char* buff);
void setPosZeros();
//void setConsoleFlags();
int setDefaultValues(struct hubo_param *h);
char* cmd [] ={ "initialize","fet","initializeAll","homeAll",
                "ctrl","enczero", "goto","get","test","update", "quit","beep", "home"," "}; //,
/*
                "get RHY", "get RHR", "get RHP", "get RKN", "get RAP", "get RAR",
                "get LHY", "get LHR", "get LHP", "get LKN", "get LAP", "get LAR",
                "get RSP", "get RSR", "get RSY", "get REB", "get RWY", "get RWP",
                "get LSP", "get LSR", "get LSY", "get LEB", "get LWY", "get LWP",
                "get NKY", "get NK1", "get NK2", "get WST", "get RF1", "get RF2",
                "get RF3", "get RF4", "get RF5", "get LF1", "get LF2", "get LF3",
                "get LF4", "get LF5"};
 */

int main() {
        printf("\n");
        printf(" ***************** hubo-ach **************** \n");
        printf(" Support: Daniel M. Lofaro dan@danlofaro.com \n");
        printf(" ******************************************* \n");

        // get initial values for hubo
        // open ach channel
        int r = ach_open(&chan_hubo_ref, HUBO_CHAN_REF_NAME, NULL);
        assert( ACH_OK == r );

        // open hubo state
        r = ach_open(&chan_hubo_state, HUBO_CHAN_STATE_NAME, NULL);
        assert( ACH_OK == r );

        // initialize control channel
        r = ach_open(&chan_hubo_init_cmd, HUBO_CHAN_INIT_CMD_NAME, NULL);
        assert( ACH_OK == r );

        // get initial values for hubo
        struct hubo_ref H_ref;
        struct hubo_init_cmd H_init;
        struct hubo_state H_state;
        struct hubo_param H_param;
        memset( &H_ref,   0, sizeof(H_ref));
        memset( &H_init,  0, sizeof(H_init));
        memset( &H_state, 0, sizeof(H_state));
        memset( &H_param, 0, sizeof(H_param));
	
	usleep(250000);

	ach_put(&chan_hubo_ref, &H_ref, sizeof(H_ref));
        ach_put(&chan_hubo_init_cmd, &H_init, sizeof(H_init));
        ach_put(&chan_hubo_state, &H_state, sizeof(H_state));

	// set default values for H_ref in ach
	setPosZeros();

	// set default values for H_init in ach
	// this is not working. Not sure if it's really needed
	// since the structs get initialized with zeros when instantiated
//	setConsoleFlags();	

	// set default values for Hubo
	setDefaultValues(&H_param);

        char *buf;
        rl_attempted_completion_function = my_completion;
        printf("\n");
        while((buf = readline(">> hubo-ach: "))!=NULL) {
        //enable auto-complete
        rl_bind_key('\t',rl_complete);

//	printf("cmd [%s]\n",buf);
//	printf(">> ");
        printf("   ");

        /* get update after every command */
        hubo_update(&H_ref, &H_state);
	
	int tsleep = 0;
        char* buf0 = getArg(buf, 0);
        //printf(buf0);

        if (strcmp(buf0,"update")==0) {
                hubo_update(&H_ref, &H_state);
                printf("--->Hubo Information Updated\n");
        }
        else if (strcmp(buf0,"get")==0) {
                double jRef = hubo_get(buf,&H_ref, &H_param);
                char* tmp = getArg(buf,1);
                printf(">> %s = %f rad \n",tmp,jRef);
        }
        else if (strcmp(buf0,"beep")==0) {
                hubo_jmc_beep(&H_param, &H_init, buf);
        }
        else if (strcmp(buf0,"home")==0) {
                hubo_jmc_home(&H_param, &H_init, buf);
                printf("%s - Home \n",getArg(buf,1));
        }
        else if (strcmp(buf0,"homeAll")==0) {
                hubo_jmc_home_all(&H_param, &H_init, buf);
                printf("%s - Home All \n",getArg(buf,1));
		tsleep = 5;
		
        }
        else if (strcmp(buf0,"ctrl")==0) {
                int onOrOff = atof(getArg(buf,2));
                if(onOrOff == 0 | onOrOff == 1) {
                        H_init.cmd[0] = HUBO_CTRL_ON_OFF;
                        H_init.cmd[1] = name2mot(getArg(buf,1),&H_param);  // set motor num
                        H_init.cmd[2] = atof(getArg(buf,2));         // 1 = on, 0 = 0ff
                        r = ach_put( &chan_hubo_init_cmd, &H_init, sizeof(H_init) );
                        if(onOrOff == 0) {
                                printf("%s - Turning Off CTRL\n",getArg(buf,1));}
                        else {
                                printf("%s - Turning On CTRL\n",getArg(buf,1));}
                }

        }
        else if (strcmp(buf0,"fet")==0) {
                int onOrOff = atof(getArg(buf,2));
                if(onOrOff == 0 | onOrOff == 1) {
                        H_init.cmd[0] = HUBO_FET_ON_OFF;
                        H_init.cmd[1] = name2mot(getArg(buf,1),&H_param);  // set motor num
                        H_init.cmd[2] = atof(getArg(buf,2));		// 1 = on, 0 = 0ff
                        int r = ach_put( &chan_hubo_init_cmd, &H_init, sizeof(H_init) );
                        if(onOrOff == 0) {
                                printf("%s - Turning Off FET\n",getArg(buf,1));}
                        else {
                                printf("%s - Turning On FET\n",getArg(buf,1));}
                }
        }
        else if (strcmp(buf0,"initialize")==0) {
                H_init.cmd[0] = HUBO_JMC_INI;
                H_init.cmd[1] = name2mot(getArg(buf,1),&H_param);	// set motor num
                //C.val[0] = atof(getArg(buf,2));
                int r = ach_put( &chan_hubo_init_cmd, &H_init, sizeof(H_init) );
		printf("%s - Initialize \n",getArg(buf,1));
        }
        else if (strcmp(buf0,"initializeAll")==0) {
                H_init.cmd[0] = HUBO_JMC_INI_ALL;
                int r = ach_put( &chan_hubo_init_cmd, &H_init, sizeof(H_init) );
                printf("%s - Initialize All\n",getArg(buf,1));
		tsleep = 8;
        }
        /* Quit */
        else if (strcmp(buf0,"quit")==0)
                break;
        if (buf[0]!=0)
        add_history(buf);
	sleep(tsleep);	// sleep for tsleep sec
        }
        free(buf);
        return 0;
}

double hubo_get(char*s, struct hubo_ref *h, struct hubo_param *p) {

        /* get joint number */
        int jointNo = name2mot(getArg(s,1),p);

        return h->ref[jointNo];
}

void hubo_jmc_beep(struct hubo_param *h, struct hubo_init_cmd *c, char* buff) {
        /* make beep */
        c->cmd[0] = HUBO_JMC_BEEP;
        c->cmd[1] = name2mot(getArg(buff, 1), h);
        c->val[2] = atof(getArg(buff,2));
        int r = ach_put( &chan_hubo_init_cmd, c, sizeof(*c) );
        printf("send beep r = %i C = %i v = %f\n",r, c->cmd[0], c->val[0]);

}

void hubo_jmc_home(struct hubo_param *h, struct hubo_init_cmd *c, char* buff) {
        /* make beiep */
        c->cmd[0] = HUBO_GOTO_HOME;
        c->cmd[1] = name2mot(getArg(buff, 1), h);
        int r = ach_put( &chan_hubo_init_cmd, c, sizeof(*c) );
//	printf(">> Home %s \n",getArg(buff,1));
}

void hubo_jmc_home_all(struct hubo_param *h, struct hubo_init_cmd *c, char* buff) {
        /* make beiep */
        c->cmd[0] = HUBO_GOTO_HOME_ALL;
        int r = ach_put( &chan_hubo_init_cmd, c, sizeof(*c) );
//	printf(">> Home %s \n",getArg(buff,1));
}
void hubo_update(struct hubo_ref *h_ref, struct hubo_state *h_state) {
        size_t fs;
        int r = ach_get( &chan_hubo_ref, h_ref, sizeof(*h_ref), &fs, NULL, ACH_O_LAST );
        if((r == ACH_OK) | (r == ACH_MISSED_FRAME)) {
                assert( sizeof(*h_ref) == fs );}
        r = ach_get( &chan_hubo_state, h_state, sizeof(*h_state), &fs, NULL, ACH_O_LAST );
        if((r == ACH_OK) | (r == ACH_MISSED_FRAME)) {
                assert( sizeof(*h_state) == fs );}
        // look into posix message que
        // posix rt signal can give signal number and an interger
}


char* getArg(string s, int argNum) {
//	printf("\n dan test\n");

        istringstream iss(s);

        int i = 0;
//	char* theOut[];
        do
        {
                string sub;
                iss >> sub;
//              cout << "Substring: " << sub << endl;
                if( i == argNum ) {
                        return (char*)sub.c_str(); }
                i++;
        } while (iss);

        return NULL;
}

int test(char* s, struct hubo_param *h) {
        printf("\n dan test\n");

        char* mot = getArg(s,1);

        int tmp = name2mot(mot, h);
        printf("mot = %i \n",tmp);
        return 0;
}

int name2mot(char* name, struct hubo_param *h) {
        /* Returns the number of the requested joint */
        int i = 0;
        int iout = -1;
        for( i = 0; i < HUBO_JOINT_COUNT ; i++ ) {
                        char *mot = h->joint[i].name;
                        if (strcmp(name, mot) == 0) {
//				printf("i = %i\n", i);
                                iout = i;}
        }
	return iout;
}



static char** my_completion( const char * text , int start,  int end) {
        char **matches;

        matches = (char **)NULL;

        if (start == 0)
                matches = rl_completion_matches ((char*)text, &my_generator);
        else
                rl_bind_key('\t',rl_abort);

        return (matches);
}

char* my_generator(const char* text, int state) {
        static int list_index, len;
        char *name;

        if (!state) {
                list_index = 0;
                len = strlen (text);
        }

        while (name = cmd[list_index]) {
                list_index++;

        if (strncmp (name, text, len) == 0)
                return (dupstr(name));
        }

        /* If no names matched, then return NULL. */
        return ((char *)NULL);
}

char * dupstr (char* s) {
        char *r;

        r = (char*) xmalloc ((strlen (s) + 1));
        strcpy (r, s);
        return (r);
}

void * xmalloc (int size) {
        void *buf;

        buf = malloc (size);
        if (!buf) {
                fprintf (stderr, "Error: Out of memory. Exiting.'n");
                exit (1);
        }
        return buf;
}


void setPosZeros() {
        // open ach channel
//        int r = ach_open(&chan_num, "hubo", NULL);
//        assert( ACH_OK == r );

        struct hubo_ref H;
        memset( &H,   0, sizeof(H));
        size_t fs = 0;
        int r = ach_get( &chan_hubo_ref, &H, sizeof(H), &fs, NULL, ACH_O_LAST );
        assert( sizeof(H) == fs );

        int i = 0;
        for( i = 0; i < HUBO_JOINT_COUNT; i++) {
                H.ref[i] = 0.0;
        }
        ach_put(&chan_hubo_ref, &H, sizeof(H));
}

void setConsoleFlags() {
        struct hubo_init_cmd C;
        memset( &C,   0, sizeof(C));
        size_t fs =0;
        int r = ach_get( &chan_hubo_init_cmd, &C, sizeof(C), &fs, NULL, ACH_O_LAST );
	assert( sizeof(C) == fs );
	int i = 0;
        for( i = 0; i < HUBO_JOINT_COUNT; i++ ) {
                C.cmd[i] = 0;
                C.val[i] = 0;
        }
        r = ach_put(&chan_hubo_init_cmd, &C, sizeof(C));
	printf("finished setConsoleFlags\n");
}

int setDefaultValues(struct hubo_param *H) {

        FILE *ptr_file;

        // open file and if fails, return 1
        if (!(ptr_file=fopen("hubo-config.txt", "r")))
                return 1;

        struct hubo_joint_param tp;                     //instantiate hubo_jubo_param struct
        memset(&tp,      0, sizeof(tp));
      
	char active[6];
        char zeroed[6];
        int j;
	char buff[100];

	// read in each non-commented line of the config file corresponding to each joint
	while (fgets(buff, sizeof(buff), ptr_file) != NULL) {
        	sscanf(buff, "%hu%s%hu%u%hu%hu%hu%hu%hhu%hu%hhu%s%hhu%s\n", 
			&tp.jntNo,
			tp.name,
			&tp.motNo,  
			&tp.refEnc, 
			&tp.drive, 
			&tp.driven, 
			&tp.harmonic, 
			&tp.enc, 
			&tp.dir,  
			&tp.jmc, 
			&tp.can, 
			active, 
			&tp.numMot, 
			zeroed);

        	// define "true" and "false" strings as 1 and 0 for "active" struct member of tp
        	if (0 == strcmp(active, "true")) tp.active = 1;
        	else if (0 == strcmp(active, "false")) tp.active = 0;
        	else ;// bail out
        	
		// define "true" and "false" strings as 1 and 0 for "zeroed" struct member of tp
        	if (0 == strcmp(zeroed, "true")) tp.zeroed = 1;
        	else if (0 == strcmp(zeroed, "false")) tp.zeroed = 0;
        	else ;// bail out
		
		// define i to be the joint number
        	int i = (int)tp.jntNo;
		//copy contents (all member values) of tp into H.joint[] 
		//substruct which will populate its member variables
		memcpy(&(H->joint[i]), &tp, sizeof(tp));        
	}

	// close file stream
	fclose(ptr_file);
	
	// print struct member's values to console to check if it worked
/*	printf ("printout of setDefaultValues() function in hubo-console.c\n");
	size_t i;
	for (i = 0; i < HUBO_JOINT_COUNT; i++) {
		printf ("%hu\t%s\t%hu\t%u\t%hu\t%hu\t%hu\t%hu\t%hhu\t%hu\t%hhu\t%hhu\t%hhu\t%hhu\n", 
			H->joint[i].jntNo, 
			H->joint[i].name,
			H->joint[i].motNo, 
			H->joint[i].refEnc, 
			H->joint[i].drive, 
			H->joint[i].driven, 
			H->joint[i].harmonic, 
			H->joint[i].enc, 
			H->joint[i].dir, 
			H->joint[i].jmc, 
			H->joint[i].can, 
			H->joint[i].active, 
			H->joint[i].numMot, 
			H->joint[i].zeroed);
	}	
*/	return 0;
}


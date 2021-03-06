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
/* -*-	indent-tabs-mode:t; tab-width: 8; c-basic-offset: 8  -*- */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <string.h>
#include <stdio.h>

// for timer
#include <time.h>
#include <sched.h>
#include <sys/io.h>
#include <unistd.h>

// for RT
#include <stdlib.h>
#include <sys/mman.h>

// for hubo
#include "hubo.h"


// Check out which CAN API to use
#ifdef HUBO_CONFIG_ESD
#include "hubo/hubo-esdcan.h"
#else
#include "hubo/hubo-socketcan.h"
#endif

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


/* At time of writing, these constants are not defined in the headers */
#ifndef PF_CAN
#define PF_CAN 29
#endif

#ifndef AF_CAN
#define AF_CAN PF_CAN
#endif

/* ... */

/* Somewhere in your app */

// Priority
#define MY_PRIORITY (49)/* we use 49 as the PRREMPT_RT use 50
			    as the priority of kernel tasklets
			    and interrupt handler by default */

#define MAX_SAFE_STACK (1024*1024) /* The maximum stack size which is
				   guaranteed safe to access without
				   faulting */


// Timing info
#define NSEC_PER_SEC    1000000000

#define hubo_home_noRef_delay 6.0	// delay before trajectories can be sent while homeing in sec


/* functions */
void stack_prefault(void);
static inline void tsnorm(struct timespec *ts);
void getMotorPosFrame(int motor, struct can_frame *frame);
void setEncRef(int jnt, struct hubo_ref *r, struct hubo_param *h);
void setEncRefAll( struct hubo_ref *r, struct hubo_param *h);
void fSetEncRef(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f);
void fResetEncoderToZero(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f);
void fSetBeep(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f, double beepTime);
void fGetCurrentValue(int jnt, struct hubo_param *h, struct can_frame *f);
void hSetBeep(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f, double beepTime);
void fGetBoardStatusAndErrorFlags(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f);
void fInitializeBoard(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f);
void fEnableMotorDriver(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f);
void fDisableMotorDriver(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f);
void fEnableFeedbackController(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f);
void fDisableFeedbackController(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f);
void fSetPositionController(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f);
void fGotoLimitAndGoOffset(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f);
void hInitilizeBoard(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f);
void hSetEncRef(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f);
void hSetEncRefAll(struct hubo_ref *r, struct hubo_param *h, struct can_frame *f);
void hIniAll(struct hubo_ref *r, struct hubo_param *h, struct hubo_state *s, struct can_frame *f);
void huboLoop(struct hubo_param *H_param);
void hMotorDriverOnOff(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f, int onOff);
void hFeedbackControllerOnOff(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f, int onOff);
void hResetEncoderToZero(int jnt, struct hubo_ref *r, struct hubo_param *h, struct hubo_state *s, struct can_frame *f);
void huboConsole(struct hubo_ref *r, struct hubo_param *h, struct hubo_state *s, struct hubo_init_cmd *c, struct can_frame *f);
void hGotoLimitAndGoOffset(int jnt, struct hubo_ref *r, struct hubo_param *h, struct hubo_state *s, struct can_frame *f);
uint32_t getEncRef(int jnt, struct hubo_ref *r , struct hubo_param *h);
void hInitializeBoard(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f);
int decodeFrame(struct hubo_state *s, struct hubo_param *h, struct can_frame *f);
double enc2rad(int jnt, int enc, struct hubo_param *h);
void hGetEncValue(int jnt, struct hubo_param *h, struct can_frame *f);
void getEncAll(struct hubo_state *s, struct hubo_param *h, struct can_frame *f);
void getEncAllSlow(struct hubo_state *s, struct hubo_param *h, struct can_frame *f);
void getCurrentAll(struct hubo_state *s, struct hubo_param *h, struct can_frame *f);
void getCurrentAllSlow(struct hubo_state *s, struct hubo_param *h, struct can_frame *f);
void hGetCurrentValue(int jnt, struct hubo_param *h, struct can_frame *f);
void setRefAll(struct hubo_ref *r, struct hubo_param *h, struct hubo_state *s, struct can_frame *f);
void hGotoLimitAndGoOffsetAll(struct hubo_ref *r, struct hubo_param *h, struct hubo_state *s, struct can_frame *f);
void hInitializeBoardAll(struct hubo_ref *r, struct hubo_param *h, struct hubo_state *s, struct can_frame *f);
void setPosZeros();
//void setConsoleFlags();
int setDefaultValues(struct hubo_param *H);

// ach message type
//typedef struct hubo h[1];

// ach channels
ach_channel_t chan_hubo_ref;	  // hubo-ach
ach_channel_t chan_hubo_init_cmd; // hubo-ach-console
ach_channel_t chan_hubo_state;    // hubo-ach-state

//int hubo_ver_can = 0;
int hubo_debug = 0;
/* time for the ref not to be sent while a joint is being moved */
//double hubo_noRefTime[HUBO_JOINT_NUM];
double hubo_noRefTimeAll = 0.0;

void huboLoop(struct hubo_param *H_param) {
	int i = 0;  // iterator
	// get initial values for hubo
	struct hubo_ref H_ref;
	struct hubo_init_cmd H_init;
	struct hubo_state H_state;
	memset( &H_ref,   0, sizeof(H_ref));
	memset( &H_init,  0, sizeof(H_init));
	memset( &H_state, 0, sizeof(H_state));
	//memset(&hubo_noRef, 0, sizeof(hubo_noRef));

	size_t fs;
	int r = ach_get( &chan_hubo_ref, &H_ref, sizeof(H_ref), &fs, NULL, ACH_O_LAST );
	if(ACH_OK != r) {printf("Ref r = %s\n",ach_result_to_string(r));}
	assert( sizeof(H_ref) == fs );
	r = ach_get( &chan_hubo_init_cmd, &H_init, sizeof(H_init), &fs, NULL, ACH_O_LAST );
	if(ACH_OK != r) {printf("CMD r = %s\n",ach_result_to_string(r));}
	assert( sizeof(H_init) == fs );
	r = ach_get( &chan_hubo_state, &H_state, sizeof(H_state), &fs, NULL, ACH_O_LAST );
	if(ACH_OK != r) {printf("State r = %s\n",ach_result_to_string(r));}
	assert( sizeof(H_state) == fs );
	
	// put back on channels
	ach_put(&chan_hubo_ref, &H_ref, sizeof(H_ref));
	ach_put(&chan_hubo_init_cmd, &H_init, sizeof(H_init));
	ach_put(&chan_hubo_state, &H_state, sizeof(H_state));
//	ach_put( &chan_hubo_ref, &H, sizeof(H));

/* period */
//	int interval = 500000000; // 2hz (0.5 sec)
//	int interval = 20000000; // 50 hz (0.02 sec)
	int interval = 10000000; // 100 hz (0.01 sec)
//	int interval = 5000000; // 200 hz (0.005 sec)
//	int interval = 2000000; // 500 hz (0.002 sec)

	double T = (double)interval/(double)NSEC_PER_SEC; // 100 hz (0.01 sec)
	printf("T = %1.3f sec\n",T);

	/* Send a message to the CAN bus */
   	struct can_frame frame;

	// time info
	struct timespec t;

	// get current time
	//clock_gettime( CLOCK_MONOTONIC,&t);
	clock_gettime( 0,&t);

	sprintf( frame.data, "1234578" );
	frame.can_dlc = strlen( frame.data );

	int a = 0;

	printf("Start Hubo Loop\n");
	while(1) {
		fs = 0;
		// wait until next shot
		clock_nanosleep(0,TIMER_ABSTIME,&t, NULL);

		/* Get latest ACH message */
		/*
		r = ach_get( &chan_hubo_ref, &H_ref, sizeof(H_ref), &fs, NULL, ACH_O_LAST );
		assert( ACH_OK == r || ACH_MISSED_FRAME == r || ACH_STALE_FRAMES == r );
		assert( ACH_STALE_FRAMES == r || sizeof(H_ref) == fs );
		*/
		// assert( sizeof(H_param) == fs );

		r = ach_get( &chan_hubo_ref, &H_ref, sizeof(H_ref), &fs, NULL, ACH_O_LAST );
		if(ACH_OK != r) {
				if(hubo_debug) {
					printf("Ref r = %s\n",ach_result_to_string(r));}
			}
		else{	assert( sizeof(H_ref) == fs ); }

/*
		r = ach_get( &chan_hubo_init_cmd, &H_init, sizeof(H_init), &fs, NULL, ACH_O_LAST );
		if(ACH_OK != r) {
				if(hubo_debug) {
					printf("CMD r = %s\n",ach_result_to_string(r));}
				}
		else{ assert( sizeof(H_init) == fs ); }
*/

		r = ach_get( &chan_hubo_state, &H_state, sizeof(H_state), &fs, NULL, ACH_O_LAST );
		if(ACH_OK != r) {
				if(hubo_debug) {
					printf("State r = %s\n",ach_result_to_string(r));}
				}
		else{ assert( sizeof(H_state) == fs ); }

		/* read hubo console */
		huboConsole(&H_ref, H_param, &H_state, &H_init, &frame);
		/* set reference for zeroed joints only */
//		for(i = 0; i < HUBO_JOINT_COUNT; i++) {
//			if(H_param->joint[i].zeroed == true) {
//				hSetEncRef(H_param->joint[i].jntNo, &H_ref, H_param, &frame);
//			}
//		}

		/* Set all Ref */
		if(hubo_noRefTimeAll < T ) {
			setRefAll(&H_ref, H_param, &H_state, &frame);
		}
		else{
			hubo_noRefTimeAll = hubo_noRefTimeAll - T;
		}
		
		/* Get all Encoder data */
		getEncAllSlow(&H_state, H_param, &frame); 

		/* Get all Current data */
//		getCurrentAllSlow(&H_state, &H_param, &frame);
		
//		hGetCurrentValue(RSY, &H_param, &frame);
//		readCan(hubo_socket[H_param.joint[RSY].can], &frame, HUBO_CAN_TIMEOUT_DEFAULT);
//		decodeFrame(&H_state, &H_param, &frame);

		/* put data back in ACH channel */
		ach_put( &chan_hubo_state, &H_state, sizeof(H_state));
		t.tv_nsec+=interval;
		tsnorm(&t);
	}


}






void stack_prefault(void) {
	unsigned char dummy[MAX_SAFE_STACK];
	memset( dummy, 0, MAX_SAFE_STACK );
}



static inline void tsnorm(struct timespec *ts){

//	clock_nanosleep( NSEC_PER_SEC, TIMER_ABSTIME, ts, NULL);
	// calculates the next shot
	while (ts->tv_nsec >= NSEC_PER_SEC) {
		//usleep(100);	// sleep for 100us (1us = 1/1,000,000 sec)
		ts->tv_nsec -= NSEC_PER_SEC;
		ts->tv_sec++;
	}
}


/*
uint32_t getEncRef(int jnt, struct hubo *h)
{
	//return (uint32_t)((double)h->joint[jnt].drive/(double)h->joint[jnt].driven/(double)h->joint[jnt].harmonic/(double)h->joint[jnt].enc*2.0*pi);
	return (uint32_t)((double)h->joint[jnt].drive/(double)h->joint[jnt].driven/(double)h->joint[jnt].harmonic/(double)h->joint[jnt].ref*2.0*pi);
}
*/
void setRefAll(struct hubo_ref *r, struct hubo_param *h, struct hubo_state *s, struct can_frame *f) {
	///> Requests all encoder and records to hubo_state
	int c[HUBO_JMC_COUNT];
	memset( &c, 0, sizeof(c));
	int jmc = 0;
	int i = 0;
	int canChan = 0;
	
	for( canChan = 0; canChan < HUBO_CAN_CHAN_NUM; canChan++) {
		for( i = 0; i < HUBO_JOINT_COUNT; i++ ) {
			jmc = h->joint[i].jmc+1;
			if((0 == c[jmc]) & (canChan == h->joint[i].can) & (s->joint[i].active == true)){	// check to see if already asked that motor controller
				hSetEncRef(i, r, h, f);
				c[jmc] = 1;
//				if(i == RHY){ printf(".%d %d %d %d",jmc,h->joint[RHY].can, canChan, c[jmc]); }
			}

		}
	}	
}




void getEncAll(struct hubo_state *s, struct hubo_param *h, struct can_frame *f) {
	///> Requests all encoder and records to hubo_state
	char c[HUBO_JMC_COUNT];
	memset( &c, 0, sizeof(c));
	//memset( &c, 1, sizeof(c));
	int jmc = 0;
	int i = 0;
//	c[h->joint[REB].jmc] = 0;
	int canChan = 0;
	for( canChan = 0; canChan < HUBO_CAN_CHAN_NUM; canChan++) {
	for( i = 0; i < HUBO_JOINT_COUNT; i++ ) {
		jmc = h->joint[i].jmc;
		if((0 == c[jmc]) & (canChan == h->joint[i].can)){	// check to see if already asked that motor controller
			hGetEncValue(i, h, f);
//			readCan(hubo_socket[h->joint[i].can], f, HUBO_CAN_TIMEOUT_DEFAULT);
//			decodeFrame(s, h, f);
			c[jmc] = 1;
		}
	}}	
	memset( &c, 0, sizeof(c));
	for( i = 0; i < HUBO_JOINT_COUNT; i++ ) {
		jmc = h->joint[i].jmc;
		if((0 == c[jmc]) & (canChan == h->joint[i].can)){	// check to see if already asked that motor controller
			readCan(hubo_socket[h->joint[i].can], f, HUBO_CAN_TIMEOUT_DEFAULT);
			decodeFrame(s, h, f);
			c[jmc] = 1;
		}
	}

}


void getEncAllSlow(struct hubo_state *s, struct hubo_param *h, struct can_frame *f) {
	///> Requests all encoder and records to hubo_state
	char c[HUBO_JMC_COUNT];
	memset( &c, 0, sizeof(c));
	//memset( &c, 1, sizeof(c));
	int jmc = 0;
	int i = 0;
//	c[h->joint[REB].jmc] = 0;
	int canChan = 0;
	for( canChan = 0; canChan < HUBO_CAN_CHAN_NUM; canChan++) {
	for( i = 0; i < HUBO_JOINT_COUNT; i++ ) {
		jmc = h->joint[i].jmc;
		if((0 == c[jmc]) & (canChan == h->joint[i].can)){	// check to see if already asked that motor controller
			hGetEncValue(i, h, f);
			readCan(hubo_socket[h->joint[i].can], f, HUBO_CAN_TIMEOUT_DEFAULT);
			decodeFrame(s, h, f);
			c[jmc] = 1;
		}
	}}	
}




void getCurrentAll(struct hubo_state *s, struct hubo_param *h, struct can_frame *f) {
	///> Requests all motor currents and records to hubo_state
	char c[HUBO_JMC_COUNT];
	memset( &c, 0, sizeof(c));
	//memset( &c, 1, sizeof(c));
	int jmc = 0;
	int i = 0;
//	c[h->joint[REB].jmc] = 0;
	int canChan = 0;
	for( canChan = 0; canChan < HUBO_CAN_CHAN_NUM; canChan++) {
	for( i = 0; i < HUBO_JOINT_COUNT; i++ ) {
		jmc = h->joint[i].jmc;
		if(0 == c[jmc]){	// check to see if already asked that motor controller
			hGetCurrentValue(i, h, f);
//			readCan(hubo_socket[h->joint[i].can], f, HUBO_CAN_TIMEOUT_DEFAULT);
//			decodeFrame(s, h, f);
			c[jmc] = 1;
		}
	}}	
	memset( &c, 0, sizeof(c));
	for( i = 0; i < HUBO_JOINT_COUNT; i++ ) {
		jmc = h->joint[i].jmc;
		if(0 == c[jmc]){	// check to see if already asked that motor controller
			readCan(hubo_socket[h->joint[i].can], f, HUBO_CAN_TIMEOUT_DEFAULT);
			decodeFrame(s, h, f);
			c[jmc] = 1;
		}
	}

}



void getCurrentAllSlow(struct hubo_state *s, struct hubo_param *h, struct can_frame *f) {
	///> Requests all motor currents and records to hubo_state
	char c[HUBO_JMC_COUNT];
	memset( &c, 0, sizeof(c));
	//memset( &c, 1, sizeof(c));
	int jmc = 0;
	int i = 0;
//	c[h->joint[REB].jmc] = 0;
	int canChan = 0;
	for( canChan = 0; canChan < HUBO_CAN_CHAN_NUM; canChan++) {
	for( i = 0; i < HUBO_JOINT_COUNT; i++ ) {
		jmc = h->joint[i].jmc;
		if(0 == c[jmc]){	// check to see if already asked that motor controller
			hGetCurrentValue(i, h, f);
			readCan(hubo_socket[h->joint[i].can], f, HUBO_CAN_TIMEOUT_DEFAULT);
			decodeFrame(s, h, f);
			c[jmc] = 1;
		}
	}}	

}










uint32_t getEncRef(int jnt, struct hubo_ref *r , struct hubo_param *h) {
	// set encoder from reference
	struct hubo_joint_param *p = &h->joint[jnt];
	return (uint32_t)((double)p->driven/(double)p->drive*(double)p->harmonic*(double)p->enc*(double)r->ref[jnt]/2.0/pi);
}

unsigned long signConvention(long _input) {
	if (_input < 0) return (unsigned long)( ((-_input)&0x007FFFFF) | (1<<23) );
	else return (unsigned long)_input;
}
void fSetEncRef(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f) {
	// set ref
	f->can_id 	= REF_BASE_TXDF + h->joint[jnt].jmc;  //CMD_TXD;F// Set ID
	__u8 data[6];
	uint16_t jmc = h->joint[jnt].jmc;
	if(h->joint[jnt].numMot == 2) {
		__u8 m0 = h->driver[jmc].jmc[0];
		__u8 m1 = h->driver[jmc].jmc[1];
//		printf("m0 = %i, m1= %i \n",m0, m1);


		unsigned long pos0 = signConvention((int)getEncRef(m0, r, h));
		unsigned long pos1 = signConvention((int)getEncRef(m1, r, h));
		f->data[0] =    (uint8_t)(pos0		& 0x000000FF);
		f->data[1] = 	(uint8_t)((pos0>>8) 	& 0x000000FF);
		f->data[2] = 	(uint8_t)((pos0>>16)	& 0x000000FF);
		f->data[3] =    (uint8_t)(pos1 		& 0x000000FF);
		f->data[4] = 	(uint8_t)((pos1>>8) 	& 0x000000FF);
		f->data[5] = 	(uint8_t)((pos1>>16)	& 0x000000FF);

		if(r->ref[m0] < 0.0) {
			f->data[2] = f->data[2] | 0x80;
		}
		if(r->ref[m1] < 0.0) {
			f->data[5] = f->data[5] | 0x80;
		}
	}
	else if(h->joint[jnt].numMot == 1) {
		__u8 m0 = h->driver[jmc].jmc[0];
		__u8 m1 = h->driver[jmc].jmc[0];
//		printf("m0 = %i, m1= %i \n",m0, m1);


		unsigned long pos0 = signConvention((int)getEncRef(m0, r, h));
		unsigned long pos1 = signConvention((int)getEncRef(m1, r, h));
		f->data[0] =    (uint8_t)(pos0		& 0x000000FF);
		f->data[1] = 	(uint8_t)((pos0>>8) 	& 0x000000FF);
		f->data[2] = 	(uint8_t)((pos0>>16)	& 0x000000FF);
		f->data[3] =    (uint8_t)(pos1 		& 0x000000FF);
		f->data[4] = 	(uint8_t)((pos1>>8) 	& 0x000000FF);
		f->data[5] = 	(uint8_t)((pos1>>16)	& 0x000000FF);

		if(r->ref[m0] < 0.0) {
			f->data[2] = f->data[2] | 0x80;
		}
		if(r->ref[m1] < 0.0) {
			f->data[5] = f->data[5] | 0x80;
		}
	}
	f->can_dlc = 6; //= strlen( data );	// Set DLC
}


// 5
void fResetEncoderToZero(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f) {
	/* Reset Encoder to Zero (REZ: 0x06)
	CH: Channel No.
	CH= 0 ~ 4 according to the board.
	CH= 0xF selects ALL Channel
	Action:
	1. Set encoder(s) to Zero.
	2. Initialize internal parameters.
	3. Reset Fault and Error Flags.
	*/

	f->can_id 	= CMD_TXDF;	// Set ID
	__u8 data[3];
	f->data[0] 	= h->joint[jnt].jmc;
	f->data[1]		= EncZero;
	f->data[2] 	= h->joint[jnt].motNo;
	//sprintf(f->data, "%s", data);
	f->can_dlc = 3; //= strlen( data );	// Set DLC
}
// 4
void fGetCurrentValue(int jnt, struct hubo_param *h, struct can_frame *f) {
	// get the value of the current in 10mA units A = V/100
	f->can_id 	= CMD_TXDF;	// Set ID
	__u8 data[2];
	f->data[0] 	= h->joint[jnt].jmc;
	f->data[1]		= SendCurrent;
	f->can_dlc = 2; //= strlen( data );	// Set DLC
}

// 4
void fSetBeep(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f, double beepTime) {
	f->can_id 	= CMD_TXDF;	// Set ID
	__u8 data[3];
	f->data[0] 	= h->joint[jnt].jmc;	// BNO
	f->data[1]	= 0x82;			// alarm
	f->data[2]	= (uint8_t)floor(beepTime/0.1);
	f->can_dlc = 3; //= strlen( data );	// Set DLC
}

// 28
void fInitializeBoard(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f) {
	f->can_id 	= CMD_TXDF;
	__u8 data[2];
	f->data[0] 	= h->joint[jnt].jmc;
//	printf("jmc = %i\n",data[0]);
	//data[0] 	= (uint8_t)88;
	f->data[1] 	= 0xFA;
	//sprintf(f->data, "%s", data);
	f->can_dlc = 2;
}

// 10.1
void fEnableMotorDriver(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f) {
	f->can_id 	= CMD_TXDF;
	__u8 data[3];
	f->data[0] 	= (uint8_t)h->joint[jnt].jmc;
	f->data[1] 	= 0x0B;
	f->data[2] 	= 0x01;
	//sprintf(f->data, "%s", data);
	f->can_dlc = 3;
}

// 10.2
void fDisableMotorDriver(int jnt, struct hubo_ref *r, struct hubo_param *h,  struct can_frame *f) {
	f->can_id 	= CMD_TXDF;
	__u8 data[3];
	f->data[0] 	= (uint8_t)h->joint[jnt].jmc;
	f->data[1] 	= 0x0B;
	f->data[2] 	= 0x01;
	//sprintf(f->data, "%s", data);
	f->can_dlc = 3;
}

// 13
void fEnableFeedbackController(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f) {
	f->can_id 	= CMD_TXDF;
	__u8 data[2];
	f->data[0] 	= (uint8_t)h->joint[jnt].jmc;
	f->data[1] 	= 0x0E;
	//sprintf(f->data, "%s", data);
	f->can_dlc = 2;
}

// 14
void fDisableFeedbackController(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f) {
	f->can_id 	= CMD_TXDF;
	__u8 data[2];
	f->data[0] 	= (uint8_t)h->joint[jnt].jmc;
	f->data[1] 	= 0x0F;
	//sprintf(f->data, "%s", data);
	f->can_dlc = 2;
}

// 15
void fSetPositionController(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f) {
	f->can_id 	= CMD_TXDF;
	__u8 data[3];
	f->data[0] 	= (uint8_t)h->joint[jnt].jmc;
	f->data[1] 	= 0x10;
	f->data[2]		= 0x00;	// position control
	//sprintf(f->data, "%s", data);
	f->can_dlc = 3;
}

void fGetEncValue(int jnt, struct hubo_param *h, struct can_frame *f) { ///> make can frame for getting the value of the Encoder
	f->can_id       = CMD_TXDF;     // Set ID
	 __u8 data[3];
	f->data[0]      = h->joint[jnt].jmc;
	f->data[1]              = SendEncoder;
	f->data[2]              = 0x00;
	f->can_dlc = 3; //= strlen( data );     // Set DLC
}

void hGetEncValue(int jnt, struct hubo_param *h, struct can_frame *f) { ///> make can frame for getting the value of the Encoder
	fGetEncValue( jnt, h, f);
	sendCan(hubo_socket[h->joint[jnt].can], f);
}

void hGetCurrentValue(int jnt, struct hubo_param *h, struct can_frame *f) { ///> make can frame for getting the motor current in amps (10mA resolution)
	fGetCurrentValue( jnt, h, f);
	sendCan(hubo_socket[h->joint[jnt].can], f);
}


void hSetBeep(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f, double beepTime) {
	fSetBeep(jnt, r, h, f, beepTime);
	f->can_dlc = 3;
}

// 16  home
void fGotoLimitAndGoOffset(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f) {
	f->can_id 	= CMD_TXDF;
	__u8 data[8];
	f->data[0] 	= (uint8_t)h->joint[jnt].jmc;
	f->data[1] 	= 0x11;
	f->data[2] 	= (((uint8_t)h->joint[jnt].motNo+1) << 4)|2; // set /DT high
	f->data[3]  	= 0x00;
	f->data[4]  	= 0x00;
	f->data[5]  	= 0x00;
	f->data[6]  	= 0x00;
	f->data[7]  	= 0x00;
	//sprintf(f->data, "%s", data);
	f->can_dlc = 8;
//	printf("go home %i\n", ((uint8_t)h->joint[jnt].motNo << 4)|2);
}

void hGotoLimitAndGoOffset(int jnt, struct hubo_ref *r, struct hubo_param *h, struct hubo_state *s, struct can_frame *f) {
	fGotoLimitAndGoOffset(jnt, r, h, f);
	sendCan(hubo_socket[h->joint[jnt].can], f);
	r->ref[jnt] = 0;
	s->joint[jnt].zeroed = true;
	
	hubo_noRefTimeAll = hubo_home_noRef_delay;
}

void hGotoLimitAndGoOffsetAll(struct hubo_ref *r, struct hubo_param *h, struct hubo_state *s, struct can_frame *f) {
	int i = 0;
	for(i = 0; i < HUBO_JOINT_COUNT; i++) {
		if(s->joint[i].active == true) {
			hGotoLimitAndGoOffset(i, r, h, s, f);
		}
	}
}

void hInitializeBoard(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f) {
	fInitializeBoard(jnt, r, h, f);
	sendCan(hubo_socket[h->joint[jnt].can], f);
	//readCan(hubo_socket[h->joint[jnt].can], f, 4);	// 8 bytes to read and 4 sec timeout
	readCan(hubo_socket[h->joint[jnt].can], f, HUBO_CAN_TIMEOUT_DEFAULT*100);	// 8 bytes to read and 4 sec timeout
}


void hInitializeBoardAll(struct hubo_ref *r, struct hubo_param *h, struct hubo_state *s, struct can_frame *f) {
	///> Initilizes all boards
	int i = 0;
	for(i = 0; i < HUBO_JOINT_COUNT; i++) {
		if(s->joint[i].active == true) {
			hInitializeBoard(i, r, h, f);
		}
	}
}

void hSetEncRef(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f) {
//	setEncRef(jnt,r, h);
	fSetEncRef(jnt, r, h, f);
	sendCan(hubo_socket[h->joint[jnt].can], f);
//	readCan(h->socket[h->joint[jnt].can], f, 4);	// 8 bytes to read and 4 sec timeout
}

void hIniAll(struct hubo_ref *r, struct hubo_param *h, struct hubo_state *s, struct can_frame *f) {
// --std=c99
		printf("2\n");
	int i = 0;
	for( i = 0; i < HUBO_JOINT_COUNT; i++ ) {
		if(s->joint[i].active) {
			hInitializeBoard(i, r, h, f);
			printf("%i\n",i);
		}
	}
}

void hMotorDriverOnOff(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f, int onOff) {
	if(onOff == 1) { // turn on FET
		fEnableMotorDriver(jnt,r, h, f);
		sendCan(hubo_socket[h->joint[jnt].can], f); }
	else if(onOff == 0) { // turn off FET
		fDisableMotorDriver(jnt,r, h, f);
		sendCan(hubo_socket[h->joint[jnt].can], f); }
}

void hFeedbackControllerOnOff(int jnt, struct hubo_ref *r, struct hubo_param *h, struct can_frame *f, int onOff) {
	if(onOff == 1) { // turn on FET
		fEnableFeedbackController(jnt,r, h, f);
		sendCan(hubo_socket[h->joint[jnt].can], f); }
	else if(onOff == 0) { // turn off FET
		fDisableFeedbackController(jnt,r, h, f);
		sendCan(hubo_socket[h->joint[jnt].can], f); }
}

void hResetEncoderToZero(int jnt, struct hubo_ref *r, struct hubo_param *h, struct hubo_state *s, struct can_frame *f) {
	fResetEncoderToZero(jnt,r, h, f);
	sendCan(hubo_socket[h->joint[jnt].can], f);
	s->joint[jnt].zeroed == true;		// need to add a can read back to confirm it was zeroed
}
void huboConsole(struct hubo_ref *r, struct hubo_param *h, struct hubo_state *s, struct hubo_init_cmd *c, struct can_frame *f) {
	/* gui for controling basic features of the hubo  */
//	printf("hubo-ach - interface 2012-08-18\n");
	size_t fs;
	int status = 0;
	while ( status == 0 | status == ACH_OK | status == ACH_MISSED_FRAME ) {
		/* get oldest ach message */
		//status = ach_get( &chan_hubo_ref, &c, sizeof(c), &fs, NULL, 0 );
		//status = ach_get( &chan_hubo_init_cmd, c, sizeof(*c), &fs, NULL, ACH_O_LAST );
		status = ach_get( &chan_hubo_init_cmd, c, sizeof(*c), &fs, NULL, 0 );
	//printf("here2 h = %f status = %i c = %d v = %f\n", h->joint[0].ref, status,(uint16_t)c->cmd[0], c->val[0]);
//	printf("here2 h = %f status = %i c = %d v = %f\n", h->joint[0].ref, status,(uint16_t)c->cmd[0], c->val[0]);
		if( status == ACH_STALE_FRAMES) {
			break; }
		else {
		//assert( sizeof(c) == fs );

//		if ( status != 0 & status != ACH_OK & status != ACH_MISSED_FRAME ) {
//			break; }
			switch (c->cmd[0]) {
				case HUBO_JMC_INI_ALL:
					hInitializeBoardAll(r,h,s,f);
					break;
				case HUBO_JMC_INI:
					hInitializeBoard(c->cmd[1],r,h,f);
					break;
				case HUBO_FET_ON_OFF:
//					printf("fet on\n");
					hMotorDriverOnOff(c->cmd[1],r,h,f,c->cmd[2]);
					break;
				case HUBO_CTRL_ON_OFF:
					hFeedbackControllerOnOff(c->cmd[1],r,h,f,c->cmd[2]);
					break;
				case HUBO_ZERO_ENC:
					hResetEncoderToZero(c->cmd[1],r,h,s,f);
					break;
				case HUBO_JMC_BEEP:
					hSetBeep(c->cmd[1],r,h,f,c->val[0]);
					break;
				case HUBO_GOTO_HOME_ALL:
					hGotoLimitAndGoOffsetAll(r,h,s,f);
					break;
				case HUBO_GOTO_HOME:
					hGotoLimitAndGoOffset(c->cmd[1],r,h,s,f);
					break;
		//		case HUBO_GOTO_REF:
				default:
					break;
			}
		}
//		printf("c = %i\n",c->cmd[0]);
	}
}


double enc2rad(int jnt, int enc, struct hubo_param *h) {
	struct hubo_joint_param *p = &h->joint[jnt];
        return (double)(enc*(double)p->drive/(double)p->driven/(double)p->harmonic/(double)p->enc*2.0*pi);
}


int decodeFrame(struct hubo_state *s, struct hubo_param *h, struct can_frame *f) {
	int fs = (int)f->can_id;
	
	/* Current and Temp */
	if( (fs >= SETTING_BASE_RXDF) & (fs < (SETTING_BASE_RXDF+0x60))) {
		int jmc = fs-SETTING_BASE_RXDF;		// find the jmc value
		int i = 0;
		int jnt0 = h->driver[jmc].jmc[0];     // jmc number
		int motNo = h->joint[jnt0].numMot;     // motor number   
		double current = 0;				// motor current
		double temp = 0;			// temperature
		if(motNo == 2 | motNo == 1) {
			for( i = 0; i < motNo; i++) {
				current = f->data[0+i*1];
				current = current/100.0;
				temp = f->data[3];
				temp = temp/100.0;
				int jnt = h->driver[jmc].jmc[i];          // motor on the same drive
				s->joint[jnt].cur = current;
				s->joint[jnt].tmp = temp;

				
			}
		}


	}
	/* Return Motor Position */
	else if( (fs >= ENC_BASE_RXDF) & (fs < CUR_BASE_RXDF) ) {
		int jmc = fs-ENC_BASE_RXDF;
		int i = 0;
		int jnt0 = h->driver[jmc].jmc[0];     // jmc number
		int motNo = h->joint[jnt0].numMot;     // motor number   
		int32_t enc = 0;
		int16_t enc16 = 0;			// encoder value for neck and fingers
		if(motNo == 2 | motNo == 1){
			for( i = 0; i < motNo; i++ ) {
				enc = 0;
				enc = (enc << 8) + f->data[3 + i*4];
				enc = (enc << 8) + f->data[2 + i*4];
				enc = (enc << 8) + f->data[1 + i*4];
				enc = (enc << 8) + f->data[0 + i*4];
				int jnt = h->driver[jmc].jmc[i];          // motor on the same drive
				s->joint[jnt].pos =  enc2rad(jnt,enc, h);
			}
		}

		else if( motNo == 3 ) { 	// neck
			for( i = 0; i < motNo; i++ ) {
				enc16 = 0;
				enc16 = (enc << 8) + f->data[1 + i*4];
				enc16 = (enc << 8) + f->data[0 + i*4];
				int jnt = h->driver[jmc].jmc[i];          // motor on the same drive
				s->joint[jnt].pos =  enc2rad(jnt,enc16, h);
			}
		}
			
		else if( motNo == 5 & f->can_dlc == 6 ) {
			for( i = 0; i < 3 ; i++ ) {
				enc16 = 0;
				enc16 = (enc << 8) + f->data[1 + i*4];
				enc16 = (enc << 8) + f->data[0 + i*4];
				int jnt = h->driver[jmc].jmc[i];          // motor on the same drive
				s->joint[jnt].pos =  enc2rad(jnt,enc16, h);
			}
		}
			
		else if( motNo == 5 & f->can_dlc == 4 ) {
			for( i = 0; i < 2; i++ ) {
				enc16 = 0;
				enc16 = (enc << 8) + f->data[1 + i*4];
				enc16 = (enc << 8) + f->data[0 + i*4];
				int jnt = h->driver[jmc].jmc[i];          // motor on the same drive
				s->joint[jnt].pos =  enc2rad(jnt,enc16, h);
			}
		}
	
	}
	return 0;
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
	memset(&tp, 0, sizeof(tp));
       
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
/*	printf ("printout of setDefaultValues() function in hubo-main.c\n");
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

int main(int argc, char **argv) {

	struct hubo_ref H_ref;
        struct hubo_init_cmd H_init;
        struct hubo_state H_state;
        struct hubo_param H_param;
        memset( &H_ref,   0, sizeof(H_ref));
        memset( &H_init,  0, sizeof(H_init));
        memset( &H_state, 0, sizeof(H_state));
        memset( &H_param, 0, sizeof(H_param));

	int vflag = 0;
	int c;

	int i = 1;
	while(argc > i) {
		if(strcmp(argv[i], "-d") == 0) {
			hubo_debug = 1;
		}
		i++;
	}

	// RT
	struct sched_param param;
	/* Declare ourself as a real time task */

	param.sched_priority = MY_PRIORITY;
	if(sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
		perror("sched_setscheduler failed");
		exit(-1);
	}

	/* Lock memory */

	if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
		perror("mlockall failed");
		exit(-2);
	}

	/* Pre-fault our stack */
	stack_prefault();


	// open hubo reference
	int r = ach_open(&chan_hubo_ref, HUBO_CHAN_REF_NAME, NULL);
	assert( ACH_OK == r );

	// open hubo state
	r = ach_open(&chan_hubo_state, HUBO_CHAN_STATE_NAME, NULL);
	assert( ACH_OK == r );

	// initilize control channel
	r = ach_open(&chan_hubo_init_cmd, HUBO_CHAN_INIT_CMD_NAME, NULL);
	assert( ACH_OK == r );

	openAllCAN( vflag );
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

	// run hubo main loop
	huboLoop(&H_param);

	return 0;
}

/*******************************************************************************#
#	    guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#										#
# This program is free software; you can redistribute it and/or modify         	#
# it under the terms of the GNU General Public License as published by   	#
# the Free Software Foundation; either version 2 of the License, or           	#
# (at your option) any later version.                                          	#
#                                                                              	#
# This program is distributed in the hope that it will be useful,              	#
# but WITHOUT ANY WARRANTY; without even the implied warranty of             	#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  		#
# GNU General Public License for more details.                                 	#
#                                                                              	#
# You should have received a copy of the GNU General Public License           	#
# along with this program; if not, write to the Free Software                  	#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA	#
#                                                                              	#
********************************************************************************/

#include "globals.h"
#include "avilib.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/types.h>
#include <linux/videodev.h>
#include <string.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>


int initGlobals (struct GLOBAL *global) {
	
    	global->debug = DEBUG;
    
	if((global->videodevice = (char *) calloc(1, 16 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->videodevice\n");
		goto error;
	}
	
	snprintf(global->videodevice, 15, "/dev/video0");

	if((global->confPath = (char *) calloc(1, 80 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->confPath\n");
		goto error;
	}
	snprintf(global->confPath, 79, "%s/.guvcviewrc",getenv("HOME"));
	
	if((global->aviFPath = (pchar *) calloc(1, 2 * sizeof(pchar)))==NULL){
		printf("couldn't calloc memory for:global->aviFPath\n");
		goto error;
	}
	if((global->imgFPath = (pchar *) calloc(1, 2 * sizeof(pchar)))==NULL){
		printf("couldn't calloc memory for:global->imgFPath\n");
		goto error;
	}
	if((global->profile_FPath = (pchar *) calloc(1, 2 * sizeof(pchar)))==NULL){
		printf("couldn't calloc memory for:global->profile_FPath\n");
		goto error;
	}
	
	if((global->aviFPath[1] = (char *) calloc(1, 100 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->aviFPath[1]\n");
		goto error;
	}
	snprintf(global->aviFPath[1], 2, "~");
	
	if((global->imgFPath[1] = (char *) calloc(1, 100 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->imgFPath[1]\n");
		goto error;
	}
	snprintf(global->imgFPath[1], 99, "%s",getenv("HOME"));
	
	if((global->imgFPath[0] = (char *) calloc(1, 20 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->imageName\n");
		goto error;
	}
	snprintf(global->imgFPath[0],10,DEFAULT_IMAGE_FNAME);
	
	if((global->aviFPath[0] = (char *) calloc(1, 20 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->aviFPath[0]\n");
		goto error;
	}
	snprintf(global->aviFPath[0],12,DEFAULT_AVI_FNAME);
   
	//~ if((global->sndfile= (char *) calloc(1, 32 * sizeof(char)))==NULL){
		//~ printf("couldn't calloc memory for:global->sndfile\n");
		//~ goto error;
	//~ }
	
	//~ snprintf(global->sndfile,32,"/tmp/guvc_sound_XXXXXX");/*template for temp sound file name*/
	
	if((global->profile_FPath[1] = (char *) calloc(1, 100 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->profile_FPath[1]\n");
		goto error;
	}
	snprintf(global->profile_FPath[1], 100, "%s",getenv("HOME"));
	
	if((global->profile_FPath[0] = (char *) calloc(1, 20 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->profile_FPath[0]\n");
		goto error;
	}
	snprintf(global->profile_FPath[0], 20, "default.gpfl");
	
	
	if((global->WVcaption= (char *) calloc(1, 32 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->WVcaption\n");
		goto error;
	}
	snprintf(global->WVcaption,10,"GUVCVIdeo");
	
   	global->stack_size=TSTACK;
	
	global->image_inc=0;
   
	if((global->imageinc_str= (char *) calloc(1, 25 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->imageinc_str\n");
		goto error;
	}
	snprintf(global->imageinc_str,20,_("File num:%d"),global->image_inc);
	
	global->vid_sleep=0;
	global->avifile=NULL; /*avi filename passed through argument options with -n */
	global->Capture_time=0; /*avi capture time passed through argument options with -t */
	global->lprofile=0; /* flag for -l command line option*/
	global->imgFormat=0; /* 0 -JPG 1-BMP 2-PNG*/
	global->AVIFormat=0; /*0-"MJPG"  1-"YUY2" 2-"DIB "(rgb32)*/ 
	global->AVI_MAX_LEN=AVI_MAX_SIZE; /* ~2 Gb*/    
	global->snd_begintime=0;/*begin time for audio capture*/
	global->currtime=0;
	global->lasttime=0;
	global->AVIstarttime=0;
	global->AVIstoptime=0;
	global->framecount=0;
	global->frmrate=5;
	global->Sound_enable=1; /*Enable Sound by Default*/
	global->Sound_SampRate=SAMPLE_RATE;
	global->Sound_SampRateInd=0;
	global->Sound_numInputDev=0;
	global->Sound_Format=WAVE_FORMAT_PCM;
	global->Sound_DefDev=0; 
	global->Sound_UseDev=0;
	global->Sound_NumChan=NUM_CHANNELS;
	global->Sound_NumChanInd=0;
	global->Sound_NumSec=NUM_SECONDS;
	
	global->FpsCount=0;
    	
	global->timer_id=0;
	global->image_timer_id=0;
	global->image_timer=0;
	global->image_npics=999;/*default max number of captures*/
	global->frmCount=0;
	global->PanStep=2;/*2 degree step for Pan*/
	global->TiltStep=2;/*2 degree step for Tilt*/
	global->DispFps=0;
	global->fps = DEFAULT_FPS;
	global->fps_num = DEFAULT_FPS_NUM;
	global->bpp = 0; //current bytes per pixel
	global->hwaccel = 1; //use hardware acceleration
	global->grabmethod = 1;//default mmap(1) or read(0)
	global->width = DEFAULT_WIDTH;
	global->height = DEFAULT_HEIGHT;
	global->winwidth=WINSIZEX;
	global->winheight=WINSIZEY;
    	global->spinbehave=0;
	global->boxvsize=0;
	if((global->mode = (char *) calloc(1, 5 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->mode\n");
		goto error;
	}
	snprintf(global->mode, 4, "jpg");
	global->format = V4L2_PIX_FMT_MJPEG;
	global->formind = 0; /*0-MJPG 1-YUYV*/
	global->Frame_Flags = YUV_NOFILT;
	global->jpeg=NULL;
   	global->jpeg_size = 0;
	/* reset with videoIn parameters */
	global->jpeg_bufsize = 0;
	global->autofocus = 0;
	global->AFcontrol = 0;
	return (0);
	
error:
	return(-1); /*no mem should exit*/

}

int closeGlobals(struct GLOBAL *global){
	if (global->videodevice) free(global->videodevice);
	if (global->confPath) free(global->confPath);
	if (global->aviFPath[1]) free(global->aviFPath[1]);
	if (global->imgFPath[1]) free(global->imgFPath[1]);
	if (global->imgFPath[0]) free(global->imgFPath[0]);
	if (global->aviFPath[0]) free(global->aviFPath[0]);
	if (global->profile_FPath[1]) free(global->profile_FPath[1]);
	if (global->profile_FPath[0]) free(global->profile_FPath[0]);
	if (global->aviFPath) free(global->aviFPath);
	if (global->imgFPath) free(global->imgFPath);
	if (global->profile_FPath) free(global->profile_FPath);
	if (global->WVcaption) free (global->WVcaption);
	if (global->imageinc_str) free(global->imageinc_str);
	if (global->avifile) free (global->avifile);
	if (global->mode) free(global->mode);
	global->videodevice=NULL;
	global->confPath=NULL;
	global->avifile=NULL;
	global->mode=NULL;
	if(global->jpeg) free (global->jpeg);
	global->jpeg=NULL;
	free(global);
	return (0);
}
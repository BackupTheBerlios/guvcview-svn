/*******************************************************************************#
#           guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#                                                                               #
# This program is free software; you can redistribute it and/or modify          #
# it under the terms of the GNU General Public License as published by          #
# the Free Software Foundation; either version 2 of the License, or             #
# (at your option) any later version.                                           #
#                                                                               #
# This program is distributed in the hope that it will be useful,               #
# but WITHOUT ANY WARRANTY; without even the implied warranty of                #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 #
# GNU General Public License for more details.                                  #
#                                                                               #
# You should have received a copy of the GNU General Public License             #
# along with this program; if not, write to the Free Software                   #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA     #
#                                                                               #
********************************************************************************/

#ifndef GLOBALS_H
#define GLOBALS_H

#include <glib.h>
#include "defs.h"
#include "utils.h"

typedef struct _sndDev 
{
	int id;
	int chan;
	int samprate;
	//PaTime Hlatency;
	//PaTime Llatency;
} sndDev;

/*global variables used in guvcview*/
struct GLOBAL 
{
	GMutex *mutex;         //global structure mutex
	char *videodevice;     // video device (def. /dev/video0)
	char *confPath;        //configuration file path
	char *avifile;         //avi filename passed through argument options with -n
	char *WVcaption;       //video preview title bar caption
	char *imageinc_str;    //label for File inc
	char *aviinc_str;      //label for File inc
	char *mode;            //mjpg (default)
	pchar* aviFPath;       //avi path [0] - filename  [1] - dir
	pchar* imgFPath;       //image path [0] - filename  [1] - dir
	pchar* profile_FPath;  //profile path [0] - filename  [1] - dir

	sndDev *Sound_IndexDev;//list of sound input devices
	BYTE *jpeg;            // jpeg buffer

	ULONG AVI_MAX_LEN;     //avi max length
	DWORD snd_begintime;   //begin time for audio capture
	DWORD currtime;
	DWORD lasttime;
	DWORD AVIstarttime;    //avi start time
	DWORD AVIstoptime;     //avi stop time
	DWORD avi_inc;         //avi name increment
	DWORD framecount;      //avi frame count
	DWORD frmCount;        //frame count for fps display calc
	DWORD image_inc;       //image name increment
	DWORD jpeg_bufsize;    // width*height/2 

	int stack_size;        //thread stack size
	int vid_sleep;         //video thread sleep time (0 by default)
	int Capture_time;      //avi capture time passed through argument options with -t 
	int imgFormat;         //image format: 0-"jpg", 1-"png", 2-"bmp"
	int AVIFormat;         //0-"MJPG"  1-"YUY2" 2-"DIB "(rgb32)
	int Sound_SampRate;    //audio sample rate
	int Sound_SampRateInd; //audio sample rate combo index
	int Sound_numInputDev; //number of audio input devices
	int Sound_DefDev;      //audio default device index
	int Sound_UseDev;      //audio used device index
	int Sound_NumChan;     //audio number of channels
	int Sound_NumChanInd;  //audio number of channels combo index
	int Sound_NumSec;      //number of audio seconds to save into avi on iteration (def. 1)
	int Sound_Format;      //audio format (mpeg2 or 16 bit PCM)
	int Sound_bitRate;     //bit rate for mpeg audio compression
	int PanStep;           //step angle for Pan
	int TiltStep;          //step angle for Tilt
	int FpsCount;          //frames counter for fps calc
	int timer_id;          //fps count timer
	int image_timer_id;    //auto image capture timer
	int image_timer;       //auto image capture time
	int image_npics;       //number of captures
	int fps;               //fps denominator
	int fps_num;           //fps numerator (usually 1)
	int bpp;               //current bytes per pixel
	int hwaccel;           //use hardware acceleration
	int grabmethod;        //default mmap(1) or read(0)[not implemented]
	int width;             //frame width
	int height;            //frame height
	int winwidth;          //control windoe width
	int winheight;         //control window height
	int boxvsize;          //size of vertical spliter
	int spinbehave;        //spin: 0-non editable 1-editable
	int format;            //v4l2 pixel format
	int Frame_Flags;       //frame filter flags
	int jpeg_size;         //jpeg buffer size

	float DispFps;         //fps value
	
	gboolean Sound_enable; //Enable/disable Sound (Def. enable)
	gboolean AFcontrol;    //Autofocus control flag (exists or not)
	gboolean autofocus;    //autofocus flag (enable/disable)
	gboolean flg_config;   //flag confPath if set in args
	gboolean lprofile;     //flag for command line -l option
	gboolean flg_npics;    //flag npics if set in args
	gboolean flg_hwaccel;  //flag hwaccel if set in args
	gboolean flg_res;      //flag resol if set in args
	gboolean flg_mode;     //flag mode if set in args
	gboolean flg_imgFPath; //flag imgFPath if set in args
	gboolean flg_FpsCount; //flag FpsCount if set in args
	gboolean debug;        //debug mode flag (--verbose)
	gboolean AVIButtPress;
	gboolean control_only; //if set don't stream video (enables image control in other apps e.g. ekiga, skype, mplayer)
};

/*----------------------------- prototypes ------------------------------------*/
int initGlobals(struct GLOBAL *global);

int closeGlobals(struct GLOBAL *global);


#endif


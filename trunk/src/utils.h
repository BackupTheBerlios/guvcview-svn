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

/*******************************************************************************#
#                                                                               #
#  MJpeg decoding and frame capture taken from luvcview                         #
#                                                                               # 
#                                                                               #
********************************************************************************/

#ifndef UTILS_H
#define UTILS_H

#include "defs.h"
/*------------- portaudio defs ----------------*/
/*---- can be override in rc file or GUI ------*/

#define SAMPLE_RATE  (0) /* 0 device default*/
//#define FRAMES_PER_BUFFER (4096)

#define NUM_SECONDS     (1) /* captures 1 second bloks */
/* sound can go for more 1 seconds than video          */

#define NUM_CHANNELS    (0) /* 0-device default 1-mono 2-stereo */

//#define DITHER_FLAG     (paDitherOff) 
#define DITHER_FLAG     (0) 

#define AUDIO_I16        (1)/*AUDIO_F32 AUDIO_I32 AUDIO_I16 AUDIO_I8 AUDIO_U8*/
/*select sample format*/

#ifdef AUDIO_F32

#define PA_SAMPLE_TYPE  paFloat32
#define PA_FOURCC       WAVE_FORMAT_IEEE_FLOAT
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define MAX_SAMPLE (1.0f)
#define PRINTF_S_FORMAT "%.8f"

#else
#ifdef AUDIO_I32

#define PA_SAMPLE_TYPE  paInt32
#define PA_FOURCC       WAVE_FORMAT_PCM
typedef int SAMPLE;
#define SAMPLE_SILENCE  (0)
#define MAX_SAMPLE (0x7FFFFFFF)
#define PRINTF_S_FORMAT "%d"

#else
#ifdef AUDIO_I16

#define PA_SAMPLE_TYPE  paInt16
#define PA_FOURCC       WAVE_FORMAT_PCM
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#define MAX_SAMPLE (0x7FFF)
#define PRINTF_S_FORMAT "%d"

#else
#ifdef AUDIO_I8

#define PA_SAMPLE_TYPE  paInt8
typedef char SAMPLE;
#define SAMPLE_SILENCE  (0)
#define MAX_SAMPLE (0x7F)
#define PRINTF_S_FORMAT "%d"

#else 
#ifdef AUDIO_U8

#define PA_SAMPLE_TYPE  paUInt8
typedef unsigned char SAMPLE;
#define SAMPLE_SILENCE  (128)
#define MAX_SAMPLE (0xFF)
#define PRINTF_S_FORMAT "%d"

#endif
#endif
#endif
#endif
#endif
/*video defs*/
//#define BI_RGB 0;
//#define BI_RLE4 1;
//#define BI_RLE8 2;
//#define BI_BITFIELDS 3;

/* Fixed point arithmetic */
//#define FIXED	Sint32
//#define FIXED_BITS 16
//#define TO_FIXED(X) (((Sint32)(X))<<(FIXED_BITS))
//#define FROM_FIXED(X) (((Sint32)(X))>>(FIXED_BITS))

/*                      */
#define ERR_NO_SOI 1
#define ERR_NOT_8BIT 2
#define ERR_HEIGHT_MISMATCH 3
#define ERR_WIDTH_MISMATCH 4
#define ERR_BAD_WIDTH_OR_HEIGHT 5
#define ERR_TOO_MANY_COMPPS 6
#define ERR_ILLEGAL_HV 7
#define ERR_QUANT_TABLE_SELECTOR 8
#define ERR_NOT_YCBCR_221111 9
#define ERR_UNKNOWN_CID_IN_SCAN 10
#define ERR_NOT_SEQUENTIAL_DCT 11
#define ERR_WRONG_MARKER 12
#define ERR_NO_EOI 13
#define ERR_BAD_TABLES 14
#define ERR_DEPTH_MISMATCH 15


int jpeg_decode(unsigned char **pic, unsigned char *buf, int *width, int *height);

void cleanBuff(BYTE* Buff,int size);

#endif


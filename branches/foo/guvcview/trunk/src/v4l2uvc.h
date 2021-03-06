/*******************************************************************************
#	    guvcview              http://berlios																				#
#     Paulo Assis <pj.assis@gmail.com>																		#
#																																		#
# This program is free software; you can redistribute it and/or modify         				#
# it under the terms of the GNU General Public License as published by         				#
# the Free Software Foundation; either version 2 of the License, or            					#
# (at your option) any later version.                                          										#
#                                                                              														#
# This program is distributed in the hope that it will be useful,              						#
# but WITHOUT ANY WARRANTY; without even the implied warranty of               		#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                		#
# GNU General Public License for more details.                                 							#
#                                                                              														#
# You should have received a copy of the GNU General Public License            			#
# along with this program; if not, write to the Free Software                  						#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    			#
#                                                                              														#
*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <linux/videodev.h>
#include <gtk/gtk.h>

#define NB_BUFFER 4
#define DHT_SIZE 432

#define DEFAULT_WIDTH 320
#define DEFAULT_HEIGHT 240

#define DEFAULT_IMAGE_FNAME "Image.bmp"
#define DEFAULT_AVI_FNAME	"capture.avi"
#define DEFAULT_FPS	25

/*
 * Private V4L2 control identifiers from UVC driver.  - this seems to change acording to driver version
 * all other User-class control IDs are defined by V4L2 (videodev.h)
 */

#define V4L2_CID_BACKLIGHT_COMPENSATION		(V4L2_CID_PRIVATE_BASE+0)
#define V4L2_CID_POWER_LINE_FREQUENCY		(V4L2_CID_PRIVATE_BASE+1)
#define V4L2_CID_SHARPNESS			(V4L2_CID_PRIVATE_BASE+2)
#define V4L2_CID_HUE_AUTO			(V4L2_CID_PRIVATE_BASE+3)

#define V4L2_CID_FOCUS_AUTO			(V4L2_CID_PRIVATE_BASE+4)
#define V4L2_CID_FOCUS_ABSOLUTE			(V4L2_CID_PRIVATE_BASE+5)
#define V4L2_CID_FOCUS_RELATIVE			(V4L2_CID_PRIVATE_BASE+6)

#define V4L2_CID_PAN_RELATIVE			(V4L2_CID_PRIVATE_BASE+7)
#define V4L2_CID_TILT_RELATIVE			(V4L2_CID_PRIVATE_BASE+8)
#define V4L2_CID_PANTILT_RESET			(V4L2_CID_PRIVATE_BASE+9)

#define V4L2_CID_EXPOSURE_AUTO			(V4L2_CID_PRIVATE_BASE+10)
#define V4L2_CID_EXPOSURE_ABSOLUTE		(V4L2_CID_PRIVATE_BASE+11)

#define V4L2_CID_WHITE_BALANCE_TEMPERATURE_AUTO	(V4L2_CID_PRIVATE_BASE+12)
#define V4L2_CID_WHITE_BALANCE_TEMPERATURE	(V4L2_CID_PRIVATE_BASE+13)

#define V4L2_CID_PRIVATE_LAST			V4L2_CID_WHITE_BALANCE_TEMPERATURE





struct vdIn {
    int fd;
	char *videodevice;
    char *status;
    char *pictName;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers rb;
	struct v4l2_timecode timecode;
    void *mem[NB_BUFFER];
    unsigned char *tmpbuffer;
    unsigned char *framebuffer;
    int isstreaming;
    int grabmethod;
    int width;
    int height;
    int formatIn;
    int formatOut;
    int framesizeIn;
    int signalquit;
    int capAVI;
	char *AVIFName;
	int fps;
    int getPict;
	int capImage;
	char *ImageFName;
    int rawFrameCapture;
    /* raw frame capture */
    unsigned int fileCounter;
    /* raw frame stream capture */
    unsigned int rfsFramesWritten;
    unsigned int rfsBytesWritten;
    /* raw stream capture */
    FILE *captureFile;
    unsigned int framesWritten;
    unsigned int bytesWritten;
	struct v4l2_streamparm streamparm;
};

typedef enum {
    INPUT_CONTROL_TYPE_INTEGER = 1,
    INPUT_CONTROL_TYPE_BOOLEAN = 2,
    INPUT_CONTROL_TYPE_MENU = 3,
    INPUT_CONTROL_TYPE_BUTTON = 4,
} InputControlType;

typedef struct _InputControl {
    unsigned int i;
    unsigned int id;
    InputControlType type;
    char * name;
    int min, max, step, default_val, enabled;
    char ** entries;
} InputControl;

typedef struct _ControlInfo {
    GtkWidget * widget;
    GtkWidget * label;
    GtkWidget * labelval;
    unsigned int idx;
    int maxchars;
} ControlInfo;

typedef struct _VidState {
    
    GtkWidget * table;
    GtkWidget * device_combo;
    GtkWidget * device_label;
    GtkWidget * input_combo;
    GtkWidget * input_label;
    
    int width_req;
    int height_req;

    InputControl * control;
    int num_controls;
    ControlInfo * control_info;
} VidState;

int
init_videoIn(struct vdIn *vd, char *device, int width, int height,
	     int format, int grabmethod);
//~ int change_format(struct vdIn *vd);
int uvcGrab(struct vdIn *vd);
void close_v4l2(struct vdIn *vd);
	
//~ int v4l2ResetControl(struct vdIn *vd, int control);
//~ int v4l2ResetPanTilt(struct vdIn *vd,int pantilt);
//~ int v4L2UpDownPan(struct vdIn *vd, short inc);
//~ int v4L2UpDownTilt(struct vdIn *vd,short inc);

int input_get_control (struct vdIn * device, InputControl * control, int * val);
int input_set_control (struct vdIn * device, InputControl * control, int val);
int input_set_framerate (struct vdIn * device, int val);
int input_get_framerate (struct vdIn * device);
InputControl *
input_enum_controls (struct vdIn * device, int * num_controls);


void
input_free_controls (InputControl * control, int num_controls);

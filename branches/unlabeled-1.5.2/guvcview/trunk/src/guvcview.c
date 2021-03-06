/*************************************************************************************************
#	    guvcview              http://guvcview.berlios.de												#
#     Paulo Assis <pj.assis@gmail.com>																#
#																														#
# This program is free software; you can redistribute it and/or modify         				#
# it under the terms of the GNU General Public License as published by   				#
# the Free Software Foundation; either version 2 of the License, or           				#
# (at your option) any later version.                                          								#
#                                                                              										#
# This program is distributed in the hope that it will be useful,              					#
# but WITHOUT ANY WARRANTY; without even the implied warranty of             		#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  		#
# GNU General Public License for more details.                                 					#
#                                                                              										#
# You should have received a copy of the GNU General Public License           		#
# along with this program; if not, write to the Free Software                  					#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA		#
#                                                                              										#
*************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <pthread.h>
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include <SDL/SDL_audio.h>
#include <SDL/SDL_timer.h>
#include <linux/videodev.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <SDL/SDL_syswm.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "v4l2uvc.h"
#include "avilib.h"


static const char version[] = VERSION;
struct vdIn *videoIn;
char confPath[80];
int AVIFormat=1; /*1-"YUY2"  2-"DIB "(rgb32) */
VidState * s;

/* The main window*/
GtkWidget *mainwin;
/* A restart Dialog */
GtkWidget *restartdialog;
 
int ARG_C;
char **ARG_V;

/*the main SDL surface*/
SDL_Surface *pscreen = NULL;

SDL_Surface *ImageSurf = NULL;
SDL_Overlay *overlay;
SDL_Rect drect;
SDL_Event sdlevent;
SDL_Thread *mythread;
Uint32 currtime;
Uint32 lasttime;
Uint32 AVIstarttime;
Uint32 AVIstoptime;
Uint32 framecount=0;
avi_t *AviOut;
unsigned char *p = NULL;
char * pim;
//Uint32 * aviIm;
//unsigned char *pixeldata = (unsigned char *)&aviIm;

unsigned char frmrate;

char *capt;

int fps = DEFAULT_FPS;
int bpp = 0; //current bytes per pixel
int hwaccel = 1; //use hardware acceleration
int grabmethod = 1;//default mmap(1) or read(0)
int width = DEFAULT_WIDTH;
int height = DEFAULT_HEIGHT;
int winwidth=WINSIZEX;
int winheight=WINSIZEY;
const char *mode="jpg"; /*jpg (default) or yuv*/
int format = V4L2_PIX_FMT_MJPEG;

/*currently it as no use - all variables are global*/
//~ struct pt_data {
    //~ SDL_Surface **ptscreen;
	//~ SDL_Overlay **ptoverlay;
    //~ SDL_Event *ptsdlevent;
    //~ SDL_Rect *drect;
    //~ struct vdIn *ptvideoIn;
    //~ unsigned char frmrate;
//~ } ptdata;


static Uint32 SDL_VIDEO_Flags =
    SDL_ANYFORMAT | SDL_DOUBLEBUF | SDL_RESIZABLE;
    
gint 
shutd (gint restart) 
{
 	int exec_status=0;
	int *tstatus;
	int i;
    printf("Shuting Down Thread\n");
	videoIn->signalquit=0;
	printf("waiting for thread to finish\n");
	SDL_WaitThread(mythread, tstatus);//wait for thread to finish
	printf("Thread terminated with satus=%i\n",*tstatus);
	/*It seems there is a race condition causing the call to SDL_Quit to sometimes segfault  */
	/*Thread signals exit before it actualy closes, ?some sort off buffering?                */
	/*will put SDL_quit some lines down and wait some ms just in case                        */
	SDL_Delay(SDL_WAIT_TIME); 
		
	gtk_window_get_size(mainwin,&winwidth,&winheight);//mainwin or widget
	
	
	close_v4l2(videoIn);
	close(videoIn->fd);
	printf("closed strutures\n");
    free(videoIn);
	free(capt);
	
	SDL_Quit();
	printf("SDL Quit\n");
	printf("cleaned allocations - 50%\n");
	gtk_main_quit();
	
    printf("GTK quit\n");
    writeConf(confPath);
	input_free_controls (s->control, s->num_controls);
	printf("free controls - vidState\n");
	
	char locpath[30];
	char fullpath[50];
	char *pwd;
	
 	if (restart==1) {
		 		 		
		 if (sscanf(ARG_V[0],"./%s",locpath)==1) { //guvcview started from local path
		 	pwd=getenv("PWD");
			sprintf(fullpath,"%s/%s",pwd,locpath);
			if((ARG_V[0]=realloc(ARG_V[0],(strlen(fullpath)+1)*sizeof(char)))!=NULL){
				strcpy(ARG_V[0],fullpath);
			} else {
				printf("Couldn't realloc mem (terminating..)\n");
				for(i=0;i<ARG_C;i++){
					free(ARG_V[i]);
				}
				free(ARG_V);
				printf("cleaned allocations - 100%\n");
				return(2);
			}
		 } 
		 
		 printf("restarting guvcview\n");
		 exec_status = execvp(ARG_V[0], ARG_V);
 	}
	
	for(i=0;i<ARG_C;i++){
		free(ARG_V[i]);
	}
	free(ARG_V);
	printf("cleanig allocations - 100%\n");
	return exec_status;
}


/* Event handlers */
gint
delete_event (GtkWidget *widget, GdkEventConfigure *event)
{
	shutd(0);//shutDown
	
	return 0;
}

void
aviClose (void)
{
  double tottime = -1;
  if (AviOut)
    {
      printf ("AVI close asked \n");
      tottime = AVIstoptime - AVIstarttime;
	  printf("AVI: %i in %x\n",framecount,tottime);
      
	  if (tottime > 0) {
	    /*try to find the real frame rate*/
		AviOut->fps = (double) framecount *1000 / tottime;
	  }
      else {
		/*set the hardware frame rate*/   
		AviOut->fps=videoIn->fps;
	  }
	  
      AVI_close (AviOut);
      printf ("close avi\n");
      AviOut = NULL;
	  framecount = 0;
    }

}

static int
num_chars (int n)
{
    int i = 0;

    if (n <= 0) {
        i++;
        n = -n;
    }

    while (n != 0) {
        n /= 10;
        i++;
    }
    return i;
}

double
ms_time (void)
{
  static struct timeval tod;
  gettimeofday (&tod, NULL);
  return ((double) tod.tv_sec * 1000.0 + (double) tod.tv_usec / 1000.0);

}


/*Controls*/
static void
set_slider_label (GtkRange * range)
{
    ControlInfo * ci = g_object_get_data (G_OBJECT (range), "control_info");
    if (ci->labelval) {
        char str[12];
        sprintf (str, "%*d", ci->maxchars, (int) gtk_range_get_value (range));
        gtk_label_set_text (GTK_LABEL (ci->labelval), str);
    }
}

static void
slider_changed (GtkRange * range, VidState * s)
{
  
    ControlInfo * ci = g_object_get_data (G_OBJECT (range), "control_info");
    InputControl * c = s->control + ci->idx;
    int val = (int) gtk_range_get_value (range);
    
    if (input_set_control (videoIn, c, val) == 0) {
        set_slider_label (range);
        printf ("changed to %d\n", val);
    }
    else {
        printf ("changed to %d, but set failed\n", val);
    }
    if (input_get_control (videoIn, c, &val) == 0) {
        printf ("hardware value is %d\n", val);
    }
    else {
        printf ("hardware get failed\n");
    }
	
}

static void
check_changed (GtkToggleButton * toggle, VidState * s)
{
    
    ControlInfo * ci = g_object_get_data (G_OBJECT (toggle), "control_info");
    InputControl * c = s->control + ci->idx;
    int val;
    
    if (c->id == V4L2_CID_EXPOSURE_AUTO) {
    val = gtk_toggle_button_get_active (toggle) ? AUTO_EXP : MAN_EXP;
	} 
    else val = gtk_toggle_button_get_active (toggle) ? 1 : 0;
    
	
    if (input_set_control (videoIn, c, val) == 0) {
        printf ("changed to %d\n", val);
    }
    else {
        printf ("changed to %d, but set failed\n", val);
    }
    if (input_get_control (videoIn, c, &val) == 0) {
        printf ("hardware value is %d\n", val);
    }
    else {
        printf ("hardware get failed\n");
    }
	
}


static void
combo_changed (GtkComboBox * combo, VidState * s)
{
    
    ControlInfo * ci = g_object_get_data (G_OBJECT (combo), "control_info");
    InputControl * c = s->control + ci->idx;
    int val = gtk_combo_box_get_active (combo);
    
	
    if (input_set_control (videoIn, c, val) == 0) {
        printf ("changed to %d\n", val);
    }
    else {
        printf ("changed to %d, but set failed\n", val);
    }
    if (input_get_control (videoIn, c, &val) == 0) {
        printf ("hardware value is %d\n", val);
    }
    else {
        printf ("hardware get failed\n");
    }
	
}

static void
resolution_changed (GtkComboBox * Resolution, void *data)
{
	/* The new resolution is writen to conf file at exit             */
	/* then is read back at start. This means that for changing */
	/* resolution we must restart the application                    */
	
	/*Resolutions are hardcoded - should be given by driver     */
	
	int index = gtk_combo_box_get_active(Resolution);
	
	switch (index) {
	       case 0: //320x240
	        width=320;
	        height=240;
	        break;
	       case 1: //640x480
	        width=640;
	        height=480;
	        break;
	     }	
	
	restartdialog = gtk_dialog_new_with_buttons ("Program Restart",
                                                  mainwin,
                                                  GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  "now",
                                                  GTK_RESPONSE_ACCEPT,
                                                  "Later",
                                                  GTK_RESPONSE_REJECT,
                                                  NULL);
	
	GtkWidget *message = gtk_label_new ("Changes will only take effect after guvcview restart.\n\n\nRestart now?\n");
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(restartdialog)->vbox), message);
	gtk_widget_show_all(GTK_CONTAINER (GTK_DIALOG(restartdialog)->vbox));
	
  gint result = gtk_dialog_run (GTK_DIALOG (restartdialog));
  switch (result)
    {
      case GTK_RESPONSE_ACCEPT:
         /*restart app*/
		 shutd(1);
         break;
      default:
         /* do nothing since Restart rejected*/		
         break;
    }
  
  gtk_widget_destroy (restartdialog);
		
}

static void
FrameRate_changed (GtkComboBox * FrameRate, void *data)
{
	/*Frame Rates are hardcoded - should be given by driver */
	
	int index = gtk_combo_box_get_active (FrameRate);
      	
	switch (index) {
	  case 0: //15 fps
	   fps = 15;
	   break;
	  case 1: //25 fps
	   fps = 25;
	   break;
      	  case 2: // 30 fps
	   fps = 30;
	   break;
          default:
          /* set to the lowest*/
      	   fps = 15;
	}
 
	input_set_framerate (videoIn, fps);
	
	input_get_framerate(videoIn);
	fps=videoIn->fps;
	printf("hardware fps is %d ,%i/%i\n",fps,
	            videoIn->streamparm.parm.capture.timeperframe.numerator,
	            videoIn->streamparm.parm.capture.timeperframe.denominator);
	
}

static void
AVIComp_changed (GtkComboBox * AVIComp, void *data)
{
	int index = gtk_combo_box_get_active (AVIComp);
      	
	AVIFormat=index+1;	

}


static void
capture_image (GtkButton * CapImageButt, GtkWidget * ImageFNameEntry)
{
  
  char * filename = gtk_entry_get_text(GTK_ENTRY(ImageFNameEntry));
	
	videoIn->ImageFName = filename;
	videoIn->capImage = TRUE;
}

static void
capture_avi (GtkButton * CapAVIButt, GtkWidget * AVIFNameEntry)
{
	char *filename = gtk_entry_get_text(GTK_ENTRY(AVIFNameEntry));
	char *compression="YUY2";

	switch (AVIFormat) {
	 case 1:
		compression="YUY2";
		break;
	 case 2:
		compression="DIB ";
		break;
	 default:
		compression="YUY2";
	}	
	if(videoIn->capAVI) {
	 printf("stoping AVI capture\n");
	 gtk_button_set_label(CapAVIButt,"Capture");
	 AVIstoptime = ms_time();
	 printf("AVI stop time:%d\n",AVIstoptime);	
	 videoIn->capAVI = FALSE;
	 aviClose();		
	} 
	else {
	 if (!(videoIn->signalquit)) {
		/*thread exited while in AVI capture mode*/
        /* we have to close AVI                  */
     printf("close AVI since thread as exited\n");
	 gtk_button_set_label(CapAVIButt,"Capture");
	 printf("AVI stop time:%d\n",AVIstoptime);	
	 aviClose(); 		 
	 } 
	 else {
	   /* thread is running so start AVI capture*/	 
	   printf("starting AVI capture to %s\n",filename);
	   gtk_button_set_label(CapAVIButt,"Stop");  
	   AviOut = AVI_open_output_file(filename);
	   /*4CC compression "YUY2" (YUV) or "DIB " (RGB24)  or  whathever*/	
	   
	   AVI_set_video(AviOut, videoIn->width, videoIn->height, videoIn->fps,compression);		
	 
	   videoIn->AVIFName = filename;
	   AVIstarttime = ms_time();
       printf("AVI start time:%d\n",AVIstarttime);		
	   videoIn->capAVI = TRUE;
	 }
	}	
}


static void
draw_controls (VidState *s)
{
    int i;
	
	
    if (s->control) {
        for (i = 0; i < s->num_controls; i++) {
            ControlInfo * ci = s->control_info + i;
            if (ci->widget)
                gtk_widget_destroy (ci->widget);
            if (ci->label)
                gtk_widget_destroy (ci->label);
            if (ci->labelval)
                gtk_widget_destroy (ci->labelval);
        }
        free (s->control_info);
        s->control_info = NULL;
        input_free_controls (s->control, s->num_controls);
        s->control = NULL;
    }
    
    s->control = input_enum_controls (videoIn, &s->num_controls);
    //fprintf(stderr,"V4L2_CID_BASE=0x%x\n",V4L2_CID_BASE);
	//fprintf(stderr,"V4L2_CID_PRIVATE_BASE=0x%x\n",V4L2_CID_PRIVATE_BASE);
	//fprintf(stderr,"V4L2_CID_PRIVATE_LAST=0x%x\n",V4L2_CID_PRIVATE_LAST);
    fprintf(stderr,"Controls:\n");
    for (i = 0; i < s->num_controls; i++) {
		fprintf(stderr,"control[%d]: 0x%x",i,s->control[i].id);
        fprintf (stderr,"  %s, %d:%d:%d, default %d\n", s->control[i].name,
                s->control[i].min, s->control[i].step, s->control[i].max,
                s->control[i].default_val);
    }

   if((s->control_info = malloc (s->num_controls * sizeof (ControlInfo)))==NULL){
   			printf("couldn't allocate memory for: s->control_info\n");
			exit(1); 
   }

    for (i = 0; i < s->num_controls; i++) {
        ControlInfo * ci = s->control_info + i;
        InputControl * c = s->control + i;

        ci->idx = i;
        ci->widget = NULL;
        ci->label = NULL;
        ci->labelval = NULL;
        
        if (c->id == V4L2_CID_EXPOSURE_AUTO) {
            int val;
            ci->widget = gtk_check_button_new_with_label (c->name);
            g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
            gtk_widget_show (ci->widget);
            gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 3, 3+i, 4+i,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

            if (input_get_control (videoIn, c, &val) == 0) {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ci->widget),
                        val==AUTO_EXP ? TRUE : FALSE);
            }
            else {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ci->widget),
                        c->default_val==AUTO_EXP ? TRUE : FALSE);
                gtk_widget_set_sensitive (ci->widget, FALSE);
            }

            if (!c->enabled) {
                gtk_widget_set_sensitive (ci->widget, FALSE);
            }
            
            g_signal_connect (G_OBJECT (ci->widget), "toggled",
                    G_CALLBACK (check_changed), s);
	        
	    } else if (c->type == INPUT_CONTROL_TYPE_INTEGER) {
            PangoFontDescription * desc;
            int val;

            if (c->step == 0)
                c->step = 1;
            ci->widget = gtk_hscale_new_with_range (c->min, c->max, c->step);
            gtk_scale_set_draw_value (GTK_SCALE (ci->widget), FALSE);

            /* This is a hack to use always round the HScale to integer
             * values.  Strangely, this functionality is normally only
             * available when draw_value is TRUE. */
            GTK_RANGE (ci->widget)->round_digits = 0;

            gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+i, 4+i,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
            g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
            ci->maxchars = MAX (num_chars (c->min), num_chars (c->max));
            gtk_widget_show (ci->widget);
            
            ci->labelval = gtk_label_new (NULL);
            gtk_table_attach (GTK_TABLE (s->table), ci->labelval, 2, 3,
                    3+i, 4+i, GTK_FILL, 0, 0, 0);
            
            desc = pango_font_description_new ();
            pango_font_description_set_family_static (desc, "monospace");
            gtk_widget_modify_font (ci->labelval, desc);
            gtk_misc_set_alignment (GTK_MISC (ci->labelval), 1, 0.5);

            if (input_get_control (videoIn, c, &val) == 0) {
                gtk_range_set_value (GTK_RANGE (ci->widget), val);
            }
            else {
                gtk_range_set_value (GTK_RANGE (ci->widget), c->default_val);
                gtk_widget_set_sensitive (ci->widget, FALSE);
                gtk_widget_set_sensitive (ci->labelval, FALSE);
            }

            if (!c->enabled) {
                gtk_widget_set_sensitive (ci->widget, FALSE);
                gtk_widget_set_sensitive (ci->labelval, FALSE);
            }
            
            set_slider_label (GTK_RANGE (ci->widget));
            g_signal_connect (G_OBJECT (ci->widget), "value-changed",
                    G_CALLBACK (slider_changed), s);

            gtk_widget_show (ci->labelval);

            ci->label = gtk_label_new (g_strdup_printf ("%s:", c->name));
        }
        else if (c->type == INPUT_CONTROL_TYPE_BOOLEAN) {
            int val;
            ci->widget = gtk_check_button_new_with_label (c->name);
            g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
            gtk_widget_show (ci->widget);
            gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 3, 3+i, 4+i,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

            if (input_get_control (videoIn, c, &val) == 0) {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ci->widget),
                        val ? TRUE : FALSE);
            }
            else {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ci->widget),
                        c->default_val ? TRUE : FALSE);
                gtk_widget_set_sensitive (ci->widget, FALSE);
            }

            if (!c->enabled) {
                gtk_widget_set_sensitive (ci->widget, FALSE);
            }
            
            g_signal_connect (G_OBJECT (ci->widget), "toggled",
                    G_CALLBACK (check_changed), s);
        }
        else if (c->type == INPUT_CONTROL_TYPE_MENU) {
            int val, j;

            ci->widget = gtk_combo_box_new_text ();
            for (j = 0; j <= c->max; j++) {
                gtk_combo_box_append_text (GTK_COMBO_BOX (ci->widget), c->entries[j]);
            }

            gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 3, 3+i, 4+i,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
            g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
            gtk_widget_show (ci->widget);

            if (input_get_control (videoIn, c, &val) == 0) {
                gtk_combo_box_set_active (GTK_COMBO_BOX (ci->widget), val);
            }
            else {
                gtk_combo_box_set_active (GTK_COMBO_BOX (ci->widget), c->default_val);
                gtk_widget_set_sensitive (ci->widget, FALSE);
            }

            if (!c->enabled) {
                gtk_widget_set_sensitive (ci->widget, FALSE);
            }
            
            g_signal_connect (G_OBJECT (ci->widget), "changed",
                    G_CALLBACK (combo_changed), s);

            ci->label = gtk_label_new (g_strdup_printf ("%s:", c->name));
        }
        else {
            fprintf (stderr, "TODO: implement menu and button\n");
            continue;
        }

        if (ci->label) {
            gtk_misc_set_alignment (GTK_MISC (ci->label), 1, 0.5);

            gtk_table_attach (GTK_TABLE (s->table), ci->label, 0, 1, 3+i, 4+i,
                    GTK_FILL, 0, 0, 0);

            gtk_widget_show (ci->label);
        }
    }

}



int main_loop(void *data)
{
	int ret=0;
	int i,j,k,l,m,n,o;
	unsigned int YUVMacroPix;
	unsigned char *pix8 = (unsigned char *)&YUVMacroPix;
	Pix *pix2;
	char *pix;
	if ((pix2= malloc(sizeof(Pix)))==NULL) {
		printf("couldn't allocate memory for: pix2\n");
		ret=1;
		return(ret);
	}
	//fprintf(stderr,"Thread started...\n");
	/*
		ImageSurf=SDL_CreateRGBSurface(SDL_SWSURFACE, overlay->w,
		   overlay->h, 24, 0x00ff0000,0x0000ff00,0x000000ff,0);
	*/
	while (videoIn->signalquit) {
	 currtime = SDL_GetTicks();
	  if (currtime - lasttime > 0) {
		frmrate = 1000/(currtime - lasttime);
	 }
	 lasttime = currtime;
	
	// sprintf(capt,"Frame Rate: %d",frmrate);
	// SDL_WM_SetCaption(capt, NULL);
	
	 if (uvcGrab(videoIn) < 0) {
	    printf("Error grabbing=> Frame Rate is %d\n",frmrate);
	    videoIn->signalquit=0;
		ret = 2;
	 }
	
	 SDL_LockYUVOverlay(overlay);
	 memcpy(p, videoIn->framebuffer,
	       videoIn->width * (videoIn->height) * 2);
	 SDL_UnlockYUVOverlay(overlay);
	 SDL_DisplayYUVOverlay(overlay, &drect);
	
	 /*capture Image*/
	 if (videoIn->capImage){

		if((pim= malloc((pscreen->w)*(pscreen->h)*3))==NULL){/*24 bits -> 3bytes 32 bits ->4 bytes*/
		 	printf("Couldn't allocate memory for: pim\n");
	     	videoIn->signalquit=0;
			ret = 3;
		
		}
		
		//char *ppmheader = "P6\n# Generated by guvcview\n320 240\n255\n";
		//FILE * out = fopen("Yimage.ppm", "wb"); //saving as ppm
		//fprintf(out, ppmheader);
		
		k=overlay->h;
		//printf("overlay->h is %i\n",overlay->h);
		//printf("and pitches[0] is %i\n",overlay->pitches[0]);
	
			
		for(j=0;j<(overlay->h);j++){

			l=j*overlay->pitches[0];/*must add lines already writen=*/
						/*pitches is the overlay number */
						/*off bytes in a line (2*width) */
						
			m=(k*3*overlay->pitches[0])>>1;/*must add lines already writen=   */
						      /*for this case (rgb) every pixel   */
						      /*as 3 bytes (3*width=3*pitches/2)  */
						      /* >>1 = (/2) divide by 2 (?faster?)*/
			for (i=0;i<((overlay->pitches[0])>>2);i++){ /*>>2 = (/4)*/
				/*iterate every 4 bytes (32 bits)*/
				/*Y-U-V-Y1 =>2 pixel (4 bytes)   */
				
				n=i<<2;/*<<2 = (*4) multiply by 4 (?faster?)*/					
				pix8[0] = p[n+l];
				pix8[1] = p[n+1+l];
				pix8[2] = p[n+2+l];
				pix8[3] = p[n+3+l];
			
				/*get RGB data*/
				pix2=yuv2rgb(YUVMacroPix,0,pix2);
			
				/*In BitMaps lines are upside down and*/
				/*pixel format is bgr                 */
				
				o=i*6;				
			
				/*first pixel*/
				pim[o+m]=pix2->b;
				pim[o+1+m]=pix2->g;
				pim[o+2+m]=pix2->r;
				/*second pixel*/
				pim[o+3+m]=pix2->b1;
				pim[o+4+m]=pix2->g1;
				pim[o+5+m]=pix2->r1;	
		  	}
			k--;
	    	}
       /* SDL_LockSurface(ImageSurf);	
		memcpy(pix, pim,(pscreen->w)*(pscreen->h)*3); //24 bits -> 3bytes 32 bits ->4 bytes
		SDL_UnlockSurface(ImageSurf);*/
	

	    if(SaveBPM(videoIn->ImageFName, width, height, 24, pim)) {
	      fprintf (stderr,"Error: Couldn't capture Image to %s \n",
			     videoIn->ImageFName);
	    } 
	    else {	  
          printf ("Capture Image to %s \n",videoIn->ImageFName);
        }
		free(pim);
	    videoIn->capImage=FALSE;	
	  }
	  
	  /*capture AVI */
	  if (videoIn->capAVI && videoIn->signalquit){
	   long framesize;		
	   switch (AVIFormat) {
		case 1:
	  	   framesize=(pscreen->w)*(pscreen->h)*2; /*YUY2 -> 2 bytes per pixel */
	           if (AVI_write_frame (AviOut,
			       p, framesize) < 0)
	                printf ("write error on avi out \n");
		   break;
		case 2:
		    framesize=(pscreen->w)*(pscreen->h)*3; /*DIB 24/32 -> 3/4 bytes per pixel*/ 
		    if((pim= malloc(framesize))==NULL){
				printf("Couldn't allocate memory for: pim\n");
	     		videoIn->signalquit=0;
				ret = 4;
			}
		    k=overlay->h;
					
		for(j=0;j<(overlay->h);j++){

			l=j*overlay->pitches[0];/*must add lines already writen=*/
						/*pitches is the overlay number */
						/*off bytes in a line (2*width) */
						
			m=(k*3*overlay->pitches[0])>>1;/*must add lines already writen=   */
						      /*for this case (rgb) every pixel   */
						      /*as 3 bytes (3*width=3*pitches/2)  */
						      /* >>1 = (/2) divide by 2 (?faster?)*/
			for (i=0;i<((overlay->pitches[0])>>2);i++){ /*>>2 = (/4)*/
				/*iterate every 4 bytes (32 bits)*/
				/*Y-U-V-Y1 =>2 pixel (4 bytes)   */
				
				n=i<<2;/*<<2 = (*4) multiply by 4 (?faster?)*/					
				pix8[0] = p[n+l];
				pix8[1] = p[n+1+l];
				pix8[2] = p[n+2+l];
				pix8[3] = p[n+3+l];
			
				/*get RGB data*/
				pix2=yuv2rgb(YUVMacroPix,0,pix2);
			
				/*In BitMaps lines are upside down and*/
				/*pixel format is bgr                 */
				
				o=i*6;				
			
				/*first pixel*/
				pim[o+m]=pix2->b;
				pim[o+1+m]=pix2->g;
				pim[o+2+m]=pix2->r;
				/*second pixel*/
				pim[o+3+m]=pix2->b1;
				pim[o+4+m]=pix2->g1;
				pim[o+5+m]=pix2->r1;	
		  	}
			k--;
	    	}
		     
		     if (AVI_write_frame (AviOut,
			       pim, framesize) < 0)
	                printf ("write error on avi out \n");
		     free(pim);
		     break;


		} 
	   framecount++;	   
		  
	  } 
	  SDL_Delay(SDL_WAIT_TIME);
	
  }
  
  /*check if thread exited while AVI in capture mode*/
   if (videoIn->capAVI) {
    AVIstoptime = ms_time();
	videoIn->capAVI = FALSE;   
   }	   
   printf("Thread terminated...\n");
   free (pix2);
   printf("cleanig Thread allocations 100%\n");
   fflush(NULL);//flush all output buffers
   return (ret);	
}

int writeConf(const char *confpath) {
	int ret=1;
	FILE *fp;
	if ((fp = fopen(confpath,"w"))!=NULL) {
   		fprintf(fp,"# guvcview configuration file\n\n");
   		fprintf(fp,"# video resolution - hardware supported (logitech) 320x240 640x480\n");
   		fprintf(fp,"resolution=%ix%i\n",width,height);
   		fprintf(fp,"# control window size: default %ix%i\n",WINSIZEX,WINSIZEY);
   		fprintf(fp,"windowsize=%ix%i\n",winwidth,winheight);
   		fprintf(fp,"# mode video format 'yuv' or 'jpg'(default)\n");
   		fprintf(fp,"mode=%s\n",mode);
   		fprintf(fp,"# frames per sec. - hardware supported 15 25 32 - default( %i )\n",DEFAULT_FPS);
   		fprintf(fp,"fps=%i\n",fps);
   		fprintf(fp,"# bytes per pixel: default (0 - current)\n");
   		fprintf(fp,"bpp=%i\n",bpp);
   		fprintf(fp,"# hardware accelaration: 0 1 (default - 1)\n");
   		fprintf(fp,"hwaccel=%i\n",hwaccel);
   		fprintf(fp,"# video grab method: 0 -read 1 -mmap (default - 1)\n");
   		fprintf(fp,"grabmethod=%i\n",grabmethod);
   		
   		printf("write %s OK\n",confpath);
   		fclose(fp);
	} else {
	printf("Could not write file %s \n Please check file permissions\n",confpath);
	ret=0;
	exit(0);
	}
	return ret;
}

int readConf(const char *confpath) {
	int ret=1;
	char variable[20];
	char value[20];

	int i=0;
	int j=0;


	FILE *fp;

	if((fp = fopen(confpath,"r"))!=NULL) {
		char line[80];

  		while (fgets(line, 80, fp) != NULL) {
			//printf("%s-->\n",line);
			j++;
			if ((line[0]=='#') || (line[0]==' ') || (line[0]=='\n')) {
					//printf("Skip line %i\n",j);
			} else if ((i=sscanf(line,"%[^#=]=%[^#\n ]",variable,value))==2){
					//printf("line nr %i found %i\n",j,i);
					//printf ("%s : %s\n",variable,value);
					/* set variables */
					if (strcmp(variable,"resolution")==0) {
						if ((i=sscanf(value,"%ix%i",&width,&height))==2)
							printf("resolution: %i x %i\n",width,height); 			
					} else if (strcmp(variable,"windowsize")==0) {
						if ((i=sscanf(value,"%ix%i",&winwidth,&winheight))==2)
							printf("windowsize: %i x %i\n",winwidth,winheight);
						} else if 	(strcmp(variable,"mode")==0) {
							mode=strdup(value);
							printf("mode: %s\n",mode);
						} else if 	(strcmp(variable,"fps")==0) {
								if ((i=sscanf(value,"%i",&fps))==1)
									printf("fps: %i\n",fps);
						} else if 	(strcmp(variable,"bpp")==0) {
								if ((i=sscanf(value,"%i",&bpp))==1)
									printf("bpp: %i\n",bpp);
						} else if 	(strcmp(variable,"hwaccel")==0) {
								if ((i=sscanf(value,"%i",&hwaccel))==1)
									printf("hwaccel: %i\n",hwaccel);
						} else if 	(strcmp(variable,"grabmethod")==0) {
								if ((i=sscanf(value,"%i",&grabmethod))==1)
									printf("grabmethod: %i\n",grabmethod);
						}
			}
		}
		fclose(fp);
	} else {
   		printf("Could not open %s for read,\n will try to create it\n",confpath);
   		ret=writeConf(confpath);
	}
	return ret;
}

int main(int argc, char *argv[])
{
    
	int i;
	
	ARG_C=argc;
	if((ARG_V=malloc(argc*sizeof(argv)))==NULL){//allocs the size of the array
		printf("couldn't allocate memory for: ARG_V)\n");
		exit(1); 
	};
	for(i=0;i<argc;i++) { 
		/*allocs size for the strings in the array*/
		/* sizeof(char) should be 1 but we use it just in case*/
		if ((ARG_V[i] = malloc((strlen(argv[i]) + 1)*sizeof(char)))!=NULL) { //must check for NULL - out of mem	  
		 	strcpy(ARG_V[i],argv[i]);
			printf("ARG_V[%i]=%s argv[%i]=%s\n",i,ARG_V[i],i,argv[i]);
		} else {
			printf("couldn't allocate memory for: ARG_V[%i]\n",i);
			exit(1); 
		}
	}
	const SDL_VideoInfo *info;
    char driver[128];
    GtkWidget * boxh;
	GtkWidget *Resolution;
	GtkWidget *FrameRate;
	GtkWidget *label_FPS;
	GtkWidget *table2;
	GtkWidget *labelResol;
	GtkWidget *CapImageButt;
	GtkWidget *ImageFNameEntry;
	GtkWidget *CapAVIButt;
	GtkWidget *AVIFNameEntry;
	GtkWidget *AVIComp;
	GtkWidget *label_AVIComp;
	
	//VidState * s;
	if ((s = malloc (sizeof (VidState)))==NULL){
		printf("couldn't allocate memory for: s\n");
		exit(1); 
	}
    SDL_Thread *mythread;
   
    if((capt = (char *) calloc(1, 100 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for: capt\n");
		exit(1);
	}
       
    const char *home;
    const char *videodevice = NULL;
    //const char *mode = NULL
    
    
    char *separateur;
    char *sizestring = NULL;
    int enableRawStreamCapture = 0;
    int enableRawFrameCapture = 0;
	
	home = getenv("HOME");
	//confPath=malloc((strlen(home)+1)*sizeof(char));
	//printf("size is...%i",strlen(home));
    //strcpy(confPath,home);
	sprintf(confPath,"%s%s", home,"/.guvcviewrc");
    //strcat(confPath,"/.guvcviewrc");
    
    readConf(confPath);

    printf("guvcview version %s \n", version);
	
    
    for (i = 1; i < argc; i++) {
	
	/* skip bad arguments */
	if (argv[i] == NULL || *argv[i] == 0 || *argv[i] != '-') {
	    continue;
	}
	if (strcmp(argv[i], "-d") == 0) {
	    if (i + 1 >= argc) {
		printf("No parameter specified with -d, aborting.\n");
		exit(1);
	    }
	    videodevice = strdup(argv[i + 1]);
	}
	if (strcmp(argv[i], "-g") == 0) {
	    /* Ask for read instead default  mmap */
	    grabmethod = 0;
	}
	if (strcmp(argv[i], "-w") == 0) {
	    /* disable hw acceleration */
	    hwaccel = 0;
	}
	if (strcmp(argv[i], "-f") == 0) {
	    if (i + 1 >= argc) {
		printf("No parameter specified with -f, aborting.\n");	
	    } else {
	    mode = strdup(argv[i + 1]);
		}
	}
	
	if (strcmp(argv[i], "-s") == 0) {
	    if (i + 1 >= argc) {
		printf("No parameter specified with -s, aborting.\n");
		exit(1);
	    }

	    sizestring = strdup(argv[i + 1]);

	    width = strtoul(sizestring, &separateur, 10);
	    if (*separateur != 'x') {
		printf("Error in size use -s widthxheight \n");
		exit(1);
	    } else {
		++separateur;
		height = strtoul(separateur, &separateur, 10);
		if (*separateur != 0)
		    printf("hmm.. dont like that!! trying this height \n");
		printf(" size width: %d height: %d \n", width, height);
	    }
		printf(" size width: %d height: %d \n", width, height);
	}
	if (strcmp(argv[i], "-S") == 0) {
	    /* Enable raw stream capture from the start */
	    enableRawStreamCapture = 1;
	}
	if (strcmp(argv[i], "-c") == 0) {
	    /* Enable raw frame capture for the first frame */
	    enableRawFrameCapture = 1;
	}
	if (strcmp(argv[i], "-C") == 0) {
	    /* Enable raw frame stream capture from the start*/
	    enableRawFrameCapture = 2;
	}

	if (strcmp(argv[i], "-h") == 0) {
	    printf("usage: guvcview [-h -d -g -f -s -c -C -S] \n");
	    printf("-h	print this message \n");
	    printf("-d	/dev/videoX       use videoX device\n");
	    printf("-g	use read method for grab instead mmap \n");
	    printf("-w	disable SDL hardware accel. \n");
	    printf
		("-f	video format  default jpg  others options are yuv jpg \n");
	    printf("-s	widthxheight      use specified input size \n");
	    printf("-c	enable raw frame capturing for the first frame\n");
	    printf("-C	enable raw frame stream capturing from the start\n");
	    printf("-S	enable raw stream capturing from the start\n");
	    exit(0);
	  }
    }
    
    if (strncmp(mode, "yuv", 3) == 0) {
		format = V4L2_PIX_FMT_YUYV;
		printf("Format is yuyv\n");
	} else if (strncmp(mode, "jpg", 3) == 0) {
		format = V4L2_PIX_FMT_MJPEG;
		printf("Format is MJPEG\n");
	} else {
		format = V4L2_PIX_FMT_MJPEG;
		printf("Format is Default MJPEG\n");
	}
	
	gtk_init(&argc, &argv);
	

	/* Create a main window */
	mainwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (mainwin), "GUVCViewer Controls");
	//gtk_widget_set_usize(mainwin, winwidth, winheight);
	gtk_window_resize(mainwin,winwidth,winheight);
	/* Add event handlers */
	gtk_signal_connect(GTK_OBJECT(mainwin), "delete_event", GTK_SIGNAL_FUNC(delete_event), 0);
	
	/* Hack to get SDL to use GTK window */
	//~ { char SDL_windowhack[32];
		//~ sprintf(SDL_windowhack,"SDL_WINDOWID=%ld",
			//~ GDK_WINDOW_XWINDOW(mainwin->window));
		//~ putenv(SDL_windowhack);
		//~ fprintf(stderr,"SDL_WINDOWID=%ld\n",GDK_WINDOW_XWINDOW(mainwin->window));
	//~ }
	
    /************* Test SDL capabilities ************/
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) {
	fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
	exit(1);
    }
	
    /* For this version, we will use hardware acceleration as default*/
    if(hwaccel) {
     	if ( ! getenv("SDL_VIDEO_YUV_HWACCEL") ) {
        	putenv("SDL_VIDEO_YUV_HWACCEL=1");
     	}
	 	if ( ! getenv("SDL_VIDEO_YUV_DIRECT") ) {
			putenv("SDL_VIDEO_YUV_DIRECT=1"); 
	 	}
	 } else {
	 	if ( ! getenv("SDL_VIDEO_YUV_HWACCEL") ) {
        	putenv("SDL_VIDEO_YUV_HWACCEL=0");
     	}
	 	if ( ! getenv("SDL_VIDEO_YUV_DIRECT") ) {
			putenv("SDL_VIDEO_YUV_DIRECT=0"); 
	 	}
	 }
     
    if (SDL_VideoDriverName(driver, sizeof(driver))) {
	printf("Video driver: %s\n", driver);
    }
    info = SDL_GetVideoInfo();

    if (info->wm_available) {
	printf("A window manager is available\n");
    }
    if (info->hw_available) {
	printf("Hardware surfaces are available (%dK video memory)\n",
	       info->video_mem);
	SDL_VIDEO_Flags |= SDL_HWSURFACE;
    }
    if (info->blit_hw) {
	printf("Copy blits between hardware surfaces are accelerated\n");
	SDL_VIDEO_Flags |= SDL_ASYNCBLIT;
    }
    if (info->blit_hw_CC) {
	printf
	    ("Colorkey blits between hardware surfaces are accelerated\n");
    }
    if (info->blit_hw_A) {
	printf("Alpha blits between hardware surfaces are accelerated\n");
    }
    if (info->blit_sw) {
	printf
	    ("Copy blits from software surfaces to hardware surfaces are accelerated\n");
    }
    if (info->blit_sw_CC) {
	printf
	    ("Colorkey blits from software surfaces to hardware surfaces are accelerated\n");
    }
    if (info->blit_sw_A) {
	printf
	    ("Alpha blits from software surfaces to hardware surfaces are accelerated\n");
    }
    if (info->blit_fill) {
	printf("Color fills on hardware surfaces are accelerated\n");
    }

    if (!(SDL_VIDEO_Flags & SDL_HWSURFACE))
	SDL_VIDEO_Flags |= SDL_SWSURFACE;

    if (videodevice == NULL || *videodevice == 0) {
	videodevice = "/dev/video0";
    }
	
    if((videoIn = (struct vdIn *) calloc(1, sizeof(struct vdIn)))==NULL){
   		printf("couldn't allocate memory for: videoIn\n");
		exit(1); 
    }
    if (init_videoIn
	(videoIn, (char *) videodevice, width, height, format,
	 grabmethod, fps) < 0)
	exit(1);
    pscreen =
	SDL_SetVideoMode(videoIn->width, videoIn->height, bpp,
			 SDL_VIDEO_Flags);
    overlay =
	SDL_CreateYUVOverlay(videoIn->width, videoIn->height,
			     SDL_YUY2_OVERLAY, pscreen);
	
    p = (unsigned char *) overlay->pixels[0];
    
	drect.x = 0;
    drect.y = 0;
    drect.w = pscreen->w;
    drect.h = pscreen->h;
    
	if (enableRawStreamCapture) {
		videoIn->captureFile = fopen("stream.raw", "wb");
		if(videoIn->captureFile == NULL) {
			perror("Unable to open file for raw stream capturing");
		} else {
			printf("Starting raw stream capturing to stream.raw ...\n");
		}
    }
    if (enableRawFrameCapture)
		videoIn->rawFrameCapture = enableRawFrameCapture;

   	SDL_WM_SetCaption("GUVCVideo", NULL);
    lasttime = SDL_GetTicks();
    
	
    /* initialize thread data - no use for it*/
    //~ ptdata.ptscreen = &pscreen;
	//~ ptdata.ptoverlay= &overlay;
    //~ ptdata.ptvideoIn = videoIn;
    //~ ptdata.ptsdlevent = &sdlevent;
    //~ ptdata.drect = &drect;
    
	
	s->table = gtk_table_new (1, 3, FALSE);
   	gtk_table_set_row_spacings (GTK_TABLE (s->table), 10);
    gtk_table_set_col_spacings (GTK_TABLE (s->table), 10);
    gtk_container_set_border_width (GTK_CONTAINER (s->table), 10);
    gtk_widget_set_size_request (s->table, 400, -1);
	
	s->control = NULL;
	draw_controls(s);
	
	boxh = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (boxh), s->table, FALSE, FALSE, 0);
	gtk_widget_show (s->table);
	
	table2 = gtk_table_new(1,3,FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table2), 10);
    gtk_table_set_col_spacings (GTK_TABLE (table2), 10);
    gtk_container_set_border_width (GTK_CONTAINER (table2), 10);
    gtk_widget_set_size_request (table2, 300, -1);
	
	/* Resolution*/
	Resolution = gtk_combo_box_new_text ();
	gtk_combo_box_append_text(GTK_COMBO_BOX(Resolution),"320x240");
	gtk_combo_box_append_text(GTK_COMBO_BOX(Resolution),"640x480");
	gtk_table_attach(GTK_TABLE(table2), Resolution, 1, 3, 3, 4,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (Resolution);
	
	if (width==640 && height==480)
	   gtk_combo_box_set_active(GTK_COMBO_BOX(Resolution),1);
	else 
	   gtk_combo_box_set_active(GTK_COMBO_BOX(Resolution),0);
	
	gtk_widget_set_sensitive (Resolution, TRUE);
	g_signal_connect (GTK_COMBO_BOX(Resolution), "changed",
    		G_CALLBACK (resolution_changed), NULL);
	
	labelResol = gtk_label_new("Resolution:");
	gtk_misc_set_alignment (GTK_MISC (labelResol), 1, 0.5);

    gtk_table_attach (GTK_TABLE(table2), labelResol, 0, 1, 3, 4,
                    GTK_FILL, 0, 0, 0);

    gtk_widget_show (labelResol);
	
	/* Frame Rate */
	FrameRate = gtk_combo_box_new_text ();
	gtk_combo_box_append_text(GTK_COMBO_BOX(FrameRate),"15 fps");
	gtk_combo_box_append_text(GTK_COMBO_BOX(FrameRate),"25 fps");
	gtk_combo_box_append_text(GTK_COMBO_BOX(FrameRate),"30 fps");
	gtk_table_attach(GTK_TABLE(table2), FrameRate, 1, 3, 2, 3,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (FrameRate);
	
	switch (videoIn->fps) {
	   case 15:
		gtk_combo_box_set_active(GTK_COMBO_BOX(FrameRate),0);
	    break;
	   case 25:	
	    gtk_combo_box_set_active(GTK_COMBO_BOX(FrameRate),1);
	    break;
	   case 30:
		gtk_combo_box_set_active(GTK_COMBO_BOX(FrameRate),2);
	    break;
	   default:
	    /*set to the lowest fps*/
	    gtk_combo_box_set_active(GTK_COMBO_BOX(FrameRate),0);
 	}
	gtk_widget_set_sensitive (FrameRate, TRUE);
	g_signal_connect (GTK_COMBO_BOX(FrameRate), "changed",
    	G_CALLBACK (FrameRate_changed), NULL);
	
	label_FPS = gtk_label_new("Frame Rate:");
	gtk_misc_set_alignment (GTK_MISC (label_FPS), 1, 0.5);

    gtk_table_attach (GTK_TABLE(table2), label_FPS, 0, 1, 2, 3,
                    GTK_FILL, 0, 0, 0);

    gtk_widget_show (label_FPS);
	
	/* Image Capture*/
	CapImageButt = gtk_button_new_with_label("Capture");
	ImageFNameEntry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(ImageFNameEntry),DEFAULT_IMAGE_FNAME);
	gtk_table_attach(GTK_TABLE(table2), CapImageButt, 0, 1, 4, 5,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table2), ImageFNameEntry, 1, 3, 4, 5,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_widget_show (CapImageButt);
	gtk_widget_show (ImageFNameEntry);
	g_signal_connect (GTK_BUTTON(CapImageButt), "clicked",
         G_CALLBACK (capture_image), ImageFNameEntry);
	
	
	/*AVI Capture*/
	CapAVIButt = gtk_button_new_with_label("Capture");
	AVIFNameEntry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(AVIFNameEntry),DEFAULT_AVI_FNAME);
	gtk_table_attach(GTK_TABLE(table2), CapAVIButt, 0, 1, 5, 6,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table2), AVIFNameEntry, 1, 3, 5, 6,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_widget_show (CapAVIButt);
	gtk_widget_show (AVIFNameEntry);
	g_signal_connect (GTK_BUTTON(CapAVIButt), "clicked",
         G_CALLBACK (capture_avi), AVIFNameEntry);
	
	gtk_box_pack_start ( GTK_BOX (boxh), table2, FALSE, FALSE, 0);
	gtk_widget_show (table2);
	
	/* AVI Compressor */
	AVIComp = gtk_combo_box_new_text ();
	gtk_combo_box_append_text(GTK_COMBO_BOX(AVIComp),"YUY2 - uncomp YUV");
	gtk_combo_box_append_text(GTK_COMBO_BOX(AVIComp),"RGB - uncomp BMP");
	gtk_table_attach(GTK_TABLE(table2), AVIComp, 1, 3, 6, 7,
                    GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (AVIComp);

	switch (AVIFormat) {
	   case 1:
		gtk_combo_box_set_active(GTK_COMBO_BOX(AVIComp),0);
	    break;
	   case 2:	
	    	gtk_combo_box_set_active(GTK_COMBO_BOX(AVIComp),1);
	    break;
	   default:
	    /*set Format to YUY2*/
	    	AVIFormat=1;
		gtk_combo_box_set_active(GTK_COMBO_BOX(AVIComp),0);
 	}
	gtk_widget_set_sensitive (AVIComp, TRUE);
	g_signal_connect (GTK_COMBO_BOX(AVIComp), "changed",
    	G_CALLBACK (AVIComp_changed), NULL);
	
	label_AVIComp = gtk_label_new("AVI Format:");
	gtk_misc_set_alignment (GTK_MISC (label_AVIComp), 1, 0.5);

    gtk_table_attach (GTK_TABLE(table2), label_AVIComp, 0, 1, 6, 7,
                    GTK_FILL, 0, 0, 0);

    gtk_widget_show (label_AVIComp);



	gtk_container_add (GTK_CONTAINER (mainwin), boxh);
    gtk_widget_show (boxh);
	
	gtk_widget_show (mainwin);
	
	/* Creating the main loop thread*/
	//~ mythread = SDL_CreateThread( main_loop,(void *) &ptdata);
	mythread = SDL_CreateThread( main_loop,NULL);
	
    
	/* The last thing to get called */
	gtk_main();

	//input_free_controls (s->control, s->num_controls);

	return 0;
}


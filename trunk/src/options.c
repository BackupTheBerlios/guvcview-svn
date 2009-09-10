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

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>

#include "defs.h"
#include "globals.h"
#include "string_utils.h"
#include "avilib.h"
#include "v4l2uvc.h"
#include "../config.h"
/*----------------------- write conf (.guvcviewrc) file ----------------------*/
int 
writeConf(struct GLOBAL *global, char *videodevice) 
{
	int ret=0;
	FILE *fp;

	if ((fp = g_fopen(global->confPath,"w"))!=NULL) 
	{
		g_fprintf(fp,"# guvcview configuration file for %s\n\n",videodevice);
		g_fprintf(fp,"# Thread stack size: default 128 pages of 64k = 8388608 bytes\n");
		g_fprintf(fp,"stack_size=%d\n",global->stack_size);
		g_fprintf(fp,"# video loop sleep time in ms: 0,1,2,3,...\n");
		g_fprintf(fp,"# increased sleep time -> less cpu load, more droped frames\n");
		g_fprintf(fp,"vid_sleep=%i\n",global->vid_sleep);
		g_fprintf(fp,"# capture method: 1- mmap 2- read\n");
		g_fprintf(fp,"cap_meth=%i\n",global->cap_meth);
		g_fprintf(fp,"# video resolution \n");
		g_fprintf(fp,"resolution='%ix%i'\n",global->width,global->height);
		g_fprintf(fp,"# control window size: default %ix%i\n",WINSIZEX,WINSIZEY);
		g_fprintf(fp,"windowsize='%ix%i'\n",global->winwidth,global->winheight);
		g_fprintf(fp,"#vertical pane size\n");
		g_fprintf(fp,"vpane=%i\n",global->boxvsize);
		g_fprintf(fp,"#spin button behavior: 0-non editable 1-editable\n");
		g_fprintf(fp,"spinbehave=%i\n", global->spinbehave);
		g_fprintf(fp,"# mode video format 'yuvy' 'yvyu' 'uyvy' 'yyuv' 'yu12' 'yv12' 'nv12' 'nv21' 'nv16' 'nv61' 'y41p' 'grey' 's501' 's505' 's508' 'gbrg' 'grbg' 'ba81' 'rggb' 'rgb3' 'bgr3' 'jpeg' 'mjpg'(default)\n");
		g_fprintf(fp,"mode='%s'\n",global->mode);
		g_fprintf(fp,"# frames per sec. - hardware supported - default( %i )\n",DEFAULT_FPS);
		g_fprintf(fp,"fps='%d/%d'\n",global->fps_num,global->fps);
		g_fprintf(fp,"#Display Fps counter: 1- Yes 0- No\n");
		g_fprintf(fp,"fps_display=%i\n",global->FpsCount);
		g_fprintf(fp,"#auto focus (continuous): 1- Yes 0- No\n");
		g_fprintf(fp,"auto_focus=%i\n",global->autofocus);
		g_fprintf(fp,"# bytes per pixel: default (0 - current)\n");
		g_fprintf(fp,"bpp=%i\n",global->bpp);
		g_fprintf(fp,"# hardware accelaration: 0 1 (default - 1)\n");
		g_fprintf(fp,"hwaccel=%i\n",global->hwaccel);
		g_fprintf(fp,"# video compression format: 0-MJPG 1-YUY2/UYVY 2-DIB (BMP 24) 3-MPEG1 4-FLV1 5-MPEG2 6-MS MPEG4 V3(DIV3) 7-MPEG4 (DIV5)\n");
		g_fprintf(fp,"avi_format=%i\n",global->VidCodec);
		g_fprintf(fp,"# video muxer: 0-avi 1-matroska\n");
		g_fprintf(fp,"vid_mux=%i\n",global->VidFormat);
		g_fprintf(fp,"# avi file max size (MAX: %d bytes)\n",AVI_MAX_SIZE);
		g_fprintf(fp,"avi_max_len=%li\n",global->AVI_MAX_LEN);
		g_fprintf(fp,"# Auto Video naming (ex: filename-n.avi)\n");
		g_fprintf(fp,"avi_inc=%d\n",global->vid_inc);
		g_fprintf(fp,"# sound 0 - disable 1 - enable\n");
		g_fprintf(fp,"sound=%i\n",global->Sound_enable);
		g_fprintf(fp,"# sound API: 0- Portaudio  1- Pulseaudio\n");
		g_fprintf(fp,"snd_api=%i\n", global->Sound_API);
		g_fprintf(fp,"# snd_device - sound device id as listed by portaudio (pulse uses default device)\n");
		g_fprintf(fp,"snd_device=%i\n",global->Sound_UseDev);
		g_fprintf(fp,"# snd_samprate - sound sample rate\n");
		g_fprintf(fp,"snd_samprate=%i\n",global->Sound_SampRateInd);
		g_fprintf(fp,"# snd_numchan - sound number of channels 0- dev def 1 - mono 2 -stereo\n");
		g_fprintf(fp,"snd_numchan=%i\n",global->Sound_NumChanInd);
		g_fprintf(fp,"#snd_numsec - video audio blocks size in sec: 1,2,3,.. \n");
		g_fprintf(fp,"# more seconds = more granularity, more memory allocation but less disc I/O\n");
		g_fprintf(fp,"snd_numsec=%i\n",global->Sound_NumSec);
		g_fprintf(fp,"# Sound Format (PCM=1 (0x0001) MP2=80 (0x0050)\n");
		g_fprintf(fp,"snd_format=%i\n",global->Sound_Format);
		g_fprintf(fp,"# Sound bit Rate used by mpeg audio default=160 Kbps\n");
		g_fprintf(fp,"#other values: 48 56 64 80 96 112 128 160 192 224 384\n");
		g_fprintf(fp,"snd_bitrate=%i\n",global->Sound_bitRate);
		g_fprintf(fp,"#Pan Step in degrees, Default=2\n");
		g_fprintf(fp,"Pan_Step=%i\n",global->PanStep);
		g_fprintf(fp,"#Tilt Step in degrees, Default=2\n");
		g_fprintf(fp,"Tilt_Step=%i\n",global->TiltStep);
		g_fprintf(fp,"# video filters: 0 -none 1- flip 2- upturn 4- negate 8- mono (add the ones you want)\n");
		g_fprintf(fp,"frame_flags=%i\n",global->Frame_Flags);
		g_fprintf(fp,"# Image capture Full Path\n");
		g_fprintf(fp,"image_path='%s/%s'\n",global->imgFPath[1],global->imgFPath[0]);
		g_fprintf(fp,"# Auto Image naming (filename-n.ext)\n");
		g_fprintf(fp,"image_inc=%d\n",global->image_inc);
		g_fprintf(fp,"# Video capture Full Path\n");
		g_fprintf(fp,"avi_path='%s/%s'\n",global->vidFPath[1],global->vidFPath[0]);
		g_fprintf(fp,"# control profiles Full Path\n");
		g_fprintf(fp,"profile_path='%s/%s'\n",global->profile_FPath[1],global->profile_FPath[0]);
		printf("write %s OK\n",global->confPath);
		fclose(fp);
	} 
	else 
	{
		g_printerr("Could not write file %s \n Please check file permissions\n",global->confPath);
		ret=-1;
	}
	return ret;
}

/*----------------------- read conf (.guvcviewrc(-videoX)) file -----------------------*/
int
readConf(struct GLOBAL *global)
{
	int ret=0;
	int signal=1; /*1=>+ or -1=>-*/
	GScanner  *scanner;
	GTokenType ttype;
	GScannerConfig config = 
	{
		" \t\r\n",                     /* characters to skip */
		G_CSET_a_2_z "_" G_CSET_A_2_Z, /* identifier start */
		G_CSET_a_2_z "_." G_CSET_A_2_Z G_CSET_DIGITS,/* identifier cont. */
		"#\n",                         /* single line comment */
		FALSE,                         /* case_sensitive */
		TRUE,                          /* skip multi-line comments */
		TRUE,                          /* skip single line comments */
		FALSE,                         /* scan multi-line comments */
		TRUE,                          /* scan identifiers */
		TRUE,                          /* scan 1-char identifiers */
		FALSE,                         /* scan NULL identifiers */
		FALSE,                         /* scan symbols */
		FALSE,                         /* scan binary */
		FALSE,                         /* scan octal */
		TRUE,                          /* scan float */
		TRUE,                          /* scan hex */
		FALSE,                         /* scan hex dollar */
		TRUE,                          /* scan single quote strings */
		TRUE,                          /* scan double quite strings */
		TRUE,                          /* numbers to int */
		FALSE,                         /* int to float */
		TRUE,                          /* identifier to string */
		TRUE,                          /* char to token */
		FALSE,                          /* symbol to token */
		FALSE,                         /* scope 0 fallback */
		FALSE                          /* store int64 */
	};

	int fd = g_open (global->confPath, O_RDONLY, 0);
	
	if (fd < 0 )
	{
		printf("Could not open %s for read,\n will try to create it\n",global->confPath);
		ret=writeConf(global, global->videodevice);
	}
	else
	{
		scanner = g_scanner_new (&config);
		g_scanner_input_file (scanner, fd);
		scanner->input_name = global->confPath;
		
		for (ttype = g_scanner_get_next_token (scanner);
			ttype != G_TOKEN_EOF;
			ttype = g_scanner_get_next_token (scanner)) 
		{
			if (ttype == G_TOKEN_STRING) 
			{
				//printf("reading %s...\n",scanner->value.v_string);
				char *name = g_strdup (scanner->value.v_string);
				ttype = g_scanner_get_next_token (scanner);
				if (ttype != G_TOKEN_EQUAL_SIGN) 
				{
					g_scanner_unexp_token (scanner,
						G_TOKEN_EQUAL_SIGN,
						NULL,
						NULL,
						NULL,
						NULL,
						FALSE);
				}
				else
				{
					ttype = g_scanner_get_next_token (scanner);
					/*check for signed integers*/
					if(ttype == '-')
					{
						signal = -1;
						ttype = g_scanner_get_next_token (scanner);
					}
					
					if (ttype == G_TOKEN_STRING)
					{
						signal=1; /*reset signal*/
						//if (g_strcmp0(name,"video_device")==0) 
						//{
						//	g_snprintf(global->videodevice,15,"%s",scanner->value.v_string);
						//}
						
						/*must check for defaults since ReadOpts runs before ReadConf*/
						if (g_strcmp0(name,"resolution")==0) 
						{
							if(global->flg_res < 1)
								sscanf(scanner->value.v_string,"%ix%i",
									&(global->width), 
									&(global->height));
						}
						else if (g_strcmp0(name,"windowsize")==0) 
						{
							sscanf(scanner->value.v_string,"%ix%i",
								&(global->winwidth), &(global->winheight));
						}
						else if (g_strcmp0(name,"mode")==0)
						{
							if(global->flg_mode < 1)
							{
								/*use fourcc but keep it compatible with luvcview*/
								if(g_strcmp0(scanner->value.v_string,"yuv") == 0)
									g_snprintf(global->mode,5,"yuyv");
								else
									g_snprintf(global->mode,5,"%s",scanner->value.v_string);
							}
						}
						else if (g_strcmp0(name,"fps")==0)
						{
							sscanf(scanner->value.v_string,"%i/%i",
								&(global->fps_num), &(global->fps));
						}
						else if (g_strcmp0(name,"image_path")==0)
						{
							if(global->flg_imgFPath < 1)
							{
								global->imgFPath = splitPath(scanner->value.v_string,global->imgFPath);
								/*get the file type*/
								global->imgFormat = check_image_type(global->imgFPath[0]);
							}
						}
						else if (g_strcmp0(name,"avi_path")==0) 
						{
							if(global->vidfile == NULL)
								global->vidFPath=splitPath(scanner->value.v_string,global->vidFPath);
						}
						else if (g_strcmp0(name,"profile_path")==0) 
						{
							if(global->lprofile < 1)
								global->profile_FPath=splitPath(scanner->value.v_string,
									global->profile_FPath);
						}
						else
						{
							printf("unexpected string value (%s) for %s\n", 
								scanner->value.v_string, name);
						}
					}
					else if (ttype==G_TOKEN_INT)
					{
						if (g_strcmp0(name,"stack_size")==0)
						{
							global->stack_size = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vid_sleep")==0)
						{
							global->vid_sleep = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"cap_meth")==0)
						{
							if(!(global->flg_cap_meth))
								global->cap_meth = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vpane")==0) 
						{
							global->boxvsize = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"spinbehave")==0) 
						{
							global->spinbehave = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"fps")==0)
						{
							/*parse non-quoted fps values*/
							int line = g_scanner_cur_line(scanner);
							
							global->fps_num = scanner->value.v_int;
							ttype = g_scanner_peek_next_token (scanner);
							if(ttype=='/')
							{
								/*get '/'*/
								ttype = g_scanner_get_next_token (scanner);
								ttype = g_scanner_peek_next_token (scanner);
								if(ttype==G_TOKEN_INT)
								{
									ttype = g_scanner_get_next_token (scanner);
									global->fps = scanner->value.v_int;
								} 
								else if (scanner->next_line>line)
								{
									/*start new loop*/
									break;
								}
								else
								{
									ttype = g_scanner_get_next_token (scanner);
									g_scanner_unexp_token (scanner,
										G_TOKEN_NONE,
										NULL,
										NULL,
										NULL,
										"bad value for fps",
										FALSE);
								}
							}
						}
						else if (strcmp(name,"fps_display")==0) 
						{
							if(global->flg_FpsCount < 1)
								global->FpsCount = (short) scanner->value.v_int;
						}
						else if (g_strcmp0(name,"auto_focus")==0) 
						{
							global->autofocus = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"bpp")==0) 
						{
							global->bpp = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"hwaccel")==0) 
						{
							if(global->flg_hwaccel < 1)
								global->hwaccel = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"avi_format")==0) 
						{
							global->VidCodec = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vid_mux")==0) 
						{
							global->VidFormat = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"avi_max_len")==0) 
						{
							global->AVI_MAX_LEN = (ULONG) scanner->value.v_int;
							global->AVI_MAX_LEN = AVI_set_MAX_LEN (global->AVI_MAX_LEN);
						}
						else if (g_strcmp0(name,"avi_inc")==0) 
						{
							global->vid_inc = (DWORD) scanner->value.v_int;
							g_snprintf(global->vidinc_str,20,_("File num:%d"),global->vid_inc);
						}
						else if (g_strcmp0(name,"sound")==0) 
						{
							global->Sound_enable = (short) scanner->value.v_int;
						}
						else if (g_strcmp0(name,"snd_api")==0) 
						{
							global->Sound_API = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"snd_device")==0) 
						{
							global->Sound_UseDev = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"snd_samprate")==0) 
						{
							global->Sound_SampRateInd = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"snd_numchan")==0) 
						{
							global->Sound_NumChanInd = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"snd_numsec")==0) 
						{
							global->Sound_NumSec = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"snd_format")==0) 
						{
							global->Sound_Format = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"snd_bitrate")==0) 
						{
							global->Sound_bitRate = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"Pan_Step")==0)
						{
							global->PanStep = signal * scanner->value.v_int;
							signal = 1; /*reset signal*/
						}
						else if (g_strcmp0(name,"Tilt_Step")==0)
						{
							global->TiltStep = signal * scanner->value.v_int;
							signal = 1; /*reset signal*/
						}
						else if (g_strcmp0(name,"frame_flags")==0) 
						{
							global->Frame_Flags = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"image_inc")==0) 
						{
							if(global->image_timer <= 0)
							{
								global->image_inc = (DWORD) scanner->value.v_int;
								g_snprintf(global->imageinc_str,20,_("File num:%d"),global->image_inc);
							}
						}
						else
						{
							printf("unexpected integer value (%lu) for %s\n", 
								scanner->value.v_int, name);
							printf("Strings must be quoted\n");
						}
					}
					else if (ttype==G_TOKEN_FLOAT)
					{
						printf("unexpected float value (%f) for %s\n", scanner->value.v_float, name);
					}
					else if (ttype==G_TOKEN_CHAR)
					{
						printf("unexpected char value (%c) for %s\n", scanner->value.v_char, name);
					}
					else
					{
						g_scanner_unexp_token (scanner,
							G_TOKEN_NONE,
							NULL,
							NULL,
							NULL,
							"string values must be quoted - skiping",
							FALSE);
						int line = g_scanner_cur_line (scanner);
						int stp=0;
					
						do
						{
							ttype = g_scanner_peek_next_token (scanner);
							if(scanner->next_line > line)
							{
								//printf("next line reached\n");
								stp=1;
								break;
							}
							else
							{
								ttype = g_scanner_get_next_token (scanner);
							}
						}
						while (!stp);
					}
				}
				g_free(name);
			}
		}
		
		g_scanner_destroy (scanner);
		close (fd);
		
		if (global->debug) 
		{
			g_printf("video_device: %s\n",global->videodevice);
			g_printf("vid_sleep: %i\n",global->vid_sleep);
			g_printf("cap_meth: %i\n",global->cap_meth);
			g_printf("resolution: %i x %i\n",global->width,global->height);
			g_printf("windowsize: %i x %i\n",global->winwidth,global->winheight);
			g_printf("vert pane: %i\n",global->boxvsize);
			g_printf("spin behavior: %i\n",global->spinbehave);
			g_printf("mode: %s\n",global->mode);
			g_printf("fps: %i/%i\n",global->fps_num,global->fps);
			g_printf("Display Fps: %i\n",global->FpsCount);
			g_printf("bpp: %i\n",global->bpp);
			g_printf("hwaccel: %i\n",global->hwaccel);
			g_printf("avi_format: %i\n",global->VidCodec);
			g_printf("sound: %i\n",global->Sound_enable);
			g_printf("sound Device: %i\n",global->Sound_UseDev);
			g_printf("sound samp rate: %i\n",global->Sound_SampRateInd);
			g_printf("sound Channels: %i\n",global->Sound_NumChanInd);
			g_printf("Sound Block Size: %i seconds\n",global->Sound_NumSec);
			g_printf("Sound Format: %i \n",global->Sound_Format);
			g_printf("Sound bit Rate: %i Kbps\n",global->Sound_bitRate);
			g_printf("Pan Step: %i degrees\n",global->PanStep);
			g_printf("Tilt Step: %i degrees\n",global->TiltStep);
			g_printf("Video Filter Flags: %i\n",global->Frame_Flags);
			g_printf("image inc: %d\n",global->image_inc);
			g_printf("profile(default):%s/%s\n",global->profile_FPath[1],global->profile_FPath[0]);
		}
	}
	
	return (ret);
}

/*------------------------- read command line options ------------------------*/
void
readOpts(int argc,char *argv[], struct GLOBAL *global)
{
	gchar *device=NULL;
	gchar *format=NULL;
	gchar *size = NULL;
	gchar *image = NULL;
	gchar *video=NULL;
	gchar *profile=NULL;
	gchar *separateur=NULL;
	gboolean help = FALSE;
	gboolean help_gtk = FALSE;
	gboolean help_all = FALSE;
	gboolean vers = FALSE;
	gchar *help_str = NULL;
	gchar *help_gtk_str = NULL;
	gchar *help_all_str = NULL;
	gchar *config = NULL;
	int hwaccel=-1;
	int FpsCount=-1;
	int cap_meth=-1;
	
	GOptionEntry entries[] =
	{
		{ "help-all", 'h', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &help_all, "Display all help options", NULL},
		{ "help-gtk", '!', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &help_gtk, "DISPLAY GTK+ help", NULL},
		{ "help", '?', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &help, "Display help", NULL},
		{ "version", 0, 0, G_OPTION_ARG_NONE, &vers, N_("Prints version"), NULL},
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &global->debug, N_("Displays debug information"), NULL },
		{ "device", 'd', 0, G_OPTION_ARG_STRING, &device, N_("Video Device to use [default: /dev/video0]"), "VIDEO_DEVICE" },
		{ "control_only", 'o', 0, G_OPTION_ARG_NONE, &global->control_only, N_("Don't stream video (image controls only)"), NULL},
		{ "capture_method", 'r', 0, G_OPTION_ARG_INT, &cap_meth, N_("Capture method (1-mmap (default)  2-read)"), "[1 | 2]"},
		{ "config", 'g', 0, G_OPTION_ARG_STRING, &config, N_("Configuration file"), "FILENAME" },
		{ "hwd_acel", 'w', 0, G_OPTION_ARG_INT, &hwaccel, N_("Hardware accelaration (enable(1) | disable(0))"), "[1 | 0]" },
		{ "format", 'f', 0, G_OPTION_ARG_STRING, &format, N_("Pixel format(mjpg|jpeg|yuyv|yvyu|uyvy|yyuv|yu12|yv12|nv12|nv21|nv16|nv61|y41p|grey|s501|s505|s508|gbrg|grbg|ba81|rggb|bgr3|rgb3)"), "FORMAT" },
		{ "size", 's', 0, G_OPTION_ARG_STRING, &size, N_("Frame size, default: 640x480"), "WIDTHxHEIGHT"},
		{ "image", 'i', 0, G_OPTION_ARG_STRING, &image, N_("Image File name"), "FILENAME"},
		{ "cap_time", 'c', 0, G_OPTION_ARG_INT, &global->image_timer, N_("Image capture interval in seconds"), "TIME"},
		{ "npics", 'm', 0, G_OPTION_ARG_INT, &global->image_npics, N_("Number of Pictures to capture"), "NUMPIC"},
		{ "video", 'n', 0, G_OPTION_ARG_STRING, &video, N_("Video File name (capture from start)"), "FILENAME"},
		{ "vid_time", 't', 0, G_OPTION_ARG_INT, &global->Capture_time,N_("Video capture time (in seconds)"), "TIME"},
		{ "exit_on_close", 0, 0, G_OPTION_ARG_NONE, &global->exit_on_close, N_("Exits guvcview after closing video"), NULL},
		{ "show_fps", 'p', 0, G_OPTION_ARG_INT, &FpsCount, N_("Show FPS value (enable(1) | disable (0))"), "[1 | 0]"},
		{ "profile", 'l', 0, G_OPTION_ARG_STRING, &profile, N_("Load Profile at start"), "FILENAME"},
		{ NULL }
	};

	GError *error = NULL;
	GOptionContext *context;
	context = g_option_context_new (N_("- local options"));
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_set_prgname (PACKAGE);
	help_str = g_option_context_get_help (context, TRUE, NULL);
	help_gtk_str = g_option_context_get_help (context, FALSE, gtk_get_option_group (TRUE));
	help_all_str = g_option_context_get_help (context, FALSE, NULL);
	/*disable automatic help parsing - must clean global before exit*/
	g_option_context_set_help_enabled (context, FALSE);
	if (!g_option_context_parse (context, &argc, &argv, &error))
	{
		g_printerr ("option parsing failed: %s\n", error->message);
		g_error_free ( error );
		closeGlobals(global);
		global=NULL;
		g_printf("%s",help_all_str);
		g_free(help_all_str);
		g_free(help_str);
		g_free(help_gtk_str);
		g_option_context_free (context);
		exit (1);
	}
	
	if(vers)
	{
		//print version and exit
		//version already printed in guvcview.c
		closeGlobals(global);
		global=NULL;
		g_free(help_all_str);
		g_free(help_str);
		g_free(help_gtk_str);
		g_option_context_free (context);
		exit(0);
	}
	/*Display help message and exit*/
	if(help_all)
	{
		closeGlobals(global);
		global=NULL;
		g_printf("%s",help_all_str);
		g_free(help_all_str);
		g_free(help_str);
		g_free(help_gtk_str);
		g_option_context_free (context);
		exit(0);
	}
	else if(help)
	{
		closeGlobals(global);
		global=NULL;
		g_printf("%s",help_str);
		g_free(help_str);
		g_free(help_gtk_str);
		g_free(help_all_str);
		g_option_context_free (context);
		exit(0);
	} else if(help_gtk)
	{
		closeGlobals(global);
		global=NULL;
		g_printf("%s",help_gtk_str);
		g_free(help_str);
		g_free(help_gtk_str);
		g_free(help_all_str);
		g_option_context_free (context);
		exit(0);
	}
	
	/*regular options*/
	if(device)
	{
		gchar *basename = NULL;
		gchar *dirname = NULL;
		basename = g_path_get_basename(device);
		if(!(g_str_has_prefix(basename,"video")))
		{
			g_printerr("%s not a valid video device name\n",
				basename);
		}
		else
		{
			g_free(global->videodevice);
			global->videodevice=NULL;
			dirname = g_path_get_dirname(device);
			if(g_strcmp0(".",dirname)==0)
			{
				g_free(dirname);
				dirname=g_strdup("/dev");
			}
		
			global->videodevice = g_strjoin("/",
				dirname,
				basename,
				NULL);
			if(global->flg_config < 1)
			{
				if(g_strcmp0("video0",basename) !=0 )
				{
					g_free(global->confPath);
					global->confPath=NULL;
					global->confPath = g_strjoin("", 
						g_get_home_dir(), 
						"/.guvcviewrc-",
						basename,
						NULL);
				}
			}
		}
		g_free(dirname);
		g_free(basename);
	}
	if(config)
	{
		g_free(global->confPath);
		global->confPath=NULL;
		global->confPath = g_strdup(config);
		global->flg_config = 1;
	}
	if(format)
	{
		/*use fourcc but keep compatability with luvcview*/
		if(g_strcmp0("yuv",format)==0)
			g_snprintf(global->mode,5,"yuyv");
		else if (g_strcmp0("bggr",format)==0) // be compatible with guvcview < 1.1.4
			g_snprintf(global->mode,5,"ba81");
		else
			g_snprintf(global->mode,5,"%s",format);
		
		global->flg_mode = 1;
	}
	if(size)
	{
		global->width = (int) g_ascii_strtoull(size, &separateur, 10);
		if (*separateur != 'x') 
		{
			g_printerr("Error in size usage: -s[--size] WIDTHxHEIGHT \n");
		} 
		else 
		{
			++separateur;
			global->height = (int) g_ascii_strtoull(separateur, &separateur, 10);
			if (*separateur != 0)
				g_printerr("hmm.. don't like that!! trying this height \n");
		}
		
		global->flg_res = 1;
	}
	if(image)
	{
		global->imgFPath=splitPath(image,global->imgFPath);
		/*get the file type*/
		global->imgFormat = check_image_type(global->imgFPath[0]);
		global->flg_imgFPath = 1;
	}
	if(global->image_timer > 0 )
	{
		global->image_inc=1;
		g_printf("capturing images every %i seconds",global->image_timer);
	}
	if(video)
	{
		global->vidfile = g_strdup(video);
		global->vidFPath=splitPath(global->vidfile,global->vidFPath);
		g_printf("capturing video: %s , from start",global->vidfile);
	}
	if(profile)
	{
		global->lprofile=1;
		global->profile_FPath=splitPath(profile,global->profile_FPath);
	}
	if(hwaccel != -1 )
	{
		global->hwaccel = hwaccel;
		global->flg_hwaccel = 1;
	}
	if(FpsCount != -1)
	{
		global->FpsCount = FpsCount;
		global->flg_FpsCount = 1;
	}
	if(cap_meth != -1)
	{
		global->flg_cap_meth = TRUE;
		global->cap_meth = cap_meth;
	}
	
	//g_printf("option capture meth is %i\n", global->cap_meth);
	g_free(help_str);
	g_free(help_gtk_str);
	g_free(help_all_str);
	g_free(device);
	g_free(config);
	g_free(format);
	g_free(size);
	g_free(image);
	g_free(video);
	g_free(profile);
	g_option_context_free (context);
}

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

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <stdlib.h>

#include "../config.h"
#include "defs.h"
#include "create_video.h"
#include "v4l2uvc.h"
#include "avilib.h"
#include "globals.h"
#include "sound.h"
#include "mp2.h"
#include "ms_time.h"
#include "string_utils.h"
#include "callbacks.h"
#include "vcodecs.h"
#include "video_format.h"
#include "audio_effects.h"
#include "globals.h"

/*--------------------------- controls enable/disable --------------------------*/
/* sound controls*/
void 
set_sensitive_snd_contrls (const int flag, struct GWIDGET *gwidget)
{
	gtk_widget_set_sensitive (gwidget->SndSampleRate, flag);
	gtk_widget_set_sensitive (gwidget->SndDevice, flag);
	gtk_widget_set_sensitive (gwidget->SndNumChan, flag);
	gtk_widget_set_sensitive (gwidget->SndComp, flag);
}

/*video controls*/
void 
set_sensitive_vid_contrls (const int flag, const int sndEnable, struct GWIDGET *gwidget)
{
	/* sound and video compression controls */
	gtk_widget_set_sensitive (gwidget->VidCodec, flag);
	gtk_widget_set_sensitive (gwidget->SndEnable, flag); 
	gtk_widget_set_sensitive (gwidget->VidInc, flag);
	gtk_widget_set_sensitive (gwidget->VidFormat, flag);/*video format combobox*/
	/* resolution and input format combos   */
	gtk_widget_set_sensitive (gwidget->Resolution, flag);
	gtk_widget_set_sensitive (gwidget->InpType, flag);
	/* Video File entry and open button     */
	gtk_widget_set_sensitive (gwidget->VidFNameEntry, flag);
	gtk_widget_set_sensitive (gwidget->VidFileButt, flag);
	
	if(sndEnable > 0) 
	{
		set_sensitive_snd_contrls(flag, gwidget);
	}
    
	gwidget->vid_widget_state = flag;
}

int initVideoFile(struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct VideoFormatData *videoF = all_data->videoF;
	
	const char *compression= get_vid4cc(global->VidCodec);
	videoF->vcodec = get_vcodec_id(global->VidCodec);
	videoF->acodec = CODEC_ID_NONE;
	int ret = 0;
	int i=0;
	g_mutex_lock(videoIn->mutex);
		gboolean capVid = videoIn->capVid;
	g_mutex_unlock(videoIn->mutex);
	
	/*alloc video ring buffer*/
	if (global->videoBuff == NULL)
	{
		int framesize= videoIn->height*videoIn->width*2; /*yuyv (maximum size)*/
		/*alloc video frames to videoBuff*/
		global->videoBuff = g_new0(VidBuff,VIDBUFF_SIZE);
		for(i=0;i<VIDBUFF_SIZE;i++)
		{
			global->videoBuff[i].frame = g_new0(BYTE,framesize);
		}
	}
	//reset indexes
	global->r_ind=0;
	global->w_ind=0;
	
	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			if(videoF->AviOut != NULL)
			{
				g_free(videoF->AviOut);
				videoF->AviOut = NULL;
			}
			videoF->AviOut = g_new0(struct avi_t, 1);
			videoF->keyframe = 1;
			
			if(AVI_open_output_file(videoF->AviOut, videoIn->VidFName)<0) 
			{
				g_printerr("Error: Couldn't create Video.\n");
				capVid = FALSE;
				g_mutex_lock(videoIn->mutex);
					videoIn->capVid = capVid;
				g_mutex_unlock(videoIn->mutex);
				pdata->capVid = capVid;
				return(-1);
			} 
			else 
			{
				/*disabling sound and video compression controls*/
				set_sensitive_vid_contrls(FALSE, global->Sound_enable, gwidget);
		
				AVI_set_video(videoF->AviOut, videoIn->width, videoIn->height, 
					videoIn->fps,compression);
		  
				/* start video capture*/
				capVid = TRUE;
				g_mutex_lock(videoIn->mutex);
					videoIn->capVid = capVid;
				g_mutex_unlock(videoIn->mutex);
				pdata->capVid = capVid;
				
				/* start sound capture*/
				if(global->Sound_enable > 0) 
				{
					/*get channels and sample rate*/
					set_sound(global,pdata);
					/*set audio header for avi*/
					AVI_set_audio(videoF->AviOut, global->Sound_NumChan, 
						global->Sound_SampRate,
						global->Sound_bitRate,
						16, /*only used for PCM*/
						global->Sound_Format);
					/* Initialize sound (open stream)*/
					if(init_sound (pdata)) 
					{
						g_printerr("error opening portaudio\n");
						global->Sound_enable=0;
						gdk_threads_enter();
						gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->SndEnable),0);
						gdk_flush();
						gdk_threads_leave();
					} 
					else 
					{
						if (global->Sound_Format == ISO_FORMAT_MPEG12) 
						{
							init_MP2_encoder(pdata, global->Sound_bitRate);    
						}
					}
				
				}
			}
			break;
			
		case MKV_FORMAT:
			/*disabling sound and video compression controls*/
			set_sensitive_vid_contrls(FALSE, global->Sound_enable, gwidget);
			
			/* start sound capture*/
			if(global->Sound_enable > 0) 
			{
				/*set channels, sample rate and allocate buffers*/
				set_sound(global,pdata);
			}
			if(init_FormatContext((void *) all_data)<0)
			{
				capVid = FALSE;
				g_mutex_lock(videoIn->mutex);
					videoIn->capVid = capVid;
				g_mutex_unlock(videoIn->mutex);
				pdata->capVid = capVid;
				return (-1);
			}
			
			videoF->keyframe = 1;
			videoF->old_apts = 0;
			videoF->apts = 0;
			videoF->vpts = 0;
			
			/* start video capture*/
			capVid = TRUE;
			g_mutex_lock(videoIn->mutex);
				videoIn->capVid = capVid;
			g_mutex_unlock(videoIn->mutex);
			pdata->capVid = capVid;
			
			
			/* start sound capture*/
			if(global->Sound_enable > 0) 
			{
				/* Initialize sound (open stream)*/
				if(init_sound (pdata)) 
				{
					g_printerr("error opening portaudio\n");
					global->Sound_enable=0;
					/*will this work with the checkbox disabled?*/
					gdk_threads_enter();
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->SndEnable),0);
					gdk_flush();
					gdk_threads_leave();
				} 
				else 
				{
					if (global->Sound_Format == ISO_FORMAT_MPEG12) 
					{
						init_MP2_encoder(pdata, global->Sound_bitRate);
					}
				}
			}
			break;
			
		default:
			
			break;
	}
	
	return (ret);
}


/* Called at avi capture stop       */
static void
aviClose (struct ALL_DATA *all_data)
{
	float tottime = 0;

	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct VideoFormatData *videoF = all_data->videoF;
	struct paRecordData *pdata = all_data->pdata;

	if (!(videoF->AviOut->closed))
	{
		tottime = (float) ((int64_t) (global->Vidstoptime - global->Vidstarttime) / 1000000); // convert to miliseconds
		
		if (global->debug) g_printf("stop= %llu start=%llu \n",global->Vidstoptime,global->Vidstarttime);
		if (tottime > 0) 
		{
			/*try to find the real frame rate*/
			videoF->AviOut->fps = (double) (global->framecount * 1000) / tottime;
		}
		else 
		{
			/*set the hardware frame rate*/   
			videoF->AviOut->fps=videoIn->fps;
		}

		if (global->debug) g_printf("VIDEO: %d frames in %f ms = %f fps\n",global->framecount,tottime,videoF->AviOut->fps);
		/*------------------- close audio stream and clean up -------------------*/
		if (global->Sound_enable > 0) 
		{
			/*wait for audio to finish*/
			int stall = wait_ms( &pdata->streaming, FALSE, 10, 30 );
			if(!(stall)) 
			{
				g_printerr("WARNING:sound capture stall (still streaming(%d) \n",
					pdata->streaming);
				pdata->streaming = 0;
			}
			if (close_sound (pdata)) g_printerr("Sound Close error\n");
			if(global->Sound_Format == ISO_FORMAT_MPEG12) close_MP2_encoder();
		} 
		AVI_close (videoF->AviOut);
		global->framecount = 0;
		global->Vidstarttime = 0;
		if (global->debug) g_printf ("close avi\n");
	}
	
	g_free(videoF->AviOut);
	pdata = NULL;
	global = NULL;
	videoIn = NULL;
	videoF->AviOut = NULL;
}


void closeVideoFile(struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct paRecordData *pdata = all_data->pdata;
	struct GWIDGET *gwidget = all_data->gwidget;
	
	int i=0;
	/*we are streaming so we need to lock a mutex*/
	gboolean capVid = FALSE;
	g_mutex_lock(videoIn->mutex);
		videoIn->capVid = capVid; /*flag video thread to stop recording frames*/
	g_mutex_unlock(videoIn->mutex);
	g_mutex_lock(pdata->mutex);
		pdata->capVid = capVid;
	g_mutex_unlock(pdata->mutex);
	/*wait for flag from video thread that recording has stopped    */
	/*wait on videoIn->VidCapStop by sleeping for 200 loops of 10 ms*/
	/*(test VidCapStop == TRUE on every loop)*/
	int stall = wait_ms(&(videoIn->VidCapStop), TRUE, 10, 200);
	if( !(stall > 0) )
	{
		g_printerr("video capture stall on exit(%d) - timeout\n",
			videoIn->VidCapStop);
	}
	global->Vidstoptime = ns_time();
	
	/*free video buffer allocations*/
	if (global->videoBuff != NULL)
	{
		/*free video frames to videoBuff*/
		for(i=0;i<VIDBUFF_SIZE;i++)
		{
			g_free(global->videoBuff[i].frame);
			global->videoBuff[i].frame = NULL;
		}
		g_free(global->videoBuff);
		global->videoBuff = NULL;
	}
	//reset the indexes
	global->r_ind=0;
	global->w_ind=0;
	
	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			aviClose(all_data);
			break;
			
		case MKV_FORMAT:
			if(clean_FormatContext ((void*) all_data))
				g_printerr("matroska close returned a error\n");
			break;
			
		default:
			
			break;
	}
	if(global->debug) g_printf("enabling controls\n");
	/*enabling sound and video compression controls*/
	set_sensitive_vid_contrls(TRUE, global->Sound_enable, gwidget);
	if(global->debug) g_printf("controls enabled\n");
	global->Vidstoptime = 0;
	global->Vidstarttime = 0;
	global->framecount = 0;
}

int write_video_data(struct ALL_DATA *all_data, BYTE *buff, int size, QWORD v_ts)
{
	struct VideoFormatData *videoF = all_data->videoF;
	struct GLOBAL *global = all_data->global;
	
	int ret =0;
	
	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			ret = AVI_write_frame (videoF->AviOut, buff, size, videoF->keyframe);
			break;
		
		case MKV_FORMAT:
			videoF->vpts = v_ts;
			ret = write_video_packet (buff, size, global->fps, videoF);
			break;
			
		default:
			
			break;
	}
	
	return (ret);
}

int write_video_frame (struct ALL_DATA *all_data, 
	void *jpeg_struct, 
	void *lavc_data,
	VidBuff *proc_buff)
{
	struct GLOBAL *global = all_data->global;
	struct VideoFormatData *videoF = all_data->videoF;
	struct GWIDGET *gwidget = all_data->gwidget;
	
	GThread *press_butt_thread = NULL;
	int ret=0;
	
	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			/*all video controls are now disabled so related values cannot be changed*/
			if(!(global->VidButtPress)) //if this is set AVI reached it's limit size
				ret = compress_frame(all_data, jpeg_struct, lavc_data, proc_buff);

			if (ret)
			{
				if (AVI_getErrno () == AVI_ERR_SIZELIM)
				{
					if (!(global->VidButtPress))
					{
						global->VidButtPress = TRUE;
						GError *err1 = NULL;
						/*avi file limit reached - must end capture close file and start new one*/
						if( (press_butt_thread =g_thread_create((GThreadFunc) split_avi, 
							all_data, //data
							FALSE,    //joinable - no need waiting for thread to finish
							&err1)    //error
						) == NULL)  
						{
							/*thread failed to start - stop video capture   */
							/*can't restart since we need IO thread to stop */
							g_printerr("Thread create failed: %s!!\n", err1->message );
							g_error_free ( err1 ) ;
							printf("stoping video capture\n");
							gdk_threads_enter();
							gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
							gdk_flush();
							gdk_threads_leave();
						}
						g_printf("AVI file size limit reached - restarted capture on new file\n");
					}
				} 
				else 
				{
					g_printerr ("write error on avi out \n");
				}
			}
		   
			//global->framecount++;
			if (videoF->keyframe) videoF->keyframe=0; /*resets key frame*/
			
			break;
		
		
		case MKV_FORMAT:
			//global->framecount++;
			
			ret = compress_frame(all_data, jpeg_struct, lavc_data, proc_buff);
			break;
		
		default:
			break;
	}
	return (0);
}

int write_audio_frame (struct ALL_DATA *all_data, AudBuff *proc_buff)
{
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct VideoFormatData *videoF = all_data->videoF;
	struct GWIDGET *gwidget = all_data->gwidget;
	
	int ret =0;
	GThread *press_butt_thread;
	
	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			if(!(global->VidButtPress)) //if this is set AVI reached it's limit size
			{
				/*write audio chunk*/
				if(global->Sound_Format == PA_FOURCC) 
				{
					Float2Int16(pdata, proc_buff); /*convert from float sample to 16 bit PCM*/
					ret=AVI_write_audio(videoF->AviOut,(BYTE *) pdata->pcm_sndBuff, pdata->aud_numSamples*2);
				}
				else if(global->Sound_Format == ISO_FORMAT_MPEG12)
				{
					int size_mp2 = MP2_encode(pdata, proc_buff, 0);
					ret=AVI_write_audio(videoF->AviOut, pdata->mp2Buff, size_mp2);
				}
			}
		
			if (ret) 
			{	
				if (AVI_getErrno () == AVI_ERR_SIZELIM) 
				{
					if (!(global->VidButtPress))
					{
						global->VidButtPress = TRUE;
						GError *err1 = NULL;
						/*avi file limit reached - must end capture close file and start new one*/
						if( (press_butt_thread =g_thread_create((GThreadFunc) split_avi, 
							all_data, //data
							FALSE,    //joinable - no need waiting for thread to finish
							&err1)    //error
						) == NULL)  
						{
							/*thread failed to start - stop video capture   */
							/*can't restart since we need IO thread to stop */
							g_printerr("Thread create failed: %s!!\n", err1->message );
							g_error_free ( err1 ) ;
							printf("stoping video capture\n");
							gdk_threads_enter();
							gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
							gdk_flush();
							gdk_threads_leave();
						}
					
						//split_avi(all_data);/*blocking call*/
						g_printf("AVI file size limit reached - restarted capture on new file\n");
					}
				} 
				else 
				{
					g_printerr ("write error on avi out \n");
				}
					
			}
			break;
		case MKV_FORMAT:
			g_mutex_lock( pdata->mutex ); //why do we need this ???
				/*set pts*/
				videoF->apts = proc_buff->time_stamp;
					
				/*write audio chunk*/
				if(global->Sound_Format == PA_FOURCC) 
				{
					Float2Int16(pdata, proc_buff); /*convert from float sample to 16 bit PCM*/
					ret = write_audio_packet ((BYTE *) pdata->pcm_sndBuff, pdata->aud_numSamples*2, pdata->samprate, videoF);
				}
				else if(global->Sound_Format == ISO_FORMAT_MPEG12)
				{
					int size_mp2 = MP2_encode(pdata, proc_buff, 0);
					ret = write_audio_packet (pdata->mp2Buff, size_mp2, pdata->samprate, videoF);
				}
			g_mutex_unlock( pdata->mutex );
			break;
			
		default:
			
			break;
	}
	return (0);
}

int sync_audio_frame(struct ALL_DATA *all_data, AudBuff *proc_buff)
{
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct VideoFormatData *videoF = all_data->videoF;
	
	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			/*first audio data - sync with video (audio stream capture can take               */
			/*a bit longer to start)                                                          */
			/*no need of locking the audio mutex yet, since we are not reading from the buffer*/
			if (!(videoF->AviOut->track[0].audio_bytes)) 
			{ 
				/*only 1 audio stream*/
				/*time diff for audio-video*/
				int synctime= (int) (pdata->snd_begintime - global->Vidstarttime)/1000000; /*convert to miliseconds*/
				if (global->debug) g_printf("shift sound by %d ms\n", synctime);
				if(synctime>10 && synctime<5000) 
				{ 	/*only sync between 10ms and 5 seconds*/
					if(global->Sound_Format == PA_FOURCC) 
					{	/*shift sound by synctime*/
						UINT32 shiftFrames = abs(synctime * global->Sound_SampRate / 1000);
						UINT32 shiftSamples = shiftFrames * global->Sound_NumChan;
						if (global->debug) g_printf("shift sound forward by %d samples\n", 
							shiftSamples);
						short *EmptySamp;
						EmptySamp=g_new0(short, shiftSamples);
						AVI_write_audio(videoF->AviOut,(BYTE *) EmptySamp,shiftSamples*sizeof(short));
						g_free(EmptySamp);
					} 
					else if(global->Sound_Format == ISO_FORMAT_MPEG12) 
					{
						int size_mp2 = MP2_encode(pdata, proc_buff, synctime);
						if (global->debug) g_printf("shift sound forward by %d bytes\n",size_mp2);
						AVI_write_audio(videoF->AviOut,pdata->mp2Buff,size_mp2);
					}
				}
			}
			break;
		
		case MKV_FORMAT:
			
			break;
		
		default:
			break;
	}
	
	return (0);
}

static int buff_scheduler(int w_ind, int r_ind)
{
	int diff_ind = 0;
	int vid_sleep = 0;
	//try to balance buffer overrun in read/write operations 
	if(w_ind >= r_ind)
		diff_ind = w_ind - r_ind;
	else
		diff_ind = (VIDBUFF_SIZE - r_ind) + w_ind;

	if(diff_ind <= 15) vid_sleep = 0; /* full throtle (must wait for audio at least 10 frames) */
	else if (diff_ind <= 20) vid_sleep = (diff_ind-15) * 2; /* <= 10  ms ~1 frame @ 90  fps */
	else if (diff_ind <= 25) vid_sleep = (diff_ind-15) * 3; /* <= 30  ms ~1 frame @ 30  fps */
	else if (diff_ind <= 30) vid_sleep = (diff_ind-15) * 4; /* <= 60  ms ~1 frame @ 15  fps */
	else if (diff_ind <= 35) vid_sleep = (diff_ind-15) * 5; /* <= 100 ms ~1 frame @ 10  fps */
	else if (diff_ind <= 40) vid_sleep = (diff_ind-15) * 6; /* <= 130 ms ~1 frame @ 7,5 fps */
	else vid_sleep = (diff_ind-15) * 7;                     /* <= 210 ms ~1 frame @ 5   fps */
	
	return vid_sleep;
}

int store_video_frame(void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;

	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	int ret = 0;
	
	g_mutex_lock(global->mutex);
	
	
	if (!global->videoBuff[global->w_ind].used)
	{
		global->videoBuff[global->w_ind].time_stamp = global->v_ts;
		/*store frame at index*/
		if((global->VidCodec == CODEC_MJPEG) &&
			(global->Frame_Flags==0) &&
			(videoIn->formatIn==V4L2_PIX_FMT_MJPEG))
		{
			/*store MJPEG frame*/
			global->videoBuff[global->w_ind].bytes_used = videoIn->buf.bytesused;
			memcpy(global->videoBuff[global->w_ind].frame, 
				videoIn->tmpbuffer, 
				global->videoBuff[global->w_ind].bytes_used);
		}
		else
		{
			/*store YUYV frame*/
			global->videoBuff[global->w_ind].bytes_used = videoIn->height*videoIn->width*2;
			memcpy(global->videoBuff[global->w_ind].frame, 
				videoIn->framebuffer, 
				global->videoBuff[global->w_ind].bytes_used);
		}
		global->videoBuff[global->w_ind].used = TRUE;
		
		global->vid_sleep = buff_scheduler(global->w_ind, global->r_ind);
		
		NEXT_IND(global->w_ind, VIDBUFF_SIZE);
	}
	else
	{
		if(global->debug) g_printerr("WARNING: buffer full waiting for free space\n");
		/*wait for IO_cond at least 200ms*/
		GTimeVal *timev;
		timev = g_new0(GTimeVal, 1);
		g_get_current_time(timev);
		g_time_val_add(timev,200*1000); /*200 ms*/
		if(g_cond_timed_wait(global->IO_cond, global->mutex, timev))
		{
			/*try to store the frame again*/
			if (!global->videoBuff[global->w_ind].used)
			{
				global->videoBuff[global->w_ind].time_stamp = global->v_ts;
				/*store frame at index*/
				if((global->VidCodec == CODEC_MJPEG) &&
					(global->Frame_Flags==0) &&
					(videoIn->formatIn==V4L2_PIX_FMT_MJPEG))
				{
					/*store MJPEG frame*/
					global->videoBuff[global->w_ind].bytes_used = videoIn->buf.bytesused;
					memcpy(global->videoBuff[global->w_ind].frame, 
						videoIn->tmpbuffer, 
						global->videoBuff[global->w_ind].bytes_used);
				}
				else
				{
					/*store YUYV frame*/
					global->videoBuff[global->w_ind].bytes_used = videoIn->height*videoIn->width*2;
					memcpy(global->videoBuff[global->w_ind].frame, 
						videoIn->framebuffer, 
						global->videoBuff[global->w_ind].bytes_used);
				}
				global->videoBuff[global->w_ind].used = TRUE;
				NEXT_IND(global->w_ind, VIDBUFF_SIZE);
			}
			else ret = -1;/*drop frame*/
			
		}
		else ret = -2;/*drop frame*/
		
		g_free(timev);
	}
	if(!ret) global->framecount++;
	g_mutex_unlock(global->mutex);
	
	return ret;
}

static void process_audio(struct ALL_DATA *all_data, 
			AudBuff *aud_proc_buff, 
			struct audio_effects **aud_eff)
{
	struct paRecordData *pdata = all_data->pdata;

	g_mutex_lock( pdata->mutex );
		gboolean used = pdata->audio_buff[pdata->r_ind].used;
    	g_mutex_unlock( pdata->mutex );
  
	/*read at most 10 audio Frames (1152 * channels  samples each)*/
	if(used)
	{
	    	g_mutex_lock( pdata->mutex );
			memcpy(aud_proc_buff->frame, pdata->audio_buff[pdata->r_ind].frame, pdata->aud_numSamples*sizeof(SAMPLE));
			pdata->audio_buff[pdata->r_ind].used = FALSE;
			aud_proc_buff->time_stamp = pdata->audio_buff[pdata->r_ind].time_stamp;
			NEXT_IND(pdata->r_ind, AUDBUFF_SIZE);	
		g_mutex_unlock( pdata->mutex ); /*now we should be able to unlock the audio mutex*/	
		sync_audio_frame(all_data, aud_proc_buff);	
		/*run effects on data*/
		/*echo*/
		if((pdata->snd_Flags & SND_ECHO)==SND_ECHO) 
		{
			Echo(pdata, aud_proc_buff, *aud_eff, 300, 0.5);
		}
		else
		{
			close_DELAY((*aud_eff)->ECHO);
			(*aud_eff)->ECHO = NULL;
		}
		/*fuzz*/
		if((pdata->snd_Flags & SND_FUZZ)==SND_FUZZ) 
		{
			Fuzz(pdata, aud_proc_buff, *aud_eff);
		}
		else
		{
			close_FILT((*aud_eff)->HPF);
			(*aud_eff)->HPF = NULL;
		}
		/*reverb*/
		if((pdata->snd_Flags & SND_REVERB)==SND_REVERB) 
		{
			Reverb(pdata, aud_proc_buff, *aud_eff, 50);
		}
		else
		{
			close_REVERB(*aud_eff);
		}
		/*wahwah*/
		if((pdata->snd_Flags & SND_WAHWAH)==SND_WAHWAH) 
		{
			WahWah (pdata, aud_proc_buff, *aud_eff, 1.5, 0, 0.7, 0.3, 2.5);
		}
		else
		{
			close_WAHWAH((*aud_eff)->wahData);
			(*aud_eff)->wahData = NULL;
		}
		/*Ducky*/
		if((pdata->snd_Flags & SND_DUCKY)==SND_DUCKY) 
		{
			change_pitch(pdata, aud_proc_buff, *aud_eff, 2);
		}
		else
		{
			close_pitch (*aud_eff);
		}
			
		write_audio_frame(all_data, aud_proc_buff);
	}
}

static gboolean process_video(struct ALL_DATA *all_data, 
				VidBuff *proc_buff, 
				struct lavcData **lavc_data, 
				struct JPEG_ENCODER_STRUCTURE **jpeg_struct)
{
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	g_mutex_lock(videoIn->mutex);
		gboolean capVid = videoIn->capVid;
	g_mutex_unlock(videoIn->mutex);
	
	gboolean finish = FALSE;
	
	g_mutex_lock(global->mutex);
    		gboolean used = global->videoBuff[global->r_ind].used;
    	g_mutex_unlock(global->mutex);
	if (used)
	{
		g_mutex_lock(global->mutex);
	    		/*read video Frame*/
			proc_buff->bytes_used = global->videoBuff[global->r_ind].bytes_used;
			memcpy(proc_buff->frame, global->videoBuff[global->r_ind].frame, proc_buff->bytes_used);
			proc_buff->time_stamp = global->videoBuff[global->r_ind].time_stamp;
			global->videoBuff[global->r_ind].used = FALSE;
			/*signals an empty slot in the video buffer*/
			g_cond_broadcast(global->IO_cond);
				
			NEXT_IND(global->r_ind,VIDBUFF_SIZE);
		g_mutex_unlock(global->mutex);

		/*process video Frame*/
		write_video_frame(all_data, (void *) jpeg_struct, (void *) lavc_data, proc_buff);
	}
	else
	{
		if (capVid)
		{
			/*video buffer underrun            */
			/*wait for next frame (sleep 10 ms)*/
			sleep_ms(10);
		}
		else 
		{
			finish = TRUE; /*all frames processed and no longer capturing so finish*/
		}
	}
	return finish;
}

void *IO_loop(void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	
	struct GWIDGET *gwidget = all_data->gwidget;
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;

	struct JPEG_ENCODER_STRUCTURE *jpg_data=NULL;
	struct lavcData *lavc_data = NULL;
	struct audio_effects *aud_eff = NULL;
	
	gboolean finished=FALSE;
	videoIn->IOfinished=FALSE;
	
    	gboolean capVid=FALSE;
    
	gboolean failed = FALSE;
	int proc_flag = 0;
	int diff_ind=0;
	global->r_ind = 0;
	pdata->r_ind = 0;
	
	//buffers to be processed (video and audio)
	int frame_size=0;
	VidBuff *proc_buff = NULL;
	AudBuff *aud_proc_buff = NULL;
	
	if(initVideoFile(all_data)<0)
	{
		g_printerr("Cap Video failed\n");
		
		gdk_threads_enter();
		/*disable signals for video capture callback*/
		g_signal_handlers_block_by_func(GTK_TOGGLE_BUTTON(gwidget->CapVidButt), G_CALLBACK (capture_vid), all_data);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
		gtk_button_set_label(GTK_BUTTON(gwidget->CapVidButt),_("Cap. Video"));
		/*enable signals for video capture callback*/
		g_signal_handlers_unblock_by_func(GTK_TOGGLE_BUTTON(gwidget->CapVidButt), G_CALLBACK (capture_vid), all_data);
		gdk_flush();
		gdk_threads_leave();
		
		finished = TRUE;
		failed = TRUE;
	}
	else
	{
		if(global->debug) g_printf("IO thread started...OK\n");
		frame_size = videoIn->height*videoIn->width*2;
		proc_buff = g_new0(VidBuff, 1);
		proc_buff->frame = g_new0(BYTE, frame_size);
	
		if (global->Sound_enable) 
		{
			aud_eff=init_audio_effects ();
			aud_proc_buff = g_new0(AudBuff, 1);
			aud_proc_buff->frame = g_new0(SAMPLE, pdata->aud_numSamples);
		}
	}
	
	if(!failed)
	{
		while(!finished)
		{
			if(global->Sound_enable)
			{
				g_mutex_lock(videoIn->mutex);
					capVid = videoIn->capVid;
				g_mutex_unlock(videoIn->mutex);

				g_mutex_lock( pdata->mutex );
				g_mutex_lock( global->mutex );
					//check read/write index delta in frames 
					if(global->w_ind >= global->r_ind)
						diff_ind = global->w_ind - global->r_ind;
					else
						diff_ind = (VIDBUFF_SIZE - global->r_ind) + global->w_ind;
			
					if( (pdata->audio_buff[pdata->r_ind].used && global->videoBuff[global->r_ind].used) &&
						(pdata->audio_buff[pdata->r_ind].time_stamp < global->videoBuff[global->r_ind].time_stamp))
					{
						proc_flag = 1;	//process audio
					}
					else if(pdata->audio_buff[pdata->r_ind].used && global->videoBuff[global->r_ind].used)
					{
						proc_flag = 2;    //process video
					}
					else if (diff_ind < 10 && capVid)
					{
						proc_flag = 3;	//sleep -wait for audio (at most 10 video frames)
					}
					else if(pdata->audio_buff[pdata->r_ind].used)
					{	
						proc_flag = 1;    //process audio
					}
					else proc_flag = 2;    //process video
				g_mutex_unlock( global->mutex );
				g_mutex_unlock( pdata->mutex );
			
			
				switch(proc_flag)
				{
					case 1:
						process_audio(all_data, aud_proc_buff, &(aud_eff));
						break;
					case 2:
						finished = process_video (all_data, proc_buff, &(lavc_data), &(jpg_data));
						break;
					default:
						sleep_ms(10);
						break;
				}
			}
			else
			{
				//process video
				finished = process_video (all_data, proc_buff, &(lavc_data), &(jpg_data));
			}
		
			if(finished)
			{
				/*wait for audio to finish*/
				int stall = wait_ms( &pdata->streaming, FALSE, 10, 30 );
				if(!(stall)) 
				{
					g_printerr("WARNING:sound capture stall (still streaming(%d) \n",
						pdata->streaming);
					pdata->streaming = 0;
				}
				process_audio(all_data, aud_proc_buff, &(aud_eff)); //process last audio if any
			}
		}
	
		/*finish capture*/
		closeVideoFile(all_data);
	
		/*free proc buffer*/
		g_free(proc_buff->frame);
		g_free(proc_buff);
		if(aud_proc_buff) 
		{
			g_free(aud_proc_buff->frame);
			g_free(aud_proc_buff);
		}
		close_audio_effects (aud_eff);
	}
	
	if(lavc_data != NULL)
	{
		int nf = clean_lavc(&lavc_data);
		if(global->debug) g_printf(" total frames encoded: %d\n", nf);
		lavc_data = NULL;
	}
	if(jpg_data != NULL) g_free(jpg_data);
	jpeg_data = NULL;
	
	if(global->jpeg != NULL) g_free(global->jpeg); //jpeg buffer used in encoding
	
	/*make sure video thread returns to full throtle*/
	global->vid_sleep = 0;
	if(global->debug) g_printf("IO thread finished...OK\n");

	global->VidButtPress = FALSE;
	videoIn->IOfinished=TRUE;
	
	return ((void *) 0);
}


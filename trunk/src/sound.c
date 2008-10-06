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

#include "sound.h"
#include "ms_time.h"
#include "globals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <portaudio.h>

/*--------------------------- sound callback ------------------------------*/
int 
recordCallback (const void *inputBuffer, void *outputBuffer,
			   unsigned long framesPerBuffer,
			   const PaStreamCallbackTimeInfo* timeInfo,
			   PaStreamCallbackFlags statusFlags,
			   void *userData )
{
    struct paRecordData *data = (struct paRecordData*)userData;
    const SAMPLE *rptr = (const SAMPLE*)inputBuffer;
    int i;
    int numSamples=framesPerBuffer*data->channels;
    
    /*data->streaming is also set by close_audio                  */
    /*avoids writing to primary buffer (it may have been freed)   */
    /*since there is a wait routine in close_audio this shouldn't */
    /*really be needed, in any case ...                           */    
    if (data->streaming) {
	data->recording = 1;
	if( inputBuffer == NULL )
	{
		for( i=0; i<numSamples; i++ )
		{
			data->recordedSamples[data->sampleIndex] = 0;/*silence*/
			data->sampleIndex++;
		}
	}
	else
	{
		for( i=0; i<numSamples; i++ )
		{
			data->recordedSamples[data->sampleIndex] = *rptr++;
			data->sampleIndex++;
		}
	}
    
	data->numSamples += numSamples;
	if (data->numSamples > (data->maxIndex-2*numSamples)) 
	{
		//primary buffer near limit (don't wait for overflow)
		//or video capture stopped
		//copy data to secondary buffer and restart primary buffer index
		data->snd_numBytes = data->numSamples*sizeof(SAMPLE);
		memcpy(data->avi_sndBuff, data->recordedSamples ,data->snd_numBytes);
		data->sampleIndex=0;
		data->numSamples = 0;
		//flags that secondary buffer as data (can be saved to file)
		data->audio_flag=1;
	}
	data->recording = 0;
    }
    
    if(data->capAVI) return (paContinue);
    else {
        /*recording stopped*/
	if(!(data->audio_flag)) {
		data->recording = 1;
		/*need to copy audio to secondary buffer*/
		data->snd_numBytes = data->numSamples*sizeof(SAMPLE);
		memcpy(data->avi_sndBuff, data->recordedSamples , data->snd_numBytes);
		data->sampleIndex=0;
		data->numSamples = 0;
		//flags that secondary buffer as data (can be saved to file)
		data->audio_flag=1;
		data->recording = 0;
	}
	data->streaming=0;
	return (paComplete);
    }
}

void
set_sound (struct GLOBAL *global, struct paRecordData* data) {
	if(global->Sound_SampRateInd==0)
	   global->Sound_SampRate=global->Sound_IndexDev[global->Sound_UseDev].samprate;/*using default*/
	
	if(global->Sound_NumChanInd==0) {
	   /*using default if channels <3 or stereo(2) otherwise*/
	   global->Sound_NumChan=(global->Sound_IndexDev[global->Sound_UseDev].chan<3)?global->Sound_IndexDev[global->Sound_UseDev].chan:2;
	}
	
	data->samprate = global->Sound_SampRate;
	data->channels = global->Sound_NumChan;
	data->numsec = global->Sound_NumSec;
	/*set audio device to use*/
	data->inputParameters.device = global->Sound_IndexDev[global->Sound_UseDev].id; /* input device */
}

/*no need of extra thread can be set in video thread*/
int
init_sound(struct paRecordData* data)
{
	PaError err;
	int i;
	int totalFrames;
	int numSamples;
	
    	/* setting maximum buffer size*/
	totalFrames = data->numsec * data->samprate;
	numSamples = totalFrames * data->channels;
	data->snd_numBytes = numSamples * sizeof(SAMPLE);
    
	data->recordedSamples = (SAMPLE *) malloc( data->snd_numBytes ); /*capture buffer*/
    	data->maxIndex = numSamples;
    	data->sampleIndex=0;
    	data->streaming=1;
	data->recording = 0;
    	
	data->avi_sndBuff = (SAMPLE *) malloc( data->snd_numBytes );/*secondary shared buffer*/
    
	if( data->recordedSamples == NULL )
	{
		printf("Could not allocate record array.\n");
		return (-2);
	}
	for( i=0; i<numSamples; i++ ) data->recordedSamples[i] = 0;
	
	err = Pa_Initialize();
	if( err != paNoError ) goto error;
	
	/* Record for a few seconds. */
	data->inputParameters.channelCount = data->channels;
	data->inputParameters.sampleFormat = PA_SAMPLE_TYPE;
	data->inputParameters.suggestedLatency = Pa_GetDeviceInfo( data->inputParameters.device )->defaultLowInputLatency;
	data->inputParameters.hostApiSpecificStreamInfo = NULL; 
	
	/*---------------------------- Record some audio. ----------------------------- */
	/* Input buffer will be twice(default) the size of frames to read               */
	/* This way even in slow machines it shouldn't overflow and drop frames         */
	err = Pa_OpenStream(
			  &data->stream,
			  &data->inputParameters,
			  NULL,                  /* &outputParameters, */
			  data->samprate,
			  paFramesPerBufferUnspecified,/* buffer Size - totalFrames*/
			  paNoFlag,      /* PaNoFlag - clip and dhiter*/
			  recordCallback, /* sound callback - using blocking API*/
			  data ); /* callback userData -no callback no data */
	if( err != paNoError ) goto error;
    
	err = Pa_StartStream( data->stream );
	if( err != paNoError ) goto error; /*should close the stream if error ?*/
    
    	/*sound start time - used to sync with video*/
	data->snd_begintime = ms_time();
    
    	return (0);
error:
	fprintf( stderr, "An error occured while using the portaudio stream\n" );
	fprintf( stderr, "Error number: %d\n", err );
	fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) ); 
	data->streaming=0;
	if(data->recordedSamples) free( data->recordedSamples );
	data->recordedSamples=NULL;
	if(data->avi_sndBuff) free(data->avi_sndBuff);
	data->avi_sndBuff=NULL;
	Pa_Terminate();
	return(-1);

}	   

int
close_sound (struct paRecordData *data) 
{
    	int stall=20;
        int err =0;
	
	/*wait for last audio chunks to be saved on video file*/
    	while ((data->streaming || (data->audio_flag>0)) && (stall>0)) {
		Pa_Sleep(100);
		stall--; /*prevents stalls (waits at max 20*100 ms)*/
	}
    	if(!(stall>0)) printf("WARNING:sound capture stall (streaming=%d flag=%d)\n",
	                                          data->streaming, data->audio_flag);
	
	data->streaming=0;    /*prevents writes on primary and secondary buffers*/
	
	err = Pa_StopStream( data->stream );
	if( err != paNoError ) goto error;
	
	/*free primary buffer*/
	if(!data->recording) {
		if(data->recordedSamples) free( data->recordedSamples  );
		data->recordedSamples=NULL;
	} else {
		fprintf( stderr, "Error: still recording audio couldn't free P. buffer\n" );
		return(-1);
	}
	
	err = Pa_CloseStream( data->stream ); /*closes the stream*/
	if( err != paNoError ) goto error; 
	
	Pa_Terminate();
	
	data->audio_flag=0; /*prevents reads on secondary buffer */
	
	/*free secondary buffer*/
	if(!data->recording) {
		if (data->avi_sndBuff) free(data->avi_sndBuff);
		data->avi_sndBuff = NULL;
	} else {
		fprintf( stderr, "Error: still recording audio couldn't free S. buffer\n" );
		return(-1);
	}
	return (0);
error:  
	fprintf( stderr, "An error occured while closing the portaudio stream\n" );
	fprintf( stderr, "Error number: %d\n", err );
	fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
	
	Pa_Terminate();
	if(data->recordedSamples) free( data->recordedSamples );
	data->recordedSamples=NULL;
	if (data->avi_sndBuff) free(data->avi_sndBuff);
    	data->avi_sndBuff = NULL;
	return(-1);
}
/*                 Matroska frame-extractor
#
#              Paulo Assis <pj.assis@gmail.com
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version. 
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
# GNU General Public License for more details.
*/

/* compile with: gcc matroska_extractor -o mkv_frame_extract
# usage: mkv_frame_extract /path/to/mkv_file.mkv
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>

#define EBML_ID_HEADER              0x1A45DFA3
#define MATROSKA_ID_SEGMENT         0x18538067
#define MATROSKA_ID_TIMECODESCALE   0x2AD7B1
#define MATROSKA_ID_INFO            0x1549A966
#define MATROSKA_ID_TRACKS          0x1654AE6B
#define MATROSKA_ID_TRACKENTRY      0xAE
#define MATROSKA_ID_TRACKNUMBER     0xD7
#define MATROSKA_ID_TRACKTYPE       0x83
#define MATROSKA_ID_TRACKAUDIO      0xE1
#define MATROSKA_ID_TRACKVIDEO      0xE0
#define MATROSKA_ID_CLUSTER         0x1F43B675
#define MATROSKA_ID_CLUSTERTIMECODE 0xE7
#define MATROSKA_ID_CLUSTERPOSITION 0xA7
#define MATROSKA_ID_CLUSTERPREVSIZE 0xAB
#define MATROSKA_ID_BLOCKGROUP      0xA0
#define MATROSKA_ID_SIMPLEBLOCK     0xA3
#define MATROSKA_ID_BLOCK           0xA1
#define MATROSKA_ID_BLOCKDURATION   0x9B
#define MATROSKA_ID_BLOCKREFERENCE  0xFB

//JPEG stuff
typedef struct tagJPGFILEHEADER 
{
	uint8_t SOI[2];/*SOI Marker 0xFFD8*/
	uint8_t APP0[2];/*APP0 MARKER 0xFF0E*/
	uint8_t length[2];/*length of header without APP0 in uint8_ts*/
	uint8_t JFIF[5];/*set to JFIF0 0x4A46494600*/
	uint8_t VERS[2];/*1-2 0x0102*/
	uint8_t density;/* 0 - No units, aspect ratio only specified
		        1 - Pixels per Inch on quickcam5000pro
			2 - Pixels per Centimetre                */
	uint8_t xdensity[2];/*120 on quickcam5000pro*/
	uint8_t ydensity[2];/*120 on quickcam5000pro*/
	uint8_t WTN;/*width Thumbnail 0*/
	uint8_t HTN;/*height Thumbnail 0*/	
} __attribute__ ((packed)) JPGFILEHEADER, *PJPGFILEHEADER;

#define JPG_HUFFMAN_TABLE_LENGTH 0x01A0

static const unsigned char JPEGHuffmanTable[JPG_HUFFMAN_TABLE_LENGTH] = 
{
	// luminance dc - length bits	
	0x00, 
	0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	// luminance dc - code
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
	0x0A, 0x0B, 
	// chrominance dc - length bits	
	0x01, 
	0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	// chrominance dc - code
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
	0x0A, 0x0B, 
	// luminance ac - number of codes with # bits (ordered by code length 1-16)
	0x10,
	0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 
	0x04, 0x04, 0x00, 0x00, 0x01, 0x7D,
	// luminance ac - run size (ordered by code length)	
	0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31,
	0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32,
	0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52,
	0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16,
	0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A,
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45,
	0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
	0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x83,
	0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94,
	0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5,
	0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
	0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
	0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8,
	0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8,
	0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
	0xF9, 0xFA, 
	// chrominance ac -number of codes with # bits (ordered by code length 1-16)
	0x11, 
	0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05,
	0x04, 0x04, 0x00, 0x01, 0x02, 0x77,
	// chrominance ac - run size (ordered by code length)
	0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06,
	0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81,
	0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33,
	0x52, 0xF0, 0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34,
	0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26, 0x27, 0x28,
	0x29, 0x2A, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44,
	0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56,
	0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
	0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A,
	0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92,
	0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3,
	0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4,
	0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
	0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,
	0xD7, 0xD8, 0xD9, 0xDA, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
	0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
	0xF9, 0xFA
};

int 
saveJPG(const char *Filename, int imgsize, uint8_t *ImagePix) 
{
	int ret=0;
	int jpgsize=0;
	uint8_t *jpgtmp=NULL;
	uint8_t *Pimg=NULL;
	uint8_t *Pjpg=NULL;
	uint8_t *tp=NULL;	
	JPGFILEHEADER JpgFileh;
	int jpghsize=sizeof(JpgFileh);
	FILE *fp;
	jpgsize=jpghsize+imgsize+sizeof(JPEGHuffmanTable)+4;/*header+huffman+marker+buffsize*/
	Pimg=ImagePix;
	
	if((jpgtmp = malloc(jpgsize))!=NULL) 
	{
		Pjpg=jpgtmp;
		/*Fill JFIF header*/
		JpgFileh.SOI[0]=0xff;
		JpgFileh.SOI[1]=0xd8;
		JpgFileh.APP0[0]=0xff;
		JpgFileh.APP0[1]=0xe0;
		JpgFileh.length[0]=0x00;
		JpgFileh.length[1]=0x10;
		JpgFileh.JFIF[0]=0x4a;//JFIF0
		JpgFileh.JFIF[1]=0x46;
		JpgFileh.JFIF[2]=0x49;
		JpgFileh.JFIF[3]=0x46;
		JpgFileh.JFIF[4]=0x00;
		JpgFileh.VERS[0]=0x01;//version 1.2
		JpgFileh.VERS[1]=0x02;
		JpgFileh.density=0x00;
		JpgFileh.xdensity[0]=0x00;
		JpgFileh.xdensity[1]=0x78;
		JpgFileh.ydensity[0]=0x00;
		JpgFileh.ydensity[1]=0x78;
		JpgFileh.WTN=0;
		JpgFileh.HTN=0;
	
		/*adds header (JFIF)*/
		memmove(Pjpg,&JpgFileh,jpghsize);
		/*moves to the end of the header struct (JFIF)*/
		Pjpg+=jpghsize;
		int headSize = ImagePix[4]*256+ImagePix[5] + 4;/*length + SOI+APP0*/
		/*moves to the end of header (MJPG)*/
		Pimg+=headSize;
		/*adds Quantization tables and everything else until   * 
		* start of frame marker (FFC0) 	 */
		tp=Pimg;
		int qtsize=0;
		while(!((tp[qtsize]== 0xff) && (tp[qtsize+1]== 0xc0))) 
		{
			qtsize++;
		}
		memmove(Pjpg,Pimg,qtsize);
		/*moves to the begining of frame marker*/
		Pjpg+=qtsize; 
		Pimg+=qtsize;
		/*insert huffman table with marker (FFC4) and length(x01a2)*/
		uint8_t HUFMARK[4];
		HUFMARK[0]=0xff;
		HUFMARK[1]=0xc4;
		HUFMARK[2]=0x01;
		HUFMARK[3]=0xa2;
		memmove(Pjpg,&HUFMARK,4);
		Pjpg+=4;
		memmove(Pjpg,&JPEGHuffmanTable,JPG_HUFFMAN_TABLE_LENGTH);/*0x01a0*/
		/*moves to the end of huffman tables (JFIF)*/
		Pjpg+=JPG_HUFFMAN_TABLE_LENGTH;
		/*copys frame data(JFIF)*/
		memmove(Pjpg,Pimg,(imgsize-(Pimg-ImagePix)));
		Pjpg+=imgsize-(Pimg-ImagePix);
		
		int totSize = Pjpg - jpgtmp;
	
		if ((fp = fopen(Filename,"wb"))!=NULL) 
		{
			ret=fwrite(jpgtmp,totSize,1,fp);/*jpeg - jfif*/
			if (ret< 1) ret=1; //write error 
			else ret=0;
			fflush(fp); //flush data stream to file system
			if(fsync(fileno(fp)) || fclose(fp))
				perror("JPEG ERROR - couldn't write to file");
		} 
		else ret=1;

		free(jpgtmp);
		jpgtmp=NULL;
		Pimg=NULL;
		Pjpg=NULL;
		tp=NULL;
	}
	else 
	{
		printf("could not allocate memmory for jpg file\n");
		ret=1;
	}
	return ret;
}


//matroska stuff
int decode_size_size(uint8_t val)
{
    if(val >= 0x80)
        return 1;
    if(val >= 0x40)
        return 2;
    if(val >= 0x20)
        return 3;
    if(val >= 0x10)
        return 4;
    if(val >= 0x08)
        return 5;
    if(val >= 0x04)
        return 6;
    if(val >= 0x02)
        return 7;
    if(val >= 0x01)
        return 8;
}

int decode_first_size_uint8_t(int val, int ssize)
{
    switch(ssize)
    {
        case 1:
            val &= 0x0000007f;
            break;
        case 2:
            val &= 0x0000003f;
            break;
        case 3:
            val &= 0x0000001f;
            break;
        case 4:
            val &= 0x0000000f;
            break;
        case 5:
            val &= 0x00000007;
            break;
        case 6:
            val &= 0x00000003;
            break;
        case 7:
            val &= 0x00000001;
            break;
        case 8:
            val &= 0x00000000;
            break;
    };
    return (val);
}

uint64_t get_size(FILE *fp)
{
    uint64_t size = 0;
    //get size
    int next_byte = fgetc(fp);
    if(next_byte != EOF)
    {
        next_byte &= 0x000000ff;
        int ssize = decode_size_size((uint8_t) next_byte);
        int suint8_t = decode_first_size_uint8_t(next_byte, ssize);
        size = suint8_t;
            
        int i=0;
        //get any remaining size uint8_ts
        for(i=1; i<ssize; i++)
        {
            next_byte = fgetc(fp);
            if(next_byte != EOF)
            {
                next_byte &= 0x000000ff;
                size <<= 8;
                size += next_byte; 
            }
            else
            {
                if(ferror(fp))
                    printf("An error ocurred when reading the file\n");
                
                fclose(fp);
                return(-2);
            }
        }
        //printf("value is 0x%016X\n", size);
            
    }
    else
    {
        if(ferror(fp))
            printf("An error ocurred when reading the file\n");
             
        fclose(fp);
        return(-2);
    }

    return (size);  
}

uint64_t get_ts(FILE *fp, uint64_t size)
{
    int next_byte = 0;
    uint64_t ts = 0;
    int i = 0;
    for(i=0; i<size; i++)
    {
        next_byte = fgetc(fp);
        if(next_byte != EOF)
        {
            next_byte &= 0x000000ff;
            ts <<= 8;
            ts += next_byte; 
        }
        else
        {
            if(ferror(fp))
                printf("An error ocurred when reading the file\n");
                
            fclose(fp);
            return(-2);
        }
    }
    //printf("timestamp is 0x%016X\n", ts);
    return(ts);
}

uint32_t get_frame_delta(FILE *fp)
{
    //allways 3 uint8_ts
    uint32_t delta = 0;
    uint8_t next_byte =0;
    int i = 0;
    for(i=0; i<2; i++)
    {
        next_byte = fgetc(fp);
        if(next_byte != EOF)
        {
            next_byte &= 0x000000ff;
            delta <<= 8;
            delta += next_byte; 
        }
        else
        {
            if(ferror(fp))
                printf("An error ocurred when reading the file\n");
                
            fclose(fp);
            return(-2);
        }
    }
    return delta;
}

int main(int argc, char *argv[])
{

    if(argc < 2)
        printf("usage: %s matroska_filename\n", argv[0]);

    char* filename = argv[1];
    
    uint8_t* in_frame = calloc(800*600,1);
    
    FILE *fp;
    if ((fp = fopen(filename,"rb"))!=NULL) {  // (rb) read in binary mode
        printf("opening %s\n", filename);
    } else {
        printf("couldn't open %s for read\n",filename);
        free(in_frame);
        return (-1);
    }
    int i = 0;
    uint64_t timecode_scale = 0;
    uint64_t cluster_file_pos = 0;
    uint64_t end_cluster_file_pos = 0;
    uint64_t cluster_ts = 0;
    uint64_t block_ts = 0;
    uint32_t ID = 0x00000000;
    int next_byte = 0;
    uint64_t uisize = 0;
    uint64_t frame_n = 0;
    uint64_t seg_inf_size = 0;
    uint64_t seg_inf_pos = 0;
    uint64_t tracks_size = 0;
    uint64_t tracks_pos = 0;
    uint64_t tmp_size = 0;
    uint64_t tmp_pos = 0;
    
    int video_track = 1;
    //EBML HEADER ID
    do
    {
        next_byte = fgetc(fp);
        if(next_byte != EOF)
        {
            next_byte &= 0x000000ff;
            ID <<= 8;
            ID &= 0xffffff00;
            ID += next_byte;
        }
        else
        {
            if(ferror(fp))
                printf("An error ocurred when reading the file\n");
                
            fclose(fp);
            free(in_frame);
            return(-2);
        }
    }
    while(ID != EBML_ID_HEADER);
    tmp_size = get_size(fp);
    printf("ebml header size:%llu - skipping\n", tmp_size);
    //skip ebml header
    fseeko(fp, tmp_size, SEEK_CUR);
    
    //search for timecode scale in segment info
    //SEGMENT ID
    do
    {
        next_byte = fgetc(fp);
        if(next_byte != EOF)
        {
            next_byte &= 0x000000ff;
            ID <<= 8;
            ID &= 0xffffff00;
            ID += next_byte;
        }
        else
        {
            if(ferror(fp))
                printf("An error ocurred when reading the file\n");
                
            fclose(fp);
            free(in_frame);
            return(-2);
        }
    }
    while(ID != MATROSKA_ID_SEGMENT);
    //SEGMENT INFO ID
    do
    {
        next_byte = fgetc(fp);
        if(next_byte != EOF)
        {
            next_byte &= 0x000000ff;
            ID <<= 8;
            ID &= 0xffffff00;
            ID += next_byte;
        }
        else
        {
            if(ferror(fp))
                printf("An error ocurred when reading the file\n");
                
            fclose(fp);
            free(in_frame);
            return(-2);
        }
    }
    while(ID != MATROSKA_ID_INFO);
    seg_inf_size = get_size(fp);
    seg_inf_pos = ftello(fp);
    //TIMECODE SCALE ID
    do
    {
        next_byte = fgetc(fp);
        if(next_byte != EOF)
        {
            next_byte &= 0x000000ff;
            ID <<= 8;
            ID &= 0x00ffff00;
            ID += next_byte;
        }
        else
        {
            if(ferror(fp))
                printf("An error ocurred when reading the file\n");
                
            fclose(fp);
            free(in_frame);
            return(-2);
        }
    }
    while((ID != MATROSKA_ID_TIMECODESCALE) && (ftello(fp) < (seg_inf_pos + seg_inf_size)));
    
    //get timecode scale
    int ts_size = get_size(fp);
    for(i=0; i< ts_size; i++)
    {
        next_byte = fgetc(fp);
        if(next_byte != EOF)
        {
            next_byte &= 0x000000ff;
            timecode_scale <<= 8;
            timecode_scale &= 0xffffffffffffff00;
            timecode_scale += next_byte;
        }
        else
        {
            if(ferror(fp))
                printf("An error ocurred when reading the file\n");
                
            fclose(fp);
            free(in_frame);
            return(-2);
        }
    }
    
    printf("timecode scale is %llu\n", timecode_scale);
    
    //SEGMENT TRACKS
    do
    {
        next_byte = fgetc(fp);
        if(next_byte != EOF)
        {
            next_byte &= 0x000000ff;
            ID <<= 8;
            ID &= 0xffffff00;
            ID += next_byte;
        }
        else
        {
            if(ferror(fp))
                printf("An error ocurred when reading the file\n");
                
            fclose(fp);
            free(in_frame);
            return(-2);
        }
    }
    while(ID != MATROSKA_ID_TRACKS);
    tracks_size = get_size(fp);
    tracks_pos = ftello(fp);
    
    //printf("tracks info size: %llu - skip to pos %llu\n", tmp_size, tmp_pos+tmp_size);
    //skip segment tracks header
    //fseeko(fp, tmp_size, SEEK_CUR);
    int track_n = 1;
    int is_video = 0;
    do
    {
        next_byte = fgetc(fp);
        if(next_byte != EOF)
        {
            next_byte &= 0x000000ff;
            
            if(next_byte == MATROSKA_ID_TRACKENTRY)
            {
                //check track number and type
                //if its a video track register number and skip rest of tracks header
                tmp_size = get_size(fp);
                tmp_pos = ftello(fp);
                track_n = 0;
                int skip = 0;
                do
                {
                    next_byte = fgetc(fp);
                    if(next_byte != EOF)
                    {
                        next_byte &= 0x000000ff;
                        if(next_byte == MATROSKA_ID_TRACKNUMBER)
                        {
                            int nsize = get_size(fp);
                            track_n = get_ts(fp, nsize);
                        }
                        else if(next_byte == MATROSKA_ID_TRACKTYPE)
                        {
                            int nsize = get_size(fp);
                            if(get_ts(fp, nsize) == 1)
                                is_video = 1;
                            else
                                skip = 1; //skip track - not a video track
                        }
                        else if (next_byte == 0x73) //0x73c5 UID
                        {
                            next_byte = fgetc(fp);
                            if(next_byte != EOF)
                            {
                                next_byte &=0x000000ff;
                                if(next_byte == 0xc5) //track uid
                                {
                                    int uid_size = get_size(fp);
                                    fseeko(fp, uid_size, SEEK_CUR); //skip uid
                                }
                            }
                            else
                            {
                                if(ferror(fp))
                                    printf("An error ocurred when reading the file\n");
                
                                fclose(fp);
                                free(in_frame);
                                return(-2);
                            }
                        }
                        
                    }
                    else
                    {
                        if(ferror(fp))
                            printf("An error ocurred when reading the file\n");
                
                        fclose(fp);
                        free(in_frame);
                        return(-2);
                    }
                    
                    if(is_video && track_n)
                    {
                        video_track = track_n;
                        printf("video track at index %d\n", video_track);
                        break;
                    }
                }
                while((ftello(fp) < (tmp_pos + tmp_size)) && !skip);
                
                fseeko(fp, tmp_pos + tmp_size, SEEK_SET);
            }
        }
        else
        {
            if(ferror(fp))
                printf("An error ocurred when reading the file\n");
                
            fclose(fp);
            free(in_frame);
            return(-2);
        }
    }
    while((ftello(fp) < (tracks_pos + tracks_size)) && !is_video);
    
    //search for clusters
    while(1)
    {
        do 
        {
            next_byte = fgetc(fp);
            if(next_byte != EOF)
            {
                next_byte &= 0x000000ff;
                //if(ftello(fp) == 5553)
                //    printf("ID is 0x%08X next is 0x%08X\n", ID, next_byte);
                ID <<= 8;
                ID &= 0xffffff00;
                ID += next_byte;
            }
            else
            {
                if(ferror(fp))
                    printf("An error ocurred when reading the file\n");
                
                fclose(fp);
                free(in_frame);
                return(-2);
            }
        }
        while(ID != MATROSKA_ID_CLUSTER);
        
        cluster_file_pos = ftello(fp);
        printf("cluster found at position 0x%016X\n", cluster_file_pos-4);
        //get size
        end_cluster_file_pos = get_size(fp);
        end_cluster_file_pos += cluster_file_pos;
        
        next_byte = fgetc(fp);
        if(next_byte != EOF)
        {
            next_byte &= 0x000000ff;
        }
        else
        {
            if(ferror(fp))
                printf("An error ocurred when reading the file\n");
                
            fclose(fp);
            free(in_frame);
            return(-2);
        }
        
        if(next_byte == MATROSKA_ID_CLUSTERTIMECODE)
        {
            //printf("cluster time code found at position 0x%016X\n", ftello(fp));
            uisize = get_size(fp);
            cluster_ts = get_ts(fp, uisize);
            //printf("cluster timestamp is %llu ms\n", cluster_ts);
        }
        else
        {
            printf("found 0x%02X instead of 0xE7 - We must have arrived to the Seek Head block - finish\n", next_byte);
            printf("extracted %llu frames\n", frame_n);
            fclose(fp);
            free(in_frame);
            return (1);
        }
        
        uint64_t bgsize = 0;
        uint64_t bsize = 0;
        uint64_t fstart = 0;
        uint64_t fend = 0;
        uint64_t f_ts = 0;
        uint64_t block_start_pos = 0;
        //get frame time stamp and frame data
        do 
        {
            
            next_byte = fgetc(fp);
            if(next_byte != EOF)
            {
                next_byte &= 0x000000ff;
                if(next_byte == MATROSKA_ID_BLOCKGROUP)
                {
                    //printf("found block group at %llu \n", ftello(fp));
                    bgsize = get_size(fp);
                    next_byte = fgetc(fp);
                    if(next_byte != EOF)
                    {
                        next_byte &= 0x000000ff;
                        if(next_byte == MATROSKA_ID_BLOCK)
                        {
                            block_start_pos = ftello(fp);
                            //printf("found block at %llu \n", block_start_pos);
                            bsize = get_size(fp); //only 1 uint8_t (1-video 2-audio)
                            //video or audio ?
                            if(get_size(fp) == video_track)
                            {
                                printf("found video frame\n");
                                //timestamp - allways 2 bytes
                                f_ts = cluster_ts;
                                f_ts += get_frame_delta(fp);
                                //empty byte before frame
                                next_byte = fgetc(fp);
                                if(next_byte == EOF)
                                {
                                    if(ferror(fp))
                                        printf("An error ocurred when reading the file\n");
                
                                    fclose(fp);
                                    free(in_frame);
                                    return(-2);
                                }
                                if(next_byte != 0)
                                    printf("frame flags set to 0x%02X\n", next_byte);
                                
                                //frame
                                if( (bsize-4) > 0)
                                {
                                    fread(in_frame, 1, bsize-4, fp);
                                    char image_file[80];
                                    sprintf( image_file, "frame_%016llu_ts-%016llu.jpg", frame_n, f_ts );
                                    frame_n++;
                                    printf("saving %s\n", image_file);
                                    saveJPG(image_file, bsize-4, in_frame);
                                }
                                else
                                    printf("frame is empty - skip it\n");
                            }
                            else
                            {
                                printf("found non video frame - skip it\n");
                                fseeko(fp, bsize-1, SEEK_CUR);
                            }
                        }
                    }
                    else
                    {
                        if(ferror(fp))
                            printf("An error ocurred when reading the file\n");
                
                        fclose(fp);
                        free(in_frame);
                        return(-2);
                    }
                    
                }
                else
                if(next_byte == MATROSKA_ID_SIMPLEBLOCK)
                {
                    block_start_pos = ftello(fp);
                    printf("found simple block at %llu\n", block_start_pos);
                    bsize = get_size(fp);
                    //video or audio
                    if(get_size(fp) == video_track)
                    {
                        printf("found video frame\n");
                        //timestamp - allways 2 bytes
                        f_ts = cluster_ts;
                        f_ts += get_frame_delta(fp);
                        //empty byte before frame
                        next_byte = fgetc(fp);
                        if(next_byte == EOF)
                        {
                            if(ferror(fp))
                                printf("An error ocurred when reading the file\n");
                
                            fclose(fp);
                            free(in_frame);
                            return(-2);
                        }
                        if(next_byte != 0)
                            printf("frame flags set to 0x%02X \n", next_byte);
                                
                        //frame
                        if( (bsize-4) > 0)
                        {
                            fread(in_frame, 1, bsize-4, fp);
                            char image_file[80];
                            sprintf( image_file, "frame_%016llu_ts-%016llu.jpg", frame_n, f_ts );
                            frame_n++;
                            printf("saving %s\n", image_file);
                            saveJPG(image_file, bsize-4, in_frame);
                        }
                         else
                            printf("frame is empty - skip it\n");
                    }
                    else
                        printf("not a video frame - skip it\n");
                }
            }
            else
            {
                if(ferror(fp))
                    printf("An error ocurred when reading the file\n");
                
                fclose(fp);
                free(in_frame);
                return(-2);
            }
        }
        while( ftello(fp) < end_cluster_file_pos);
    }
    printf("extracted %llu frames\n", frame_n);
    fclose(fp);
    free(in_frame);
    return 0;
}

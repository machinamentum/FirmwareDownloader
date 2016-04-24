/**
Copyright 2013 3DSGuy

This file is part of make_cdn_cia.

make_cdn_cia is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

make_cdn_cia is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with make_cdn_cia.  If not, see <http://www.gnu.org/licenses/>.
**/
#include "lib.h"
#include "utils.h"

//MISC
void char_to_int_array(unsigned char destination[], char source[], int size, int endianness, int base)
{	
	char tmp[size][2];
    unsigned char *byte_array = malloc(size*sizeof(unsigned char));
	memset(byte_array, 0, size);
	memset(destination, 0, size);
	memset(tmp, 0, size*2);
    
    for (int i = 0; i < size; i ++){
		tmp[i][0] = source[(i*2)];
        tmp[i][1] = source[((i*2)+1)];
		tmp[i][2] = '\0';
        byte_array[i] = (unsigned char)strtol(tmp[i], NULL, base);
    }
	endian_memcpy(destination,byte_array,size,endianness);
	/**
	for (int i = 0; i < size; i++){
        switch (endianness){
        	case(BIG_ENDIAN):
        	destination[i] = byte_array[i];
        	break;
        	case(LITTLE_ENDIAN):
        	destination[i] = byte_array[((size-1)-i)];
        	break;
        }
    }
	**/
	free(byte_array);
}

void endian_memcpy(u8 *destination, u8 *source, u32 size, int endianness)
{ 
    for (u32 i = 0; i < size; i++){
        switch (endianness){
            case(BIG_ENDIAN):
                destination[i] = source[i];
                break;
            case(LITTLE_ENDIAN):
                destination[i] = source[((size-1)-i)];
                break;
        }
    }
}

void u8_hex_print_be(u8 *array, int len)
{
	for(int i = 0; i < len; i++)
		printf("%02x",array[i]);
}

void u8_hex_print_le(u8 *array, int len)
{
	for(int i = 0; i < len; i++)
		printf("%02x",array[len - i - 1]);
}

u32 align_value(u32 value, u32 alignment)
{
	u32 tmp = value;
	while(tmp > alignment)
		tmp -= alignment;
	return (value + (alignment - tmp));
}

void resolve_flag(unsigned char flag, unsigned char *flag_bool)
{
	unsigned char bit_mask[8] = {0x80,0x40,0x20,0x10,0x8,0x4,0x2,0x1};
	for(int i = 0; i < 8; i++){
		if (flag >= bit_mask[i]){
			flag_bool[7-i] = True;
			flag -= bit_mask[i];
		}
		else
			flag_bool[7-i] = False;
	}
}

void resolve_flag_u16(u16 flag, unsigned char *flag_bool)
{
	u16 bit_mask[16] = {0x8000,0x4000,0x2000,0x1000,0x800,0x400,0x200,0x100,0x80,0x40,0x20,0x10,0x8,0x4,0x2,0x1};
	for(int i = 0; i < 16; i++){
		if (flag >= bit_mask[i]){
			flag_bool[15-i] = True;
			flag -= bit_mask[i];
		}
	else
		flag_bool[15-i] = False;
	}
}

//IO Related
void PrintProgress(u32 nSize, u32 nCurrent)
{
	// Don't attempt to calculate anything if we don't have a final size
	if (nSize == 0) return;
	
	// Calculate percent and bar width
	double fPercent = ((double)nCurrent / nSize) * 100.0;
	u16 barDrawWidth = (fPercent / 100) * 40;

	int i = 0;
	printf("% 3.2f%% ", fPercent);
	for (i = 0; i < barDrawWidth; i++)
	{
		printf("|");
	}
	printf("\r");

	// Make sure the screen updates
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}

void WriteBuffer(void *buffer, u64 size, u64 offset, FILE *output)
{
	fseek_64(output,offset,SEEK_SET);
	fwrite(buffer,size,1,output);
} 

void write_align_padding(FILE *output, size_t alignment)
{
	long int pos = ftell(output);
	long int usedbytes = pos & (alignment - 1);
	if (usedbytes)
	{
		long int padbytes = (alignment - usedbytes);
		char* pad = (char*)malloc(padbytes);
		memset(pad, 0, padbytes);
		fwrite(pad, padbytes, 1, output);
		free(pad);
	}
}

u64 GetFileSize_u64(char *filename)
{
	u64 size;
#ifdef _WIN32
	int fh;
 	u64 n;
  	fh = _open( filename, 0 );
  	n = _lseeki64(fh, 0, SEEK_END);
	_close(fh);
	size = (n / sizeof(short))*2;
#else
	FILE *file = fopen(filename,"rb");
	fseeko(file, 0L, SEEK_END);
	size = ftello(file);
	fclose(file);
#endif
	return size;
}

int TruncateFile_u64(char *filename, u64 filelen)
{
#ifdef _WIN32
	HANDLE fh;
 
	LARGE_INTEGER fp;
	fp.QuadPart = filelen;
 
	fh = CreateFile(filename, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (fh == INVALID_HANDLE_VALUE) {
		printf("[!] Invalid File handle\n");
		return 1;
	}
 
	if (SetFilePointerEx(fh, fp, NULL, FILE_BEGIN) == 0 ||
	    SetEndOfFile(fh) == 0) {
		printf("[!] truncate failed\n");
		CloseHandle(fh);
		return 1;
	}
 
	CloseHandle(fh);
	return 0;
#else
	return truncate(filename,filelen);
#endif	
}

int fseek_64(FILE *fp, u64 file_pos, int whence)
{
#ifdef _WIN32
	fpos_t pos = file_pos;
	return fsetpos(fp,&pos); //I can't believe the 2gb problem with Windows & MINGW, maybe I have a bad installation :/
#else
	return fseeko(fp,file_pos,whence);
#endif
}

int makedir(const char* dir)
{
#ifdef _WIN32
	return _mkdir(dir);
#else
	return mkdir(dir, 0777);
#endif
}

char *getcwdir(char *buffer,int maxlen)
{
#ifdef _WIN32
return _getcwd(buffer,maxlen);
#else
return getcwd(buffer,maxlen);
#endif
}

void DownloadFile_InternalSave(void* out, unsigned char* buffer, u32 readSize)
{
	FILE* os = (FILE*)out;
	fwrite(buffer, readSize, 1, os);
}

static u32 install_offset = 0;
void DownloadFile_InternalInstall(void* out, unsigned char* buffer, u32 readSize)
{
	u32 bytesWritten;
	Handle* handle = (Handle*)out;

	FSFILE_Write(*handle, &bytesWritten, install_offset, buffer, readSize, 0);

	install_offset += bytesWritten;
}

Result DownloadFile_Internal(const char *url, void *out, bool bProgress,
							 void (*write)(void* out, unsigned char* buffer, u32 readSize))
{
    httpcContext context;
    u32 fileSize = 0;
    u32 procSize = 0;
    Result ret = 0;
    Result dlret = HTTPC_RESULTCODE_DOWNLOADPENDING;
    u32 status;
    u32 bufSize = 0x100000;
    u32 readSize = 0;
    httpcOpenContext(&context, HTTPC_METHOD_GET, (char*)url, 1);

    ret = httpcBeginRequest(&context);
    if (ret != 0) goto _out;

    ret = httpcGetResponseStatusCode(&context, &status, 0);
    if (ret != 0) goto _out;

    if (status != 200)
    {
        ret = status;
        goto _out;
    }

    ret = httpcGetDownloadSizeState(&context, NULL, &fileSize);
    if (ret != 0) goto _out;

    {
        unsigned char *buffer = (unsigned char *)linearAlloc(bufSize);
        if (buffer == NULL)
        {
            printf("Error allocating download buffer\n");
            ret = -1;
            goto _out;
        }

        while (dlret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING)
        {
            memset(buffer, 0, bufSize);

            dlret = httpcDownloadData(&context, buffer, bufSize, &readSize);
            write(out, buffer, readSize);

            procSize += readSize;
            if (bProgress)
            {
            	PrintProgress(fileSize, procSize);
            }
        }
        printf("\n");
        linearFree(buffer);
    }
_out:
    httpcCloseContext(&context);

    return ret;
}


Result DownloadFile(const char *url, FILE *os, bool bProgress)
{
	return DownloadFile_Internal(url, os, bProgress, DownloadFile_InternalSave);
}


Result DownloadFileInstall(const char *url, Handle *handle, u32* offset)
{
	install_offset = *offset;
	Result res = DownloadFile_Internal(url, handle, true, DownloadFile_InternalInstall);
	*offset = install_offset;
	return res;
}

//Data Size conversion
u16 u8_to_u16(u8 *value, u8 endianness)
{
	u16 new_value;
	switch(endianness){
		case(BIG_ENDIAN): new_value =  (value[1]<<0) | (value[0]<<8); break;
		case(LITTLE_ENDIAN): new_value = (value[0]<<0) | (value[1]<<8); break;
	}
	return new_value;
}

u32 u8_to_u32(u8 *value, u8 endianness)
{
	u32 new_value;
	switch(endianness){
		case(BIG_ENDIAN): new_value = (value[3]<<0) | (value[2]<<8) | (value[1]<<16) | (value[0]<<24); break;
		case(LITTLE_ENDIAN): new_value = (value[0]<<0) | (value[1]<<8) | (value[2]<<16) | (value[3]<<24); break;
	}
	return new_value;
}


u64 u8_to_u64(u8 *value, u8 endianness)
{
	u64 u64_return = 0;
	switch(endianness){
		case(BIG_ENDIAN): 
			u64_return |= (u64)value[7]<<0;
			u64_return |= (u64)value[6]<<8;
			u64_return |= (u64)value[5]<<16;
			u64_return |= (u64)value[4]<<24;
			u64_return |= (u64)value[3]<<32;
			u64_return |= (u64)value[2]<<40;
			u64_return |= (u64)value[1]<<48;
			u64_return |= (u64)value[0]<<56;
			break;
			//return (value[7]<<0) | (value[6]<<8) | (value[5]<<16) | (value[4]<<24) | (value[3]<<32) | (value[2]<<40) | (value[1]<<48) | (value[0]<<56);
		case(LITTLE_ENDIAN): 
			u64_return |= (u64)value[0]<<0;
			u64_return |= (u64)value[1]<<8;
			u64_return |= (u64)value[2]<<16;
			u64_return |= (u64)value[3]<<24;
			u64_return |= (u64)value[4]<<32;
			u64_return |= (u64)value[5]<<40;
			u64_return |= (u64)value[6]<<48;
			u64_return |= (u64)value[7]<<56;
			break;
			//return (value[0]<<0) | (value[1]<<8) | (value[2]<<16) | (value[3]<<24) | (value[4]<<32) | (value[5]<<40) | (value[6]<<48) | (value[7]<<56);
	}
	return u64_return;
}

int u16_to_u8(u8 *out_value, u16 in_value, u8 endianness)
{
	switch(endianness){
		case(BIG_ENDIAN):
			out_value[0]=(in_value >> 8);
			out_value[1]=(in_value >> 0);
			break;
		case(LITTLE_ENDIAN):
			out_value[0]=(in_value >> 0);
			out_value[1]=(in_value >> 8);
			break;
	}
	return 0;
}

int u32_to_u8(u8 *out_value, u32 in_value, u8 endianness)
{
	switch(endianness){
		case(BIG_ENDIAN):
			out_value[0]=(in_value >> 24);
			out_value[1]=(in_value >> 16);
			out_value[2]=(in_value >> 8);
			out_value[3]=(in_value >> 0);
			break;
		case(LITTLE_ENDIAN):
			out_value[0]=(in_value >> 0);
			out_value[1]=(in_value >> 8);
			out_value[2]=(in_value >> 16);
			out_value[3]=(in_value >> 24);
			break;
	}
	return 0;
}

int u64_to_u8(u8 *out_value, u64 in_value, u8 endianness)
{
	switch(endianness){
		case(BIG_ENDIAN):
			out_value[0]=(in_value >> 56);
			out_value[1]=(in_value >> 48);
			out_value[2]=(in_value >> 40);
			out_value[3]=(in_value >> 32);
			out_value[4]=(in_value >> 24);
			out_value[5]=(in_value >> 16);
			out_value[6]=(in_value >> 8);
			out_value[7]=(in_value >> 0);
			break;
		case(LITTLE_ENDIAN):
			out_value[0]=(in_value >> 0);
			out_value[1]=(in_value >> 8);
			out_value[2]=(in_value >> 16);
			out_value[3]=(in_value >> 24);
			out_value[4]=(in_value >> 32);
			out_value[5]=(in_value >> 40);
			out_value[6]=(in_value >> 48);
			out_value[7]=(in_value >> 56);
			break;
	}
	return 0;
}

//Copied from ctrtool
void memdump(FILE* fout, const char* prefix, const u8* data, u32 size)
{
	u32 i;
	u32 prefixlen = strlen(prefix);
	u32 offs = 0;
	u32 line = 0;
	while(size)
	{
		u32 max = 32;

		if (max > size)
			max = size;

		if (line==0)
			fprintf(fout, "%s", prefix);
		else
			fprintf(fout, "%*s", prefixlen, "");


		for(i=0; i<max; i++)
			fprintf(fout, "%02X", data[offs+i]);
		fprintf(fout, "\n");
		line++;
		size -= max;
		offs += max;
	}
}



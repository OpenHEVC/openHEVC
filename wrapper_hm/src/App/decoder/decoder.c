#include <stdio.h>
#include "wrapper/wrapper.h"

int find_start_code (unsigned char *Buf, int zeros_in_startcode)
{
    int info;
    int i;
    
    info = 1;
    for (i = 0; i < zeros_in_startcode; i++)
        if(Buf[i] != 0)
            info = 0;
    
    if(Buf[i] != 1)
        info = 0;
    return info;
}

int get_next_nal(FILE* inpf, unsigned char* Buf)
{
	int pos = 0;
	while(!feof(inpf)&&(fgetc(inpf))==0);
    
	int StartCodeFound = 0;
	int info2 = 0;
	int info3 = 0;
    
    
    
	while (!StartCodeFound)
	{
		if (feof (inpf))
		{
            //			return -1;
			return pos - 1;
		}
		Buf[pos++] = fgetc (inpf);
		if(pos>4)
            info3 = find_start_code(&Buf[pos-4], 3);
		if(info3 != 1 && pos > 3)
			info2 = find_start_code(&Buf[pos-3], 2);
		StartCodeFound = (info2 == 1 || info3 == 1);
	}
	fseek (inpf, - 3, SEEK_CUR);
	return pos - 4 + info2;
}
int dontRead=0;
unsigned char Y[2048 * 2048];
unsigned char U[2048 * 2048/4];
unsigned char V[2048 * 2048/4];
int main(){
    const char *filename;
    unsigned char* buf = calloc ( 1000000, sizeof(char));
    FILE *f;
    int gotpicture=0;
    filename = "/Users/mraulet/Dropbox/test_sequences/HEVC/9.0/BQSquare_416x240_60.bin";
    Init_SDL(80, 832, 480);
    
    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "could not open %s\n", filename);
        exit(1);
    }
    
    libDecoderInit();
    for (;;){
        int nal_len;
        nal_len = get_next_nal(f, buf);
        if (nal_len == - 1) exit(10);
        libDecoderDecode(buf, nal_len, Y, U, V, &gotpicture);
        if (gotpicture = 1)
            SDL_Display(80, 416, 240, Y, U, V);
    }
    libDecoderClose();
    return(0);
}
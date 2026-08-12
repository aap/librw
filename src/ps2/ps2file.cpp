#ifdef RW_PS2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../rwbase.h"

#include <sifdev.h>

struct FILE_PS2
{
	int used;
	int fd;
	int pos;
	int size;
};
FILE_PS2 ps2files[64];

/* file functions */

void*
ps2fopen(const char *path, const char *mode)
{
	int flags = 0;
	int fd;
	int i;
	char *r, *w, *plus;

//	printf("trying to open <%s> mode <%s>\n", path, mode);
	for(i = 0; i < nelem(ps2files); i++){
		if(!ps2files[i].used)
			goto found;
	}
	// no file pointer available
	return nil;
found:

	r = strchr(mode, 'r');
	w = strchr(mode, 'w');
	plus = strchr(mode, '+');

	if(plus)
		flags = SCE_RDWR;
	else if(r)
		flags = SCE_RDONLY;
	else if(w)
		flags = SCE_WRONLY;

	if(w)
		flags |= SCE_CREAT | SCE_TRUNC;

	fd = sceOpen(path, flags);
	if(fd < 0)
		return nil;

	ps2files[i].used = 1;
	ps2files[i].fd = fd;
	ps2files[i].pos = 0;
	if(w){
		ps2files[i].size = 0;
	}else{
		ps2files[i].size = sceLseek(fd, 0, SCE_SEEK_END);
		sceLseek(fd, 0, SCE_SEEK_SET);
	}
	return &ps2files[i];
}

int
ps2fclose(void *fp)
{
	FILE_PS2 *f = (FILE_PS2*)fp;
	if(!f->used)
		return EOF;
	sceClose(f->fd);
	f->used = 0;
	f->fd = -1;
	return 0;
}

int
ps2fseek(void *fp, long offset, int whence)
{
	FILE_PS2 *f = (FILE_PS2*)fp;
	f->pos = sceLseek(f->fd, offset, whence);
	return f->pos;
}

long
ps2ftell(void *fp)
{
	FILE_PS2 *f = (FILE_PS2*)fp;
	return f->pos;
}

size_t
ps2fread(void *ptr, size_t size, size_t nmemb, void *fp)
{
	FILE_PS2 *f = (FILE_PS2*)fp;
	int n = sceRead(f->fd, ptr, size*nmemb);
	f->pos += n;
	return n/size;
}

size_t
ps2fwrite(const void *ptr, size_t size, size_t nmemb, void *fp)
{
	FILE_PS2 *f = (FILE_PS2*)fp;
	int n = sceWrite(f->fd, ptr, size*nmemb);
	f->pos += n;
	if(f->pos > f->size)
		f->size = f->pos;
	return n/size;
}

int
ps2feof(void *fp)
{
	FILE_PS2 *f = (FILE_PS2*)fp;
	return f->pos >= f->size;
}

#endif

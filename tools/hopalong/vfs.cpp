#include <rw.h>
#include <string.h>

#include "vfs.h"

struct FILE_VFS
{
	int used;
	rw::uint8 *data;
	int pos;
	int size;
};
FILE_VFS vfsfiles[64];
VFS *globalVFS;

void*
vfs_fopen(const char *path, const char *mode)
{
	int flags = 0;
	int i, j;
	char *r, *w, *plus;

//	printf("trying to open <%s> mode <%s>\n", path, mode);
	const char *file = strrchr((char*)path, '/');
	if(file)
		file++;
	else
		file = path;
	for(i = 0; i < nelem(vfsfiles); i++){
		if(!vfsfiles[i].used)
			goto found;
	}
	// no file pointer available
	return nil;
found:

	// why can't we pass const char*? urghhh....
	r = strchr((char*)mode, 'r');
	w = strchr((char*)mode, 'w');
	plus = strchr((char*)mode, '+');
	// cannot write files
	if(w || plus)
		return nil;

	for(j = 0; j < globalVFS->numFiles; j++){
		if(strcmp(globalVFS->files[j].name, file) == 0)
			goto found2;
	}
	// file not found
	return nil;
found2:

	vfsfiles[i].used = 1;
	vfsfiles[i].data = globalVFS->files[j].data;
	vfsfiles[i].pos = 0;
	vfsfiles[i].size = globalVFS->files[j].length;
	return &vfsfiles[i];
}

int
vfs_fclose(void *fp)
{
	FILE_VFS *f = (FILE_VFS*)fp;
	if(!f->used)
		return EOF;
	f->used = 0;
	f->data = nil;
	return 0;
}

int
vfs_fseek(void *fp, long offset, int whence)
{
	FILE_VFS *f = (FILE_VFS*)fp;
	switch(whence){
	case 0:
		f->pos = offset;
		break;
	case 1:
		f->pos += offset;
		break;
	case 2:
		f->pos = f->size - offset;
		break;
	}
	if(f->pos > f->size) f->pos = f->size;
	return f->pos;
}

long
vfs_ftell(void *fp)
{
	FILE_VFS *f = (FILE_VFS*)fp;
	return f->pos;
}

size_t
vfs_fread(void *ptr, size_t size, size_t nmemb, void *fp)
{
	FILE_VFS *f = (FILE_VFS*)fp;
	size_t sz = size*nmemb;
	if(sz > f->size-f->pos)
		sz = f->size-f->pos;
	memcpy(ptr, f->data+f->pos, sz);
	f->pos += sz;
	return sz/size;
}

size_t
vfs_fwrite(const void *ptr, size_t size, size_t nmemb, void *fp)
{
	return 0;
}

int
vfs_feof(void *fp)
{
	FILE_VFS *f = (FILE_VFS*)fp;
	return f->pos >= f->size;
}

void
installVFS(VFS *vfs)
{
	globalVFS = vfs;
        rw::engine->filefuncs.rwfopen = vfs_fopen;
        rw::engine->filefuncs.rwfclose = vfs_fclose;
        rw::engine->filefuncs.rwfseek = vfs_fseek;
        rw::engine->filefuncs.rwftell = vfs_ftell;
        rw::engine->filefuncs.rwfread = vfs_fread;
        rw::engine->filefuncs.rwfwrite = vfs_fwrite;
        rw::engine->filefuncs.rwfeof = vfs_feof;
}

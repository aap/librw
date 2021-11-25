struct VFS_file
{
	const char *name;
	rw::uint8 *data;
	rw::uint32 length;
};

struct VFS
{
	// TODO: directories?
	VFS_file *files;
	int numFiles;
};

void installVFS(VFS *vfs);

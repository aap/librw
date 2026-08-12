void *ps2fopen(const char *path, const char *mode);
int ps2fclose(void *fp);
int ps2fseek(void *fp, long offset, int whence);
long ps2ftell(void *fp); 
size_t ps2fread(void *ptr, size_t size, size_t nmemb, void *fp);
size_t ps2fwrite(const void *ptr, size_t size, size_t nmemb, void *fp);
int ps2feof(void *fp);

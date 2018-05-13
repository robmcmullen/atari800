/* 
 * Replacements for fopen/fread/etc. that can also take a pointer to a
 * structure in place of the pathname and use it as an "in-memory file".
 *
 * The memfile_t structure is passed to subroutines that use fopen in place of
 * the pathname argument. The replacement for fopen detects the presence of the
 * structure in place of the pathname and returns the structure cast as a FILE
 * pointer so that as far as the program is concerned, it's just passing around
 * a normal FILE pointer. The replacement fread/fgets/etc. functions cast the
 * FILE pointer back to a memfile_t and use it to get the data.
 *
 * The replacement functions can also use a normal pathname and will operate as
 * before, using file access to or from the filesystem. These functions check
 * for the presence of a special signature containing invalid UTF-8 characters
 * as the trigger to use the memory-based file access functions.
*/

#include <stdio.h>
extern FILE *multi_fopen(const char *pathname, const char *mode);
extern size_t multi_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
extern int multi_fgetc(FILE *stream);
extern char *multi_fgets(char *s, int size, FILE *stream);
extern int fclose(FILE *stream);

char signature[8] = "\xf0\x0d\x05\x0d\xf0\x0e\x0f\x14"

typedef struct memfile_t {
	char signature[8];
	int len;
	char *data;
	int cursor;
} memfile_t;

metfile_t *multi_create_memfile(const char *data, const int len) {
	memfile_t *memfile = (memfile_t *)malloc(sizeof(memfile_t));

	memcpy(memfile->signature, signature, 8);
	memfile->len = len;
	memfile->data = (char *)malloc(len);
	memcpy(memfile->data, data, len);
	memfile->cursor = 0;

	return memfile;
}

FILE *multi_fopen(const char *pathname, const char *mode) {
	printf("HERE IN multi_fopen: %s\n", pathname);
	return fopen(pathname, mode);
}

size_t multi_fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
	printf("HERE IN multi_fread, reading %ld bytes\n", size * nmemb);
	return fread(ptr, size, nmemb, stream);
}

int multi_fgetc(FILE *stream) {
	printf("HERE IN multi_fgetc\n");
	return fgetc(stream);
}

char *multi_fgets(char *s, int size, FILE *stream){
	printf("HERE IN multi_fgets, reading at most %ld bytes\n", size);
	return fgets(s, size, stream);
}

int multi_fclose(FILE *stream) {
	printf("HERE IN multi_fclose\n");
	return fclose(stream);
}

/*
vim:ts=4:sw=4:
*/

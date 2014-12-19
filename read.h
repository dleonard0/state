#ifndef read_h
#define read_h

struct str;

/* Interface to read a state rules file. */
struct reader {
	/** Opens the file named by @a path; @returns an rctxt */
	void * (*open)(void *fctxt, const struct str *path);
	/** Reads data from an opened file. */
	int    (*read)(void *rctxt, char *dst, unsigned len);
	/** Closes the opened file */
	void   (*close)(void *rctxt);
};

extern const struct reader stdio_reader;

#endif /* read_h */

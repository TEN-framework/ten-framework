#include <libwebsockets.h>

/* if you need > 2GB trie files */
//typedef off_t jg2_file_offset;
typedef uint32_t jg2_file_offset;

struct lws_fts_file {
	int fd;
	jg2_file_offset root, flen, filepath_table;
	int max_direct_hits;
	int max_completion_hits;
	int filepaths;
};



#define TRIE_FILE_HDR_SIZE 20
#define MAX_VLI 5

#define LWS_FTS_LINES_PER_CHUNK 200

int
rq32(unsigned char *b, uint32_t *d);

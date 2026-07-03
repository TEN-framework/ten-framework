typedef int (*mp3_done_cb)(void *opaque);

int
play_mp3(mpg123_handle *mh, mp3_done_cb cb, void *opaque);


int
spool_capture(uint8_t *buf, size_t len);

#ifndef __AIDEV_H__
#define __AIDEV_H__

void* aidev_init (void);
void  aidev_free (void *ctxt);
void  aidev_start(void *ctxt, int start);

// when buf is NULL, size is used for command
enum {
    AIDEV_CMD_STOP = 0,
    AIDEV_CMD_RESET_BUFFER,
};
int aidev_read(void *ctxt, void *buf, int size, int wait);

#endif

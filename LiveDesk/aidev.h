#ifndef __AIDEV_H__
#define __AIDEV_H__

void* aidev_init(void);
void  aidev_free(void *ctxt);

enum {
    AIDEV_CMD_READ = 0,
    AIDEV_CMD_START,
    AIDEV_CMD_STOP,
    AIDEV_CMD_RESET_BUFFER,
};
int aidev_ctrl(void *ctxt, int cmd, void *buf, int size);

#endif

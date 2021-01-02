#ifndef   _CHANNEL_H
#define  _CHANNEL_H

struct channel_pool *alloc_channel_pool();
void free_channel_pool(struct channel_pool *channel_pool);

#endif

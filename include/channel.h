#ifndef   _CHANNEL_H
#define  _CHANNEL_H

#define CHANNEL_ID_HASH_SIZE 64
#define CHANNEL_NAME_HASH_SIZE 64
#define CHANNEL_NAME_SIZE   64

struct channel_pool {
    int64_t source_id;
    struct hlist_head id_hash[CHANNEL_ID_HASH_SIZE];
    struct hlist_head name_hash[CHANNEL_NAME_HASH_SIZE];
    void (*init)(struct channel_pool *channel_pool);
    void (*destruct)(struct channel_pool *channel_pool);
    int64_t (*open)(struct channel_pool *channel_pool, char *name, int msgsize, int maxmsg);
    void (*close)(struct channel_pool *channel_pool, int64_t channel_id);
    void (*unlink)(struct channel_pool *channel_pool, char *name);
    ssize_t (*receive)(struct channel_pool *channel_pool, int64_t channel_id, char *msg_ptr, size_t msg_len);
    ssize_t (*send)(struct channel_pool *channel_pool, int64_t channel_id, const char *msg_ptr, size_t msg_len);
    int (*isempty)(struct channel_pool *channel_pool, int64_t channel_id);
    int (*isfull)(struct channel_pool *channel_pool, int64_t channel_id);
    int (*getname)(struct channel_pool *channel_pool, int64_t channel_id, char *buf, size_t buf_len);
    int (*getmsgsize)(struct channel_pool *channel_pool, int64_t channel_id);
};

struct channel_pool *alloc_channel_pool();
void free_channel_pool(struct channel_pool *channel_pool);

#endif

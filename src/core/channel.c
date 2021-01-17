#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "hlist.h"
#include "list.h"
#include "channel.h"
#include "extend_errno.h"

struct channel_pool *alloc_channel_pool();
void free_channel_pool(struct channel_pool *channel_pool);
static inline uint32_t cal_name_hash(char *name, int len);
static void channel_pool_init(struct channel_pool *channel_pool);
static void channel_pool_destruct(struct channel_pool *channel_pool);
static int64_t channel_pool_open(struct channel_pool *channel_pool, char *name, int msgsize, int maxmsg);
static void channel_pool_close(struct channel_pool *channel_pool, int64_t channel_id);
static void channel_pool_unlink(struct channel_pool *channel_pool, char *name);
static ssize_t channel_pool_receive(struct channel_pool *channel_pool, int64_t channel_id, char *msg_ptr, size_t msg_len);
static ssize_t channel_pool_send(struct channel_pool *channel_pool, int64_t channel_id, const char *msg_ptr, size_t msg_len);
static int channel_pool_isempty(struct channel_pool *channel_pool, int64_t channel_id);
static int channel_pool_isfull(struct channel_pool *channel_pool, int64_t channel_id);
static int channel_pool_getname(struct channel_pool *channel_pool, int64_t channel_id, char *buf, size_t buf_len);
static int channel_pool_getmsgsize(struct channel_pool *channel_pool, int64_t channel_id);

struct channel {
    struct hlist_node name_node;
    char name[CHANNEL_NAME_SIZE+1];
    struct list_head list_head;
    int unlinked;
    int refcnt;
    int msgsize;
    int maxmsg;
    int curmsgs;
};

struct channel_data {
    struct list_head data_node;
    void *mem_base;
    int mem_len;
    void *data_base;
    int data_len;
};

struct id_name_node {
    struct hlist_node id_node;
    int64_t id;
    struct channel *channel;
};

static inline uint32_t cal_name_hash(char *name, int len){
    uint32_t ret = 0;
    int i;
    for(i = 0; i < len; i++){
       ret += (unsigned char)name[i];
    }
    return ret & (CHANNEL_NAME_HASH_SIZE - 1);
}

struct channel_pool *alloc_channel_pool(){
    struct channel_pool *channel_pool = calloc(1, sizeof(struct channel_pool));
    if(!channel_pool){
        return NULL;
    }
    channel_pool->open = channel_pool_open;
    channel_pool->close = channel_pool_close;
    channel_pool->unlink = channel_pool_unlink;
    channel_pool->init = channel_pool_init;
    channel_pool->destruct = channel_pool_destruct;
    channel_pool->send = channel_pool_send;
    channel_pool->receive = channel_pool_receive;
    channel_pool->isempty = channel_pool_isempty;
    channel_pool->isfull = channel_pool_isfull;
    channel_pool->getname = channel_pool_getname;
    channel_pool->getmsgsize = channel_pool_getmsgsize;
    channel_pool->init(channel_pool);
    return channel_pool;
}

void free_channel_pool(struct channel_pool *channel_pool){
    channel_pool->destruct(channel_pool);
    free(channel_pool);
}

static void channel_pool_init(struct channel_pool *channel_pool){
    int i;
    channel_pool->source_id = 1;
    for(i = 0; i < CHANNEL_ID_HASH_SIZE; i++){
        INIT_HLIST_HEAD(&channel_pool->id_hash[i]);
    }
    for(i = 0; i < CHANNEL_NAME_HASH_SIZE; i++){
        INIT_HLIST_HEAD(&channel_pool->name_hash[i]);
    }
}

static void channel_pool_destruct(struct channel_pool *channel_pool){
    int i;
    struct hlist_head *head;
    struct hlist_node *cur, *next;
    struct id_name_node *id_name_node;
    struct channel *channel;
    struct channel_data *cur_channel_data, *next_channel_data;
    for(i=0; i < CHANNEL_ID_HASH_SIZE; i++){
        head = &channel_pool->id_hash[i];
        hlist_for_each_entry_safe(id_name_node, cur, next, head, id_node){
            free(id_name_node);
        }
    }
    for(i=0; i < CHANNEL_NAME_HASH_SIZE; i++){
        head = &channel_pool->name_hash[i];
        hlist_for_each_entry_safe(channel, cur, next, head, name_node){
	    list_for_each_entry_safe(cur_channel_data, next_channel_data, &(channel->list_head), data_node) {
                free(cur_channel_data->mem_base);
            }
            free(channel);
        }
    }
}

static int64_t channel_pool_open(struct channel_pool *channel_pool, char *name, int msgsize, int maxmsg){
    struct channel *channel, *find_channel = NULL;
    struct hlist_node *cur, *next;
    if(strlen(name) > CHANNEL_NAME_SIZE){
        errno = ECHANNELNAME;
        return -1;
    }
    struct hlist_head *head = &(channel_pool->name_hash[cal_name_hash(name, strlen(name))]);
    hlist_for_each_entry_safe(channel, cur, next, head, name_node){
        if(strcmp(channel->name, name) == 0){
	    if(channel->unlinked){
	        errno = ECHANNELUNLINKED;
                return -1;
	    }
	    find_channel = channel;
	    break;
	}
    }
    if(!find_channel){
        find_channel = calloc(1, sizeof(struct channel));
        strcpy(find_channel->name, name);
        INIT_LIST_HEAD(&(find_channel->list_head));
        find_channel->msgsize = msgsize;
        find_channel->maxmsg = maxmsg;
        find_channel->curmsgs = 0;
        find_channel->unlinked = 0;
        find_channel->refcnt = 0;
        hlist_add_head(&(find_channel->name_node), head);
    }
    struct id_name_node *id_name_node = calloc(1, sizeof(struct id_name_node));
    id_name_node->id = channel_pool->source_id++;
    id_name_node->channel = find_channel;
    find_channel->refcnt++;
    hlist_add_head(&(id_name_node->id_node), &(channel_pool->id_hash[id_name_node->id & (CHANNEL_ID_HASH_SIZE - 1)]));
    return id_name_node->id;
}

static void channel_pool_close(struct channel_pool *channel_pool, int64_t channel_id){
    struct hlist_head *head;
    struct hlist_node *cur, *next;
    struct id_name_node *id_name_node;
    struct channel *channel;
    struct channel_data *cur_channel_data, *next_channel_data;
    head = &channel_pool->id_hash[channel_id & (CHANNEL_ID_HASH_SIZE - 1)];
    hlist_for_each_entry_safe(id_name_node, cur, next, head, id_node){
        if(channel_id == id_name_node->id) {
	    hlist_del(&(id_name_node->id_node));
            channel = id_name_node->channel;
	    free(id_name_node);
            channel->refcnt--;
	    if(!channel->refcnt && channel->unlinked){
	        list_for_each_entry_safe(cur_channel_data, next_channel_data, &(channel->list_head), data_node) {
                    free(cur_channel_data->mem_base);
                }
		hlist_del(&(channel->name_node));
                free(channel);
	    }
	    return;
	}
    }
}

static void channel_pool_unlink(struct channel_pool *channel_pool, char *name){
    struct channel *channel, *find_channel = NULL;
    struct channel_data *cur_channel_data, *next_channel_data;
    struct hlist_node *cur, *next;
    struct hlist_head *head = &(channel_pool->name_hash[cal_name_hash(name, strlen(name))]);
    hlist_for_each_entry_safe(channel, cur, next, head, name_node){
        if(strcmp(channel->name, name) == 0){
	    if(!channel->refcnt){
	        list_for_each_entry_safe(cur_channel_data, next_channel_data, &(channel->list_head), data_node) {
                    free(cur_channel_data->mem_base);
                }
		hlist_del(&(channel->name_node));
                free(channel);
	    } else {
	        channel->unlinked = 1;
	    }
	    return;
	}
    }
}

static ssize_t channel_pool_receive(struct channel_pool *channel_pool, int64_t channel_id, char *msg_ptr, size_t msg_len){
    struct hlist_head *head;
    struct hlist_node *cur, *next;
    struct id_name_node *id_name_node;
    struct channel *channel;
    struct channel_data *channel_data;
    int data_len;
    head = &channel_pool->id_hash[channel_id & (CHANNEL_ID_HASH_SIZE - 1)];
    hlist_for_each_entry_safe(id_name_node, cur, next, head, id_node){
        if(channel_id == id_name_node->id) {
            channel = id_name_node->channel;
	    if(msg_len < channel->msgsize){
	        errno = EMSGSIZE;
                return -1;
	    }
	    if(!channel->curmsgs){
	        errno = EAGAIN;
                return -1;
	    }
	    channel_data = list_entry(channel->list_head.next, typeof(*channel_data), data_node);
	    data_len = channel_data->data_len;
	    memcpy(msg_ptr, channel_data->data_base, data_len);
	    list_del(&(channel_data->data_node));
            free(channel_data->mem_base);
	    channel->curmsgs--;
	    return data_len;
	}
    }
}

static ssize_t channel_pool_send(struct channel_pool *channel_pool, int64_t channel_id, const char *msg_ptr, size_t msg_len){
    struct hlist_head *head;
    struct hlist_node *cur, *next;
    struct id_name_node *id_name_node;
    struct channel *channel;
    struct channel_data *channel_data;
    int mem_len;
    head = &channel_pool->id_hash[channel_id & (CHANNEL_ID_HASH_SIZE - 1)];
    hlist_for_each_entry_safe(id_name_node, cur, next, head, id_node){
        if(channel_id == id_name_node->id) {
            channel = id_name_node->channel;
	    if(msg_len > channel->msgsize){
	        errno = EMSGSIZE;
                return -1;
	    }
	    if(channel->curmsgs >= channel->maxmsg){
	        errno = EAGAIN;
                return -1;
	    }
	    mem_len = sizeof(struct channel_data) + msg_len;
	    channel_data = calloc(1, mem_len);
	    channel_data->mem_base = channel_data;
	    channel_data->mem_len = mem_len;
	    channel_data->data_base = (char *)(channel_data->mem_base) + sizeof(struct channel_data);
	    channel_data->data_len = msg_len;
	    memcpy(channel_data->data_base, msg_ptr, msg_len);
	    list_add_before(&(channel_data->data_node), &(channel->list_head));
	    channel->curmsgs++;
	    return msg_len;
	}
    }
}

static int channel_pool_isempty(struct channel_pool *channel_pool, int64_t channel_id){
    struct hlist_head *head;
    struct hlist_node *cur, *next;
    struct id_name_node *id_name_node;
    struct channel *channel;
    head = &channel_pool->id_hash[channel_id & (CHANNEL_ID_HASH_SIZE - 1)];
    hlist_for_each_entry_safe(id_name_node, cur, next, head, id_node){
        if(channel_id == id_name_node->id) {
            channel = id_name_node->channel;
	    return !channel->curmsgs;
	}
    }
}

static int channel_pool_isfull(struct channel_pool *channel_pool, int64_t channel_id){
    struct hlist_head *head;
    struct hlist_node *cur, *next;
    struct id_name_node *id_name_node;
    struct channel *channel;
    head = &channel_pool->id_hash[channel_id & (CHANNEL_ID_HASH_SIZE - 1)];
    hlist_for_each_entry_safe(id_name_node, cur, next, head, id_node){
        if(channel_id == id_name_node->id) {
            channel = id_name_node->channel;
	    if(channel->curmsgs >= channel->maxmsg){
                return 1;
	    } else {
                return 0;
	    }
	}
    }
}

static int channel_pool_getname(struct channel_pool *channel_pool, int64_t channel_id, char *buf, size_t buf_len){
    struct hlist_head *head;
    struct hlist_node *cur, *next;
    struct id_name_node *id_name_node;
    struct channel *channel;
    head = &channel_pool->id_hash[channel_id & (CHANNEL_ID_HASH_SIZE - 1)];
    hlist_for_each_entry_safe(id_name_node, cur, next, head, id_node){
        if(channel_id == id_name_node->id) {
            channel = id_name_node->channel;
	    if(strlen(channel->name) + 1 > buf_len) {
	        errno = ECHANNELNAME;
                return -1;
	    }
	    strcpy(buf, channel->name);
	    return 0;
	}
    }
    errno = EINVAL;
    return -1;
}

static int channel_pool_getmsgsize(struct channel_pool *channel_pool, int64_t channel_id){
    struct hlist_head *head;
    struct hlist_node *cur, *next;
    struct id_name_node *id_name_node;
    struct channel *channel;
    head = &channel_pool->id_hash[channel_id & (CHANNEL_ID_HASH_SIZE - 1)];
    hlist_for_each_entry_safe(id_name_node, cur, next, head, id_node){
        if(channel_id == id_name_node->id) {
            channel = id_name_node->channel;
	    return channel->msgsize;
	}
    }
    errno = EINVAL;
    return -1;
}

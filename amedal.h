/*
 *
 * gonga@ee.kth.se 2014 :-)
 */

#ifndef __AMEDAL_H_
#define __AMEDAL_H_

///=========================================================================/
///=========================================================================/
#include "contiki.h"
#include "lib/random.h"

#include "net/rime/rimeaddr.h"

#define CLOCK_DT(a,b)     ((signed short)((a)-(b)) < 0)
///=========================================================================/
///=========================================================================/
#define MAX_SYNCH_PKTS   10
#define MAX_REPEAT   10

///=========================================================================/
///=========================================================================/
struct data_item_t{
    uint8_t node_id;
    uint8_t hopcount;
};
#define DATA_ITEM_LEN sizeof(struct data_item_t)
///=========================================================================/
///=========================================================================/
struct nodelist_item{
    struct nodelist_item *next;
    uint8_t node_id;
    uint8_t hopcount;
};
///=========================================================================/
///=========================================================================/
#if CONF_NETWORK_SIZE != 0
const static uint8_t num_neighbors = CONF_NETWORK_SIZE;
#else //CONF_NETWORK_SIZE != 0
const static uint8_t num_neighbors = 2;
#endif //CONF_NETWORK_CLIQUE_SIZE != 0
///=========================================================================/
///=========================================================================/
#if CONF_NETWORK_SIZE <= 20
#define NETWORK_PAYLOAD_SIZE  (sizeof(struct data_item_t))*CONF_NETWORK_SIZE
#else //CONF_NETWORK_SIZE <= 50
#define  NETWORK_PAYLOAD_SIZE  (sizeof(struct data_item_t))*53
#endif //CONF_NETWORK_SIZE <= 50
///=========================================================================/
///=========================================================================/
typedef struct{
    uint8_t type;
    uint8_t data[NETWORK_PAYLOAD_SIZE];
}data_packet_t;
#define DATA_PACKET_SIZE sizeof(data_packet_t)
///=========================================================================/
///=========================================================================/
typedef enum{
    SYNCHER,
    DATA_NODE
}node_type_t;
///=========================================================================/
///=========================================================================/
enum{
    SYNCH_PACKET,
    DATA_PACKET,
    PRINT_DATA
};
///=========================================================================/
///=========================================================================/
uint8_t get_curr_neighbors();
uint8_t get_elapsed_rounds();
uint16_t get_discovery_latency();
//return first hope neighbors...
uint8_t neighbors_1hop();

//called upper-layer to initialize execution..
void startup_protocol(uint8_t start_id);
///=========================================================================/
///=========================================================================/
void neighs_init();

struct nodelist_item *neighs_get(uint8_t nodeid);

void neighs_add_itself();

uint8_t neighs_num_nodes();

/**
 * @brief neighs_1hop: return the 1 hop neighbors
 * @return
 */
uint8_t neighs_1hop(void);

/**
 * @brief neighs_all_found: return TRUE if all neighbors have been found
 * @return
 */
uint8_t neighs_all_found();

uint8_t neighs_updated();

uint8_t neighs_updateReset();

/**
 * @brief neighs_remove: remove a specific neighbor
 * @param nodeid
 */
void neighs_remove(uint8_t nodeid);

/**
 * @brief neighs_remove_all
 */
void neighs_remove_all();

/**
 * @brief neighs_flush_all
 */
void neighs_flush_all();

//adds one or several elements to the list of neighbors. The epidemic mechanism
//ios activated since more than one neighbors can be discoreverd at the same
//time.
uint8_t neighs_register(uint8_t *pkt_hdr);

//adds a single element to the list of neighbors. The epidemic mechanism is
//not used when using this function
uint8_t neighs_sregister(uint8_t *pkt_hdr);

uint8_t neighs_add2payload(uint8_t *data_ptr);

uint8_t log2_n(uint16_t n);

uint16_t random_int(uint16_t size_num);

///=========================================================================/
///=========================================================================/
#if CONF_CHANNEL_POOL_SIZE != 0
#define CHANNEL_POOL_SIZE CONF_CHANNEL_POOL_SIZE
const static  uint8_t num_channels = CHANNEL_POOL_SIZE;
#else   //CONF_CHANNEL_POOL_SIZE
#define CHANNEL_POOL_SIZE    1
const static  uint8_t num_channels = CHANNEL_POOL_SIZE;
#endif //CONF_CHANNEL_POOL_SIZE
///=========================================================================/
///=========================================================================/
#if HOPCOUNT_FILTER_NDISC != 0
    #if CHANNEL_POOL_SIZE == 1
        #define MAX_HOPCOUNT  HOPCOUNT_FILTER_NDISC //1
    #else //CHANNEL_POOL_SIZE == 1
        #define MAX_HOPCOUNT HOPCOUNT_FILTER_NDISC
    #endif //CHANNEL_POOL_SIZE == 1
#else //HOPCOUNT_FILTER_NDISC != 0
    #define MAX_HOPCOUNT 15
#endif //CONF_MAX_HOPCOUNT != 0
///=========================================================================/
///=========================================================================/

#endif /* __AMEDAL_H__ */

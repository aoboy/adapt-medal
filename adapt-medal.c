/*
 * \filename: mc-hopcount_neighdisc.c
 * \description: This is a joint network and RDC layer protocol implementation
 *               of multichannel wireless neighbor discovery with additional
 *               epidemic dissemination mechanism.
 *
 * \author
 *         Antonio Gonga <gonga@kth.se>
 * \date
 *      May 20th, 2013, 01:40 AM
 *
 */

#include "contiki.h"
#include "net/rime.h"
#include "random.h"

#include "net/packetbuf.h"
#include "net/netstack.h"
#include "net/rime.h"
#include "dev/watchdog.h"

#include "sys/rtimer.h"


#include "dev/leds.h"
#include "./cc2420.h"
#include "dev/button-sensor.h"

#include "dev/serial-line.h"
#include "node-id.h"

#include "./amedal.h"

#include <string.h>

#define DEBUG 1

#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#define SIZE_NETWORK 11
/*---------------------------------------------------------------------------*/
typedef struct{
    uint8_t channel;
    uint8_t num_nodes[SIZE_NETWORK];
    uint16_t prob_vec[SIZE_NETWORK];
}prob_table_t;
/*---------------------------------------------------------------------------*/
/** -------------------------------------------------------------------------
 *     (2*k + N - 1) - sqrt((2*k + N - 1)^2 - 4*k*N)
 * p = ---------------------------------------------------
 *                     2*N
 * where, k - number of channels
 *        N - clique size..
 ----------------------------------------------------------------------------*/
static prob_table_t lookup_tbl[] = {
    {1, {2, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}, {32768,13107,6554,4369,3277,2621,2185,1872,1638,1456,1311}},
    {2, {2, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}, {32768,20323,11685,8120,6210,5025,4219,3635,3193,2847,2568}},
    {3, {2, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}, {32768,24087,15575,11289,8812,7216,6105,5289,4664,4171,3772}},
    {4, {2, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}, {32768,26214,18488,13936,11102,9201,7847,6836,6053,5431,4923}},
    {5, {2, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}, {32768,27538,20681,16136,13107,10994,9450,8279,7362,6626,6023}},
    {6, {2, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}, {32768,28430,22356,17964,14857,12607,10923,9623,8594,7760,7072}},
    {7, {2, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}, {32768,29067,23659,19488,16384,14055,12272,10872,9750,8833,8070}},
    {8, {2, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}, {32768,29544,24693,20766,17716,15356,13506,12032,10835,9848,9021}},
    {9, {2, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}, {32768,29913,25528,21845,18881,16523,14635,13107,11852,10806,9925}},
    {10, {2, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}, {32768,30207,26214,22763,19904,17571,15668,14103,12803,11711,10784}},
    {11, {2, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}, {32768,30447,26786,23551,20804,18514,16612,15026,13694,12565,11599}},
    {12, {2, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}, {32768,30645,27269,24232,21600,19364,17476,15881,14528,13370,12373}},
    {13, {2, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}, {32768,30813,27683,24825,22307,20131,18268,16674,15307,14129,13107}},
    {14, {2, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}, {32768,30956,28039,25346,22937,20826,18994,17409,16037,14844,13803}},
    {15, {2, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}, {32768,31080,28351,25806,23502,21456,19661,18091,16720,15519,14464}},
    {16, {2, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50}, {32768,31188,28624,26214,24009,22030,20274,18724,17359,16155,15090}},
};
/*---------------------------------------------------------------------------*/
typedef rtimer_clock_t my_clock_t;
static volatile rtimer_clock_t callback_time;     

static struct rtimer generic_timer;
/*---------------------------------------------------------------------------*/
static volatile uint16_t slot_increment_offset = 0;

/**
 * @brief phase_i: the variable that holds at which phase we are in
 */
static volatile uint8_t  phase_i  = 1;
static volatile uint8_t adapt_num_neighbors = 2;

static volatile uint16_t phase_timer_offset = 0;
/*---------------------------------------------------------------------------*/
static volatile uint16_t discovery_time = 0;
static volatile uint16_t mean_disc_time = 0;
static volatile uint16_t max_disc_time  = 0;
/*---------------------------------------------------------------------------*/
static uint8_t rounds_counter = 0;
static uint8_t num_rounds_counter = 0;

static my_clock_t ref_timer;

#if CONF_NETWORK_CLIQUE_SIZE != 0
static node_info_t neigh_table[CONF_NETWORK_CLIQUE_SIZE];
#else
static node_info_t neigh_table[MAX_NETWORK_SIZE];
#endif //CONF_NETWORK_CLIQUE_SIZE
/*---------------------------------------------------------------------------*/
static volatile uint8_t radio_is_on = 0;
/*---------------------------------------------------------------------------*/
//static uint8_t num_neighbors = 0;


static volatile uint8_t nd_is_started = 0;

static volatile uint8_t nxt_channel_idx  = 0;
static volatile uint8_t clear_vars_val   = 0;

static volatile uint8_t packet_received_flag = 0;

static volatile uint8_t radio_read_flag = 0;

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/* the sender node_id: helps create links whose sender_id is the node that was 
 * behaving/acting as a transmitter
 */
static volatile uint8_t sender_nodeid = 0;
static volatile uint8_t all_neighbors_found_flag = 0;
/*---------------------------------------------------------------------------*/
static uint8_t i3e154_channels [] ={11, 12, 13, 14, 15, 16, 17,
                                    18, 19, 20, 21, 22, 23, 24, 25, 26};
/*---------------------------------------------------------------------------*/
PROCESS(adapt_multich_epidemic_discovery, "adapt-medal process");
AUTOSTART_PROCESSES(&adapt_multich_epidemic_discovery);
/*---------------------------------------------------------------------------*/
/**---------------------------------------------------------------------------*/
static void on(void){
  if(radio_is_on == 0) {
    radio_is_on = 1;
    NETSTACK_RADIO.on();
  }
}
/*----------------------------------------------------------------------------*/
/**---------------------------------------------------------------------------*/
static void off(void){
  if(radio_is_on) {
    radio_is_on = 0;
    NETSTACK_RADIO.off();
  }
}
/*----------------------------------------------------------------------------*/
/**
 * @brief leds_set : display a counter into the Leds
 * @param count
 */
static void leds_set(uint8_t count){
    if(count & LEDS_GREEN){
        leds_toggle(LEDS_GREEN);
    }else{

        if(count & LEDS_YELLOW){
            leds_toggle(LEDS_YELLOW);
        }else{

            if(count & LEDS_RED){
                leds_toggle(LEDS_RED);
            }
        }
    }
}
/*---------------------------------------------------------------------------*/
/**
 * @brief nodeid_no_exist
 * @param nodeid
 * @return
 */
static uint8_t nodeid_exist(uint8_t nodeid){
    uint8_t k;
    for (k = 0; k < num_neighbors; k++){   
        if (neigh_table[k].node_id == nodeid){
            return 1;
        }
    }
 return 0;
}
/*---------------------------------------------------------------------------*/
/* add and update neighbors table upon reception of a packet from a neighbor..
 * it also performs hopcount filtering, by checking the length of each received
 * nodeid hop count..
 *----------------------------------------------------------------------------*/
/**
 * @brief add_neighbors: adds and updates the neighbors table
 * @param p
 */
static void add_neighbors(uint8_t *p){
    uint8_t j, k;
    for (k = 0; k < num_neighbors; k++){

        node_info_t *nif = (node_info_t*)(p + k*sizeof(node_info_t));
        uint8_t nodeid = nif->node_id;
        uint8_t hopcount = nif->hopcount;

        /*check if the received nodeid is not zero/null*/
        if(nodeid != 0){
            /**insert only when the nodeid is new, otherwise, do an update*/
            if(nodeid_exist(nodeid)){
                /*update current estimative..node already exists...*/
                for (j = 0; j < num_neighbors; j++){
                    node_info_t *nt = &neigh_table[j];
                    if(nt->node_id == nodeid && hopcount < nt->hopcount){
                        neigh_table[j].hopcount = hopcount;
                        break;
                    }
                }
            } //if==>nodeid_no_exist UPDATE....
            else{                
                for(j = 0; j < num_neighbors; j++){
                    if(neigh_table[j].node_id == 0 && hopcount <= MAX_HOPCOUNT){
                        neigh_table[j].node_id  = nodeid;
                        neigh_table[j].hopcount = hopcount;
                        break;
                    }
                } //end for ==>J
            }
        } //end of if==>nid != 0
    } //end of for ==>K
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/**
 * @brief get_random_channel
 * @param num_channels: the maximum number of channels in use for that case
 * @param rand_num    : is the pseudo random number to extract the channelId
 * @return            : a random index from 0 to num_channels-1. The index
 *                      gives the channel to use for current time-slot
 */
static uint8_t random_int(uint8_t size_num){
    uint16_t rand_num = random_rand();

    /** this check is important to prevent division by zero error*/
    if(size_num == 0){
        return 0;
    }

    uint16_t val = (65535 / size_num);

    if ( rand_num < val){
        return 0;
    }else{
        uint8_t k;
        for(k = 1; k < size_num; k++){
            if (rand_num >= k*val && rand_num <= (k+1)*val){
                return k;
            }
        }
    }
    return 0;
}
/*---------------------------------------------------------------------------*/
/**
 * @brief node_link_active
 * @param ploss
 * @return
 */
static uint8_t node_link_active(uint8_t ploss){
    uint16_t loss_p = (65535/100)*ploss;

    if (random_rand() > loss_p){
        return 1;
    }else{
        return 0;
    }
}
/*---------------------------------------------------------------------------*/
/**
 * @brief calc_trans_p : computes the transmission probability given
 *                       k-the number of channels, and n-the clique size..
 *                            ((2k + n - 1) - sqrt((2k + n - 1)^2 - 4kn))
 *                      p = -------------------------------------
 *                                        2n
 * @param k            : the number of channels
 * @param n            : the clique size
 * @return             : the computed transmission probability
 */
static uint16_t calc_trans_p(const uint8_t k, const uint8_t n){
    uint8_t i = 0;
    uint16_t a = 2*k + n - 1;
    uint16_t s = a*a - 4*k*n;
    uint16_t c = 2*n;

    uint16_t x1 = 0, xn = 100;

    for(i = 0; i < 5; i++){
        xn = (xn + s/xn)/2;

        if(i > 0 && x1 == xn){
            break;
        }else{
            x1 = xn;
        }
        //PRINTF("(%3u %3u)", xn, a);
    }

    uint16_t prob = (a - xn)*(65535/c);

    return  prob;
}
/*---------------------------------------------------------------------------*/
/**
 * @brief get_tx_prob  : returns an optimal transmisison probability given the
 *                       network size and the number of channels in use.
 * @param num_channels : the maximum number of channels in use
 * @param nwk_size     : the current network size.
 * @return
 */
static uint16_t get_tx_prob(const uint8_t num_channels, const uint8_t nwk_size){
    uint8_t j;

    prob_table_t *prb = &lookup_tbl[(num_channels-1)%16];
    if(prb->channel == num_channels){
        for (j = 0; j < SIZE_NETWORK; j++){
            if (prb->num_nodes[j] == nwk_size){
                return prb->prob_vec[j];
            }
        }
    }

    return calc_trans_p(num_channels, nwk_size);
}
/*---------------------------------------------------------------------------*/
/**
 * @brief all_neighbors_found: checks if all neighbors have been discovered
 * @param num_neighs         : current network size
 * @return
 */
static uint8_t all_neighbors_found(uint8_t num_neighs){
    uint8_t k;

    for(k = 0; k < num_neighs; k++){
        if (neigh_table[k].node_id == 0){
            return 0;
        }
    }

    return 1;
}
/*---------------------------------------------------------------------------*/
/**
 * @brief clear_vars
 */
static void clear_vars(){
    clear_vars_val = 0;

    nd_is_started      = 0;
    slot_increment_offset = 0;
    packet_received_flag  = 0;
    all_neighbors_found_flag = 0;

    cc2420_set_channel(26);
    on();

    memset(neigh_table, 0, sizeof(neigh_table));

    /*PRINTF(">%d,%d,%u,%u,%u-%u\n", rounds_counter, node_id,
           CHANNEL_POOL_SIZE, num_neighbors, discovery_time, mean_disc_time/rounds_counter);*/

    node_info_t *np  =  &neigh_table[0];

    np->node_id      =  node_id; //(rimeaddr_node_addr.u8[0]  & 0xff);
    np->hopcount     =  0;

    discovery_time = 0;

    //Reset adaptive parameters..
    phase_i = 1;
    adapt_num_neighbors = 2;
    phase_timer_offset = 1 + adapt_num_neighbors;
}
/*---------------------------------------------------------------------------*/
/**
 * @brief aloha_transmit: main transmit function that's invoked probabilisticly
 *                        to transmit a neighbor advert packet. Epidemic dissemination
 *                        is used here to speed up the neighbor discovery process
 *
 * @param t             : the real time timer pointer
 * @param ptr           : a pointer of data to the callback function
 * @return              : an integer without any meaningful value
 */
static char aloha_transmit(void){
    uint8_t k;

    data_packet_t  dpacket;
    memset(&dpacket, 0, sizeof(data_packet_t));

    my_clock_t t_end= RTIMER_NOW() + 2*ONE_MSEC;

    uint16_t delay = 0;
    while(RTIMER_CLOCK_LT(RTIMER_NOW(), t_end)){
        delay = delay + 1;
    }

    dpacket.ptype = DATA_PACKET;
    dpacket.node_id = node_id;

    //fill the payload...
    for (k = 0; k < num_neighbors; k++){

        node_info_t *nt = &neigh_table[k];

        if(nt->node_id != 0 && (nt->hopcount + 1 <= MAX_HOPCOUNT)){
            uint8_t dt_idx = k*sizeof(node_info_t);
            dpacket.data[dt_idx]     = nt->node_id;
            dpacket.data[dt_idx + 1] = nt->hopcount + 1;
        }
    }
    //check if we are not receiving a packet...
    if(radio_read_flag == 0){
        NETSTACK_RADIO.send(&dpacket, DICSCOVERY_PACKET_SIZE);
    }

    return 1;
}
/*---------------------------------------------------------------------------*/
/**
 * @brief auto_channel_hopping: it's a callback and a main process function
 *                              and it's triggered by the real time timer for
 *                              T-millisecs. Where T, is the size of the time
 *                              slot. A node flips a coin and based on pre-stored
 *                              values or by computing Pt it decides if it should
 *                              transmit or receive a packet.
 *                              A random channel for every time-slot is also
 *                              generated in this function.
 *                              After \tau-terminating time, the function exits
 * @param t                   : The real time timer pointer
 * @param ptr                 : a data pointer to the callback function
 * @return                    : an integer without any meaningful value yet...
 */
//phase_timer_offset = 1 + adapt_num_neighbors;
static char auto_channel_hopping(struct rtimer *t, void *ptr){   

    if(nd_is_started){

        int ret = 0;

        slot_increment_offset++;

        if (slot_increment_offset != 0 &&
                (slot_increment_offset % phase_timer_offset) == 0){
            //increment phase_i.
            phase_i = phase_i + 1;
            uint8_t shift_val = phase_i+1;
            //Compute number of neighbors for phase i.
            adapt_num_neighbors = (1 << phase_i); // N_i = 2^i
            //compute time when next phase ends.
            phase_timer_offset = phase_timer_offset + (1 << shift_val);
        }
        //generate a random prob to compare with.
        uint16_t prob = random_rand();
        //generate a random channel
        nxt_channel_idx =  random_int(num_channels);
        //compute trans probability
        uint16_t tx_prob = get_tx_prob(num_channels, adapt_num_neighbors);

        //set the channel..
        cc2420_set_channel(i3e154_channels[nxt_channel_idx]);

        if(prob < tx_prob){
            aloha_transmit();
        }else{
            //listen
        }

        callback_time   = (ref_timer + (slot_increment_offset*TS));

        ret = rtimer_set(&generic_timer, callback_time, 1,
                         (void (*)(struct rtimer *, void *))auto_channel_hopping, NULL);

        leds_toggle(LEDS_GREEN);

        if(ret){
            PRINTF("synchronization failed\n");
        }

        return 1;
    }
    else{

        if(clear_vars_val){
         clear_vars();
        }
        return 0;
    }

    return 0;
}
/*---------------------------------------------------------------------------*/
static void node_trigger(){

}
/*---------------------------------------------------------------------------*/
/**
 * @brief packet_input: this fucntion is called by the device radio driver
 *                      whenever a packet is received. Appropriate actions are
 *                      taken according to an algorithm to determine the exact
 *                      actions to perform..
 */
static void packet_input(void){

    int len = 0;

    radio_read_flag = 1;

    len = NETSTACK_RADIO.read(packetbuf_dataptr(), PACKETBUF_SIZE);
    radio_read_flag = 0;

    if(len > 0) {
        packetbuf_set_datalen(len);
    }else{
        //PRINTF("collision\n");
        return;
    }

    data_packet_t *pkt = (data_packet_t*)packetbuf_dataptr();

    if(pkt->ptype == SYNCH_PACKET){
        
        leds_set(pkt->seqno);
        
        //rounds_counter = pkt->rounds;

        //num_rounds_counter++;
        
        sender_nodeid  = pkt->node_id;

        //make sure that we know when we can start transmitting..
        if(nd_is_started == 0){

            nd_is_started = 1;

            slot_increment_offset = 0;

            all_neighbors_found_flag = 0;

            /**clear neighbors list.. */
            memset(neigh_table, 0, sizeof(neigh_table));

            node_info_t *np  =  &neigh_table[0];
            np->node_id      =  node_id; //(rimeaddr_node_addr.u8[0]  & 0xff);
            np->hopcount     =  0;
            
            uint8_t time_2_start = (MAX_SYNCH_PKTS  - pkt->seqno);

            if(time_2_start == 0){
               callback_time = RTIMER_NOW() + 2;
            }else{
                callback_time = RTIMER_NOW() + time_2_start*TS;
            }

            ref_timer = callback_time;

            uint8_t ret = rtimer_set(&generic_timer, callback_time, 1,
                                     (void (*)(struct rtimer *, void *))auto_channel_hopping, NULL);

            if(ret){
                PRINTF("error auto_channel\n");
            }
        }
    }
    
    /*uint8_t ploss = 10;
    if(node_link_active(ploss) == 0){
        //PRINTF("T_%2d FAIL...\n", slot_increment_offset+1);
        return;
    }*/

    if(pkt->ptype == DATA_PACKET){

        packet_received_flag = 1;

        leds_toggle(LEDS_BLUE);
        leds_toggle(LEDS_YELLOW);
        leds_toggle(LEDS_RED);

        //add possible neighbors to the table../
        if(all_neighbors_found_flag == 0){

            add_neighbors(&pkt->data[0]);

            //if (node_id == 2){
                PRINTF("T%u  ", slot_increment_offset+1);
                uint8_t k;
                for (k = 0; k < num_neighbors; k++){
                    PRINTF("%2u", pkt->data[k*2]);//neigh_table[
                    //PRINTF("%2d ",neigh_table[k].node_id);
                }
                PRINTF(" -> ");
                for (k = 0; k < num_neighbors; k++){
                    //PRINTF("%2u", pkt->data[k*2]);//neigh_table[
                    PRINTF("%2d ",neigh_table[k].node_id);
                }
                PRINTF("\n");
                //check if we have found all our neighbors/
            //} //end of node_id==2


            if(all_neighbors_found(num_neighbors)){

                all_neighbors_found_flag = 1;

                discovery_time = slot_increment_offset + 1;

                mean_disc_time += discovery_time;

                if(discovery_time > max_disc_time){
                    max_disc_time = discovery_time;
                }

                PRINTF(">:%d,%d,%u,%u,%u-%u\n", rounds_counter, node_id,
                       CHANNEL_POOL_SIZE, num_neighbors, discovery_time, mean_disc_time/rounds_counter);
            }

        } //end of <all_neighbors_found_flag>
    } //end of <pkt->ptype == DATA_PACKET>
}
/*---------------------------------------------------------------------------*/
/**
 * @brief send_packet
 * @param sent
 * @param ptr
 */
static void send_packet(mac_callback_t sent, void *ptr){
   //do nothing...
}
/*---------------------------------------------------------------------------*/
/**
 * @brief send_list
 * @param sent
 * @param ptr
 * @param buf_list
 */
static void
send_list(mac_callback_t sent, void *ptr, struct rdc_buf_list *buf_list){
    //do nothing...
}
/*---------------------------------------------------------------------------*/
/**
 * @brief on
 * @return
 */
static int rd_on(void){
  return NETSTACK_RADIO.on();
}
/*---------------------------------------------------------------------------*/
/**
 * @brief off
 * @param keep_radio_on
 * @return
 */
static int rd_off(int keep_radio_on){
    return NETSTACK_RADIO.off();
}
/*---------------------------------------------------------------------------*/
/**
 * @brief channel_check_interval
 * @return
 */
static unsigned short channel_check_interval(void){
  return 8;
}
/*---------------------------------------------------------------------------*/
/**
 * @brief init: initialization function, it's called by the network driver
 *              main function to start the RDC-Radio Duty Cycling layer
 */
static void init(void){

  phase_timer_offset = 1 + adapt_num_neighbors;

  memset(neigh_table, 0, sizeof(neigh_table));

  node_info_t *np  =  &neigh_table[0];
  np->node_id      =  node_id; //(rimeaddr_node_addr.u8[0]  & 0xff);
  np->hopcount     =  0;

  on();
}
/*---------------------------------------------------------------------------*/
/**
 * @brief PROCESS_THREAD
 */
PROCESS_THREAD(adapt_multich_epidemic_discovery, ev, data){

    PROCESS_BEGIN();

    /*PRINTF("node_id: %2u, netsize: %2u, num_ch: %2u, hopc: %u\n",
           node_id, num_neighbors, num_channels, HOPCOUNT_FILTER_NDISC);*/

    while(1) {
        PROCESS_WAIT_EVENT_UNTIL(ev == serial_line_event_message);

        char *command = (char*)data;

        if(command != NULL){
            if(!strcmp(command, "mean")){
                PRINTF("report,%u,%u,%u,%u,%u,%u,%u,E\n", rounds_counter, node_id, num_channels,
                       num_neighbors,  MAX_HOPCOUNT, mean_disc_time/rounds_counter, max_disc_time);
            }
            if(!strcmp(command, "nodeinfo")){
                if(node_id != 0){
                    PRINTF("nid:,%u,%u,E\n", node_id, rimeaddr_node_addr.u8[1]);
                }else{
                    PRINTF("rid:,%u,E\n", rimeaddr_node_addr.u8[1]);
                }
            }
            if(!strcmp(command, "allconv")){
                /*test_case related
                 *all nodes have converged..
                */
                cc2420_set_channel(26);
                nd_is_started = 0;
                clear_vars_val = 1;

            }
            if(!strcmp(command, "newround")){
                /*test_case related all nodes have converged..begin a new round*/
                //nd_is_started = 1;
                all_neighbors_found_flag = 0;
                rounds_counter = rounds_counter + 1;

                /*
                //initialize the next phase to be executed
                //turn the radio off..
                off();

                //initialize the channel hopping by selecting a random channel.
                nxt_channel_idx =  random_int(num_channels);
                cc2420_set_channel(i3e154_channels[nxt_channel_idx]);

                on();

                callback_time = RTIMER_NOW() + 2;
                ref_timer = callback_time;

                uint8_t ret = rtimer_set(&generic_timer, callback_time, 1,
                                         (void (*)(struct rtimer *, void *))auto_channel_hopping, NULL);

                if(ret){
                    PRINTF("error auto_channel\n");
                }*/
            }
        }
    }
    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
const struct rdc_driver nodedisc_driver = {
    "adapt-medal-rdc",
    init,
    send_packet,
    send_list,
    packet_input,
    rd_on,
    rd_off,
    channel_check_interval,
};
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(adapt_print_process, ev, data){
    PROCESS_BEGIN();

    PROCESS_END();
}

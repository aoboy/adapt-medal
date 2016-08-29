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
///=========================================================================/
///=========================================================================/
#include "contiki-conf.h"
#include "dev/leds.h"
#include "dev/radio.h"
#include "dev/watchdog.h"
#include "lib/random.h"
/** RDC driver file*/
#include "./adapt-medal.h"
/** */
#include "net/netstack.h"
#include "net/rime.h"
#include "sys/compower.h"
#include "sys/pt.h"
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

///=========================================================================/
///=========================================================================/
#define ONE_KILO            (1024)
#define ONE_MSEC            (RTIMER_SECOND/ONE_KILO)
#define TS                  (15*ONE_MSEC)
#define TS_75P              (3*TS)/4
#define HALF_MSEC           (ONE_MSEC/2)
#define GUARD_PERIOD        ONE_MSEC
#define SWITCHING_PERIOD    (3*ONE_MSEC)
#define DATA_PERIOD         (5*ONE_MSEC)
#define FRAME_PERIOD        (16*TS)
///=========================================================================/
///=========================================================================/
static char* newRound="newround";
static char* outPUT="output";
#define SIZE_NETWORK 11
///=========================================================================/
///=========================================================================/
typedef rtimer_clock_t my_clock_t;
static volatile rtimer_clock_t callback_time;     

static struct rtimer generic_timer;
///=========================================================================/
static volatile uint16_t slot_increment_offset = 0;

static volatile uint16_t upper_bound_time = 50;

/**
 * @brief phase_i: the variable that holds at which phase we are in
 */
static volatile uint8_t  phase_i  = 0;
static volatile uint8_t adapt_num_neighbors = 2;

static volatile uint8_t curr_num_neighbors  = 1;

static volatile uint16_t phase_timer_offset = 0;
static volatile uint16_t phase_length = 0;
///=========================================================================/
static volatile uint16_t discovery_time = 0;
static volatile uint16_t mean_disc_time = 0;
static volatile uint16_t max_disc_time  = 0;
///=========================================================================/
static uint8_t rounds_counter = 0;
static uint8_t num_rounds_counter = 0;

static my_clock_t ref_timer;

///=========================================================================/
static volatile uint8_t radio_is_on = 0;
///=========================================================================/
//static uint8_t num_neighbors = 0;


static volatile uint8_t nd_is_started = 0;

static volatile uint8_t nxt_channel_idx  = 0;
static volatile uint8_t clear_vars_val   = 0;

static volatile uint8_t packet_received_flag = 0;

static volatile uint8_t radio_read_flag = 0;

///=========================================================================/
///=========================================================================/
/* the sender node_id: helps create links whose sender_id is the node that was 
 * behaving/acting as a transmitter
 */
static volatile uint8_t all_neighbors_found_flag = 0;
///=========================================================================/
static uint8_t i3e154_channels [] ={11, 12, 13, 14, 15, 16, 17,
                                    18, 19, 20, 21, 22, 23, 24, 25, 26};
///=========================================================================/
PROCESS(async_adapt_process, "adapt-medal process");
PROCESS(nodeoutput_process, "print serial process");
//AUTOSTART_PROCESSES(&async_adapt_process, &nodeoutput_process);
///=========================================================================/
static void on(void){
  if(radio_is_on == 0) {
    radio_is_on = 1;
    NETSTACK_RADIO.on();
  }
}
///=========================================================================/
///=========================================================================/
static void off(void){
  if(radio_is_on) {
    radio_is_on = 0;
    NETSTACK_RADIO.off();
  }
}
///=========================================================================/
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
///=========================================================================/
uint8_t get_curr_neighbors(){
    return curr_num_neighbors;
}
///=========================================================================/
uint8_t get_elapsed_rounds(){
    return rounds_counter;
}
///=========================================================================/
uint16_t get_discovery_latency(){
    return discovery_time;
}
///=========================================================================/
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
static uint16_t calc_trans_p(const uint8_t k, const uint16_t n){
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

///=========================================================================/
/**
 * @brief clear_vars
 */
static void reset_process(){
    clear_vars_val = 0;

    nd_is_started      = 0;
    slot_increment_offset = 0;
    packet_received_flag  = 0;
    all_neighbors_found_flag = 0;

    curr_num_neighbors = 0;

    cc2420_set_channel(26);

    //turn radio off...
    off();

    neighs_flush_all();

    discovery_time = 0;

    //Reset adaptive parameters..
    phase_i = 0;
    adapt_num_neighbors = 2;
    phase_timer_offset = 1 + adapt_num_neighbors;

    //process_post(&async_adapt_process, serial_line_event_message, newRound);
}
///=========================================================================/
/**
 * @brief transmit_beacon
 */
static void transmit_beacon(void){

    data_packet_t d2trans;

    memset(&d2trans, 0, DATA_PACKET_SIZE);

    d2trans.type = DATA_PACKET; 

    if(neighs_add2payload(&d2trans.data[0])){

        if(radio_read_flag == 0) {
            NETSTACK_RADIO.send((void*)&d2trans, DATA_PACKET_SIZE);
        }
    }
}
///=========================================================================/
/**
 * @brief transmit_beacon
 */
/*static void transmit_beacon(void){

    data_packet_t d2trans;

    memset(&d2trans, 0, DATA_PACKET_SIZE);

    d2trans.type = DATA_PACKET; 
    struct data_item_t *dt_item = (struct data_item_t *)(&d2trans.data[0]);

    dt_item->hopcount = 1;
    dt_item->node_id  = rimeaddr_node_addr.u8[0];

    if(radio_read_flag == 0) {
        NETSTACK_RADIO.send((void*)&d2trans, DATA_PACKET_SIZE);
    }
}*/
///=========================================================================/
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

    ///WATCHDOG
    watchdog_stop();
    
    if(nd_is_started  && curr_num_neighbors < num_neighbors
                     /*&& slot_increment_offset < 4*upper_bound_time*/){

        //turn radio on if it was previously turned off
        if(radio_is_on == 0) {	
             on();
        }
        slot_increment_offset++;

        if (slot_increment_offset != 0 &&
                (slot_increment_offset % phase_timer_offset) == 0){
            //increment phase_i.
            phase_i = phase_i + 1;
            //uint8_t shift_val = phase_i+1;
            //Compute number of neighbors for phase i.
            adapt_num_neighbors = (1 << phase_i); // N_i = 2^i
            //compute length of the current phase or epoch.
            phase_length = 4*phase_i*adapt_num_neighbors;
            //compute time when next phase ends
            phase_timer_offset = phase_timer_offset + phase_length;
        }
        
        //generate a random prob to compare with.
        uint16_t prob = random_rand();
	
        //generate a random channel
        nxt_channel_idx =  random_int(num_channels);
	
        //compute trans probability
        uint16_t tx_prob = calc_trans_p(num_channels, adapt_num_neighbors);

        //set the channel..
        cc2420_set_channel(i3e154_channels[nxt_channel_idx]);

        if(prob < tx_prob){
	  
	    uint16_t delay = 0;
	    
	    rtimer_clock_t t_end= RTIMER_NOW() + 2*ONE_MSEC;

	    while(RTIMER_CLOCK_LT(RTIMER_NOW(), t_end)){
		delay = delay + 1;
	    }	
	    
	    //transmit a beacon packet..
            transmit_beacon();
        }else{
            //listen
        }

        //callback_time   = (ref_timer + (slot_increment_offset*TS));
        callback_time = RTIMER_NOW() + TS;

        int ret = rtimer_set(&generic_timer, callback_time, 1,
                         (void (*)(struct rtimer *, void *))auto_channel_hopping, NULL);

        leds_toggle(LEDS_GREEN);

        if(ret){
            PRINTF("synchronization failed\n");
        }
    }else{
        reset_process();
    }

        ///WATCHDOG
    watchdog_start();
    
    return 0;
}
///=========================================================================/
static void node_trigger(){    


        //turn radio on/off  ....
        //on();
        off();

        //callback_time = RTIMER_NOW() + TS*(1+random_int(200));
	callback_time = RTIMER_NOW() + TS*(1 /*+random_int(2)*/);
        ref_timer = callback_time;

        int ret = rtimer_set(&generic_timer, callback_time, 1,
                                 (void (*)(struct rtimer *, void *))auto_channel_hopping, NULL);

        if(ret){
            PRINTF("error auto_channel\n");
        }
}
///=========================================================================/
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

    ///PRINTF("gotIT1\n");

    if(len > 0) {
        packetbuf_set_datalen(len);
    }else{
        ///PRINTF("collision\n");
        return;
    }

    data_packet_t *inpkt = (data_packet_t*)packetbuf_dataptr();

    if(inpkt->type == DATA_PACKET){

        packet_received_flag = 1;

        leds_toggle(LEDS_BLUE);
        leds_toggle(LEDS_YELLOW);
        leds_toggle(LEDS_RED);

        //add possible neighbors to the table../
        if(all_neighbors_found_flag == 0){

            ///add new contents to table if it contains any new entry
            //neighs_sregister(&inpkt->data[0]);
	    //register for multihop epidemic discovery..
	    neighs_register(&inpkt->data[0]);

            ///curr_num_neighbors
            ///
            uint8_t tmp_num_neighs = neighs_num_nodes();

            //if(all_neighbors_found(num_neighbors)){
            if(curr_num_neighbors < tmp_num_neighs){

                //set the new number as the current number of neighbors..
                curr_num_neighbors = tmp_num_neighs;


                /*if(all_neighbors_found(num_neighbors)){
                    all_neighbors_found_flag = 1;
                }*/

                discovery_time = slot_increment_offset + 1;

                mean_disc_time += discovery_time;

                if(discovery_time > max_disc_time){
                    max_disc_time = discovery_time;
                }

                process_post(&nodeoutput_process, PROCESS_EVENT_CONTINUE, outPUT);
            }else{
		//
	        
		//process_post(&nodeoutput_process, PROCESS_EVENT_CONTINUE, outPUT);
	    }

        } //end of <all_neighbors_found_flag>
    } //end of <pkt->ptype == DATA_PACKET>
}
///=========================================================================/
/**
 * @brief send_packet
 * @param sent
 * @param ptr
 */
static void send_packet(mac_callback_t sent, void *ptr){
   //do nothing...
}
///=========================================================================/
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
///=========================================================================/
/**
 * @brief on
 * @return
 */
static int rd_on(void){
  return NETSTACK_RADIO.on();
}
///=========================================================================/
/**
 * @brief off
 * @param keep_radio_on
 * @return
 */
static int rd_off(int keep_radio_on){
    return NETSTACK_RADIO.off();
}
///=========================================================================/
/**
 * @brief channel_check_interval
 * @return
 */
static unsigned short channel_check_interval(void){
  return 8;
}
///=========================================================================/
/**
 * @brief init: initialization function, it's called by the network driver
 *              main function to start the RDC-Radio Duty Cycling layer
 */
static void init(void){

  ///initialize list of neighbors
  neighs_init();

  reset_process();  

  //set the channel..
  nxt_channel_idx = random_int(num_channels);
  cc2420_set_channel(i3e154_channels[nxt_channel_idx]);

  //turn radio on..
  //on();

  //turn radio off
  off();

  process_start(&async_adapt_process, NULL);

  process_start(&nodeoutput_process, NULL);
}
///=========================================================================/
void startup_protocol(uint8_t start_id){

    if(1 /** rimeaddr_node_addr.u8[0] == start_id*/){

        nd_is_started = 1;
        rounds_counter = 1;

        //AUTOMATIC MODE.....
	off();
	
        node_trigger ();
    }
}
///=========================================================================/
/**async_adapt-medal.c
 * @brief PROCESS_THREAD
 */
PROCESS_THREAD(async_adapt_process, ev, data){
    static struct etimer et;

    PROCESS_BEGIN();

    while(1) {
        PROCESS_WAIT_EVENT_UNTIL(ev == serial_line_event_message);

        char *command = (char*)data;

        if(command != NULL){
            if(!strcmp(command, "allconv")){
                /*test_case related
                 *all nodes have converged..
                */
                cc2420_set_channel(26);
                nd_is_started = 0;
                clear_vars_val = 1;
		
		off();

                ///NOTE: temp solution.. just for simulation with script..
                //reset_process();
            }
            if(!strcmp(command, "newround")){
                /*test_case related all nodes have converged..begin a new round*/
                nd_is_started = 1;
                all_neighbors_found_flag = 0;
                rounds_counter = rounds_counter + 1;

                //initialize the next phase to be executed
                //turn the radio off..
                off();

                ///initialize discovery process....
                node_trigger();
            }
        }
    }
    PROCESS_END();
}
///=========================================================================/
///=========================================================================/
PROCESS_THREAD(nodeoutput_process, ev, data){
    static struct etimer et;

    PROCESS_BEGIN();

    while(1){

        PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_CONTINUE);
        //PROCESS_WAIT_EVENT_UNTIL(ev == serial_line_event_message);

        /*char *command = (char*)data;

        if(command != NULL){
	  
	}*/
	if(curr_num_neighbors/**get_curr_neighbors()*/ == num_neighbors){
	      PRINTF("END>:%d,%d,%u,%u,%u,%u\n", get_elapsed_rounds(), rimeaddr_node_addr.u8[0],
                  CHANNEL_POOL_SIZE, neighs_num_nodes(),  neighs_1hop(), get_discovery_latency());
	}else{
	      PRINTF(">:%d,%d,%u,%u,%u,%u\n", get_elapsed_rounds(), rimeaddr_node_addr.u8[0],
                  CHANNEL_POOL_SIZE, neighs_num_nodes(),  neighs_1hop(), get_discovery_latency());	  
	}

    }
    PROCESS_END();
}
///=========================================================================/
///=========================================================================/
const struct rdc_driver adaptmecc_driver = {
    "adapt-medal-rdc",
    init,
    send_packet,
    send_list,
    packet_input,
    rd_on,
    rd_off,
    channel_check_interval,
};
///=========================================================================/
///=========================================================================/

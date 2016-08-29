/** ------------------------------------------------------------------------
 * Department of Automatic Control,
 * KTH - Royal Institute of Technology,
 * School of Electrical Engineering
 * @address: Osquldasvag 10, SE-10044, STOCKHOLM, Sweden
 * @author: António Gonga < gonga@ee.kth.se>
 *
 * @date: May 15th, 2014
 * @filename: indriya-medal-example.c
 * @description: this example is used together with the medal RDC layer
 *               for testbed evaluations such as INDRIYA and TWIST*
 * NOTICE: This file is part of research I have been developing at KTH. You
 *         are welcome to modify it, AS LONG AS you leave this head notice
 *         and the author is properly acknowledged.
 * ------------------------------------------------------------------------*/

#include "./amedal.h"

#include "contiki.h"
#include "net/rime.h"
#include "random.h"

#include "dev/button-sensor.h"

#include "dev/leds.h"

#include "node-id.h"

#define DEBUG 1

#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

static struct rime_sniffer pkt_rcv_sniffer;

static uint8_t start_app_flag = 0;

//static void (* input_pkt)(void);
static void input_pkt();


RIME_SNIFFER(pkt_rcv_sniffer, input_pkt, NULL);

/*---------------------------------------------------------------------------*/
PROCESS(example_adaptive_process, "example medal-adaptive");
AUTOSTART_PROCESSES(&example_adaptive_process);
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
extern uint8_t get_curr_neighbors();
extern uint8_t get_elapsed_rounds();
extern uint16_t get_discovery_latency();
extern void startup_protocol(uint8_t start_id);
/*---------------------------------------------------------------------------*/
static void input_pkt()
{
   //PRINTF("packet rcvd");
   /*PRINTF("K>:%d,%d,%u,%u,%u,%u\n", get_elapsed_rounds(), rimeaddr_node_addr.u8[0],
          CHANNEL_POOL_SIZE, get_curr_neighbors(), neighbors_1hop(),get_discovery_latency());*/
   
   if(get_curr_neighbors() == num_neighbors){
       PRINTF("END>:%d,%d,%u,%u,%u,%u\n", get_elapsed_rounds(), rimeaddr_node_addr.u8[0],
              CHANNEL_POOL_SIZE, get_curr_neighbors(),  neighs_1hop(),
                                 get_discovery_latency());
   }
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
//#define STARTER_ID 174
#define STARTER_ID 1
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_adaptive_process, ev, data)
{
    static struct etimer et;
    /** the process exits removing the sniffer created.*/
    //PROCESS_EXITHANDLER(rime_sniffer_remove(&pkt_rcv_sniffer);)

    PROCESS_BEGIN();

    /** Create a sniffer to receive incoming packets*/
    RIME_SNIFFER(pkt_rcv_sniffer, input_pkt, NULL);

    /** Tell RIME to add the sniffer*/
    rime_sniffer_add(&pkt_rcv_sniffer);
 
     //start up protocol..
  
    if(start_app_flag == 0){
      start_app_flag = 1;
      
      startup_protocol(1);
    }
    PRINTF("NET_SIZE: %u, PLD_SIZE: %u\n", CONF_NETWORK_SIZE, NETWORK_PAYLOAD_SIZE);
    
    while(1) {
        PROCESS_YIELD();
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/


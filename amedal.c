
#include "lib/list.h"
#include "lib/memb.h"
#include "contiki-conf.h"
#include "net/rime/rimeaddr.h"

#include "./amedal.h"

#include <string.h>
///=========================================================================/
///=========================================================================/
#define CDEBUG 0
#if CDEBUG
#include <stdio.h>
//volatile char *cooja_debug_ptr;
volatile char *cooja_debug_ptr;
#define COOJA_DEBUG_PRINTF(...) \
    do{ char tmp[100]; sprintf(tmp,__VA_ARGS__); cooja_debug_ptr=tmp; } while(0);
#else //COOJA_DEBUG_PRINTF
#define COOJA_DEBUG_PRINTF(...)
#endif //COOJA_DEBUG_PRINTF
///=========================================================================/
///=========================================================================/
LIST(neighs_list);
#if CONF_NETWORK_SIZE != 0
MEMB(neighs_memb, struct nodelist_item, CONF_NETWORK_SIZE);
#else
MEMB(neighs_memb, struct nodelist_item, 2);
#endif //CONF_NETWORK_SIZE != 0

static volatile uint8_t neighbors_update_flag = 0;
///=========================================================================/
///=========================================================================/
void neighs_init(){
    //
      list_init(neighs_list);
      memb_init(&neighs_memb);
      
      neighs_add_itself();
}
///=========================================================================/
///=========================================================================/
/**
 * @brief neighs_get
 * @param nodeid
 * @return
 */
struct nodelist_item *neighs_get(uint8_t nodeid){
    struct nodelist_item *localp = list_head(neighs_list);

    for( ; localp != NULL; localp = list_item_next(localp)){
        if (localp->node_id == nodeid){
            return localp;
        }
    }
    return NULL;
}
///=========================================================================/
///=========================================================================/
void neighs_add_itself(){

    struct nodelist_item *ais = neighs_get(rimeaddr_node_addr.u8[0]);
    if(ais == NULL){
        //we add a new element..
        ais = memb_alloc(&neighs_memb);
        if(ais != NULL){
            ais->node_id  = rimeaddr_node_addr.u8[0];
            ais->hopcount = 0;
            ais->next     = NULL;

            //add new element to the list..
            list_add(neighs_list, ais);

            return;
        }
    }
}
///=========================================================================/
///=========================================================================/
/**
 * @brief get_fraction_nodes
 * @return
 */
uint8_t neighs_num_nodes(){
    uint8_t frac_neigh = 0;

    struct nodelist_item *lpf = list_head(neighs_list);

    for( ; lpf != NULL; lpf = list_item_next(lpf)){
        frac_neigh = frac_neigh + 1;
    }

    return frac_neigh;
}
///=========================================================================/
///=========================================================================/
uint8_t neighs_1hop(void){
    uint8_t frac_1hop_neigh = 0;

    struct nodelist_item *lpfx = list_head(neighs_list);

    for( ; lpfx != NULL; lpfx = list_item_next(lpfx)){
        if(lpfx->hopcount == 1){
            frac_1hop_neigh = frac_1hop_neigh + 1;
        }
    }

    return frac_1hop_neigh;
}
///=========================================================================/
///=========================================================================/
/**
 * @brief neighs_full
 * @return
 */
uint8_t neighs_all_found(){
    uint8_t i = 0;
    struct nodelist_item *lp = list_head(neighs_list);
    
    i = list_length(neighs_list);

    /*for( ; lp != NULL; lp = list_item_next(lp)){
        i++;
    }*/
    if (i ==num_neighbors){
        return 1;
    }
    return 0;
}
///=========================================================================/
///=========================================================================/
uint8_t neighs_updated(){
  if(neighbors_update_flag){
      return 1;
  }else{
      return 0;
  }
}
///=========================================================================/
///=========================================================================/
uint8_t neighs_updateReset(){
  neighbors_update_flag = 0;
}
///=========================================================================/
///=========================================================================/
/**
 * @brief neighs_remove
 * @param nodeid
 */
void neighs_remove(uint8_t nodeid){
    struct nodelist_item *n2r = neighs_get(nodeid);
    if(n2r != NULL){
        list_remove(neighs_list, n2r);
        memb_free(&neighs_memb, n2r);
    }
}
///=========================================================================/
///=========================================================================/
/**
 * @brief neighs_remove_all
 */
void neighs_remove_all(){
    struct nodelist_item *n2ra = list_head(neighs_list);

    for (; n2ra != NULL; n2ra = list_item_next(n2ra)){
        list_remove(neighs_list, n2ra);
        memb_free(&neighs_memb, n2ra);
    }
}
///=========================================================================/
///=========================================================================/
void neighs_flush_all(void){
  struct nodelist_item *n2fa;

  while(1){

    n2fa = list_pop(neighs_list);

    if(n2fa != NULL) {
      memb_free(&neighs_memb, n2fa);
    }else {
      ///add first entry here ??
      ///      
      neighs_add_itself();
      
      //reactualize updates...
      neighbors_update_flag = 0;

      break;
    }
  } ///end of while(1)
}
///=========================================================================/
///=========================================================================/
/**
 * @brief neighs_register: the function is used to register/update neighbor
 *                         nodes.
 * @param pkt_hdr
 * @return
 */
uint8_t neighs_register(uint8_t *pkt_hdr){
    uint8_t k = 0, max_nodes= NETWORK_PAYLOAD_SIZE/DATA_ITEM_LEN;

    for ( k = 0; k < max_nodes; k++){
      
        uint8_t dpos = k*DATA_ITEM_LEN;

        struct data_item_t *ditem = (struct data_item_t*)(pkt_hdr + dpos);

        if (ditem->node_id == 0 || ditem->hopcount > MAX_HOPCOUNT){
            //reduces load to update/add
            return 0;
        }

        struct nodelist_item *nli = neighs_get(ditem->node_id);
	
        if(nli == NULL){
            //node does not exist in our table.. we add a new element..
            nli = memb_alloc(&neighs_memb);
	    
            if(nli != NULL){
                nli->node_id  = ditem->node_id;
                nli->hopcount = ditem->hopcount;
                nli->next     = NULL;

                //add new element to the list..
                list_add(neighs_list, nli);
		
		neighbors_update_flag = 1;
            }
        }else{
            //we update an existing element..
            if(nli->node_id == ditem->node_id && ditem->hopcount <= MAX_HOPCOUNT){
                //nli->hopcount = ditem->hopcount;
                //we make an update if we receive a neighbor with a smaller hop
                //count
                if(nli->hopcount > ditem->hopcount){
                    nli->hopcount = ditem->hopcount;
		    
		    neighbors_update_flag = 1;
                }
            }
        } //else

    }   

    return 0;
}
///=========================================================================/
///=========================================================================/
/**
 * @brief neighs_sregister: adds or updates a single element to the neighbors
 *                          table. The Epidemic mechanism is not used
 * @param pkt_hdr
 * @return
 */
uint8_t neighs_sregister(uint8_t *pkt_hdr){
    uint8_t k = 0;

    for ( k = 0; k < 1; k++){
        uint8_t dpos = k*DATA_ITEM_LEN;

        struct data_item_t *ditem = (struct data_item_t*)(pkt_hdr + dpos);

        if (ditem->node_id == 0 || ditem->hopcount > MAX_HOPCOUNT){
            //reduces load to update/add
            return 0;
        }

        struct nodelist_item *nli = neighs_get(ditem->node_id);
        if(nli == NULL){
            //we add a new element..
            nli = memb_alloc(&neighs_memb);
            if(nli != NULL){
                nli->node_id  = ditem->node_id;
                nli->hopcount = ditem->hopcount;
                nli->next     = NULL;

                //add new element to the list..
                list_add(neighs_list, nli);
            }
        }else{
            //we update an existing element..
            if(nli->node_id == ditem->node_id && ditem->hopcount <= MAX_HOPCOUNT){
                //we make an update if we receive a neighbor with a smaller hop
                //count
                if(nli->hopcount > ditem->hopcount){
                    nli->hopcount = ditem->hopcount;
                }
            }
        } //else

    }

    return 0;
}
///=========================================================================/
///=========================================================================/
uint8_t neighs_add2payload(uint8_t *data_ptr){
    uint8_t pkt_offset = 0, llen=0, lidx=0, diff;

    struct nodelist_item *a2p = list_head(neighs_list);
    
    llen = list_length(neighs_list);
    
    if(llen <= (NETWORK_PAYLOAD_SIZE/DATA_ITEM_LEN)){
	diff = 0;
    }else{
	diff = llen - NETWORK_PAYLOAD_SIZE/DATA_ITEM_LEN;
    }

    for(lidx = 0;    a2p != NULL;    a2p = list_item_next(a2p), lidx=lidx + 1){

        if(a2p->hopcount < MAX_HOPCOUNT && lidx >= diff){

            struct data_item_t *d2a = (struct data_item_t *)(&data_ptr[pkt_offset]);

            d2a->node_id = a2p->node_id;
            d2a->hopcount= a2p->hopcount + 1;

            pkt_offset = pkt_offset + DATA_ITEM_LEN;
        }
    }
    if(pkt_offset){
        return 1;
    }
    //this is a serious error.. :(
    return 0;
}
///=========================================================================/
///=========================================================================/
/**
 * @brief log2 - computes the logarithm base 2, i.e, log(N,2) of a number N
 * @param n
 * @return
 */
uint8_t log2_n(uint16_t n){
    int bits = 1;
    int b;
    for (b = 8; b >= 1; b/=2){
        int s = 1 << b;
        if (n >= s){
            n >>= b;
            bits += b;
        }
    }
    bits = bits - 1;
    return bits;
}
///=========================================================================/
///=========================================================================/
/**
 * @brief random_int
 * @param size_num
 * @return
 */
uint16_t random_int(uint16_t size_num){

    uint16_t rand_num = random_rand();

    if(size_num == 0){
        return 0;
    }

    uint16_t val = (65535 / size_num);
    if ( rand_num < val){
        return 0;
    }else{
        uint16_t k;
        for(k = 1; k < size_num; k++){
            if (rand_num >= k*val && rand_num <= (k+1)*val){
                return k;
            }
        }
    }
    return 0;
}
///=========================================================================/
///=========================================================================/

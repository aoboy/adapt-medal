#SET TO Contiki-2.7/ renamed as nddev-contiki
CONTIKI = $(DROPBOX_HOME)/neighbor-discovery/nddev-contiki

ifndef TARGET
TARGET=sky
endif

CONTIKI_SOURCEFILES += amedal.c epidemic_adaptive_medal.c

DEFINES=NETSTACK_MAC=nullmac_driver,NETSTACK_RDC=adaptmecc_driver,CC2420_CONF_AUTOACK=0
DEFINES+=CONF_CHANNEL_POOL_SIZE=4,CONF_NETWORK_SIZE=105
DEFINES+=HOPCOUNT_FILTER_NDISC=15

#DEFINES+=IN_INDRIYA=1
#DEFINES+=IN_TWIST=1

clear:
	rm -rf *.sky symbols.* obj_* *~
	make clean

bin-amedal:
	make clear
	make example-adaptive-medal.upload

synchro:
	make clear
	make node-synch.upload
#	make nsynch-rbs.upload

tb-synchro:
	make clear
	make nsynch-tb.upload
        #cp nsynch-tb.sky tb-bin/nsynch-tb.exe
	#cp nsynch-tb.sky tb-bin-twist/nsynch-tb.exe


motelist:
	MOTES=$(shell $(MOTELIST) 2>&- | grep USB | \
	cut -f 4 -d \  | \
	perl -ne 'print $$1 . " " if(m-(/dev/\w+)-);')

reload:
	make sky-reset


include $(CONTIKI)/Makefile.include


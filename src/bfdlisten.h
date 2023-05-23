#ifndef GWTT_GPN_BFD_STATUS_H_
#define GWTT_GPN_BFD_STATUS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#define CMD_BUFF_SIZE    1024
#define BUFSIZE          1024

#define BFD_CONFIG_FILE  "/etc/config/bfd"
#define BFD_CONTROL_SOCK_PATH "/var/run/frr/bfdd.sock"
#define BFD_STATE_FILE  "/var/state/bfd"
#define BCM_NOTIFY_ALL					 ((uint64_t)-1)

/* Notify flags to use with bcm_notify. */
#define BCM_NOTIFY_ALL ((uint64_t)-1)
#define BCM_NOTIFY_PEER_STATE (1ULL << 0)
#define BCM_NOTIFY_CONFIG (1ULL << 1)
#define BCM_NOTIFY_NONE 0

/* Notification special ID. */
#define BCM_NOTIFY_ID 		0

enum bc_msg_type {
	BMT_RESPONSE = 1,
	BMT_REQUEST_ADD = 2,
	BMT_REQUEST_DEL = 3,
	BMT_NOTIFY = 4,
	BMT_NOTIFY_ADD = 5,
	BMT_NOTIFY_DEL = 6,
};


struct bfd_control_msg {
	/* Total length without the header. */
	uint32_t bcm_length;
	/*
	 * Message request/response id.
	 * All requests will have a correspondent response with the
	 * same id.
	 */
	uint16_t bcm_id;
	/* Message type. */
	uint8_t bcm_type;
	/* Message version. */
	uint8_t bcm_ver;
	/* Message payload. */
	uint8_t bcm_data[0];
};


#define INFO_SYSLOG(format, ...)								\
        do{														\
            syslog(LOG_INFO, format, __VA_ARGS__);				\
        }while(0)

#define DBG_SYSLOG(format, ...)									\
        do{														\
            syslog(LOG_DEBUG, format, __VA_ARGS__);				\
        }while(0)

#define ERR_SYSLOG(format, ...)									\
		do{														\
			syslog(LOG_ERR, format, __VA_ARGS__);				\
		}while(0)
#define ERR_SYS_GOTO(ret, exit, format, ...)					\
		do{ 													\
			if(0 != ret){										\
				syslog(LOG_ERR, format, __VA_ARGS__);			\
				goto exit;										\
			}													\
		}while(0)

typedef int (*bfd_control_recv_cb)(struct bfd_control_msg *, void *arg);

int bfd_control_init(const char *path);

uint16_t bfd_control_send(int sd, enum bc_msg_type bmt, const void *data, size_t datalen);

int bfd_bcm_recv(struct bfd_control_msg *bcm, void *arg);



#endif

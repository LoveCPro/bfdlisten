#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>
#include <uci.h>

#include "bfdlisten.h"

static const char *diag2str(uint8_t diag)
{
	switch (diag) {
	case 0:
		return "No Diagnostic";
	case 1:
		return "Control Detection Time Expired";
	case 2:
		return "Echo Function Failed";
	case 3:
		return "Neighbor Signaled Session Down";
	case 4:
		return "Forwarding Plane Reset";
	case 5:
		return "Path Down";
	case 6:
		return "Concatenated Path Down";
	case 7:
		return "Administratively Down";
	case 8:
		return "Reverse Concatenated Path Down";
	default:
		return "Reserved for future use";
	}
}


static char * gwtt_system(const char * cmd)
{
	FILE * fp;
	int res;
	char buf[BUFSIZE];
	char * tmp = (char *)calloc(1, 1024);


	if (cmd == NULL){
		return NULL;
	}
	if ((fp = popen(cmd, "r")) == NULL){
		return NULL;
	}

	while(fgets(buf, BUFSIZE, fp) != NULL){
		buf[strlen(buf) - 1] = '\0';
		strcat(tmp, buf);
	}

	res = pclose(fp);

	return tmp;
}

int tcp_wr_state(	struct json_object *obj)
{
	struct json_object *val_obj = NULL;
	const char * op = NULL;
	const char * dstip = NULL;
	const char * srcip = NULL;
	const char * ifname = NULL;
	const char * state = NULL;

	time_t t;
	struct tm * lt;
	char timestr[64] = {0};

	int diag=  0;
	char cmd[CMD_BUFF_SIZE] = {0};
	int flag = 0;
	int rc = UCI_OK;
	struct uci_package *p = NULL;
	struct uci_element *e = NULL;
	struct uci_section *s = NULL;
	const char *value = NULL;

	struct uci_context *uci_ctx = NULL;
	uci_ctx = uci_alloc_context();
	if(NULL == uci_ctx){
		ERR_SYSLOG("BFDLISTEN: failed in allocing context! Func=%s,Line=%d\n",__FUNCTION__, __LINE__);
	}

	rc = uci_load(uci_ctx, BFD_CONFIG_FILE, &p);
    ERR_SYS_GOTO(rc, fail, "BFDLISTEN: failed in loading uci file bfd! Func=%s,Line=%d,Errno=%d\n",
               __FUNCTION__, __LINE__, rc);

	json_object_object_get_ex(obj, "op", &val_obj);
	if(val_obj == NULL){
		return -1;
	}
	op = json_object_get_string(val_obj);
	if(strcmp(op, "status")){
		ERR_SYSLOG("BFDLISTEN : op is %s, Func=%s, Line=%d\n", op, __FUNCTION__, __LINE__);
		return 0;
	}

	json_object_object_get_ex(obj, "peer-address", &val_obj);
	if(val_obj != NULL){
		dstip  =  json_object_get_string(val_obj);
	}
	json_object_object_get_ex(obj, "local-address", &val_obj);
	if(val_obj != NULL){
		srcip  =  json_object_get_string(val_obj);
	}
	json_object_object_get_ex(obj, "local-interface", &val_obj);
	if(val_obj != NULL){
		ifname  =  json_object_get_string(val_obj);
	}
	json_object_object_get_ex(obj, "state", &val_obj);
	if(val_obj != NULL){
		state  =  json_object_get_string(val_obj);
	}


	json_object_object_get_ex(obj, "diagnostics", &val_obj);
	if(val_obj != NULL){
		diag  =  (uint8_t)atoi(json_object_get_string(val_obj));
	}

	uci_foreach_element(&p->sections, e){
       	s = uci_to_section(e);
		if(NULL != (value = uci_lookup_option_string(uci_ctx, s, "type"))){
			if(strcmp(value, "3")){
				continue;
			}
		}

		if(NULL != (value = uci_lookup_option_string(uci_ctx, s, "enabled"))){
			if(strcmp(value, "1")){
				continue;
			}
		}
		if(NULL == (value = uci_lookup_option_string(uci_ctx, s, "dstip"))){
			continue;
		}
		if(strcmp(value, dstip)){
			continue;
		}
		value = uci_lookup_option_string(uci_ctx, s, "srcip");
		if((srcip != NULL && value != NULL && !strcmp(value, srcip)) || (NULL == srcip && value == NULL)){

		}else{
			continue;
		}
		value = uci_lookup_option_string(uci_ctx, s, "interface");
		if((ifname != NULL && value != NULL && !strcmp(value, ifname)) || (NULL == ifname && value == NULL)){

		}else{
			continue;
		}
		flag = 1;
		break;

	}

	if(flag == 1){
		time(&t);
		lt = localtime(&t);
		sprintf(timestr, "%d/%d/%d %d:%d:%d", lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, \
					lt->tm_hour, lt->tm_min, lt->tm_sec);

		//write /var/state bfd status
		sprintf(cmd, "uci -P /var/state revert bfd.%s.state", s->e.name);
		system(cmd);	

		sprintf(cmd, "uci -P /var/state revert bfd.%s.diag", s->e.name);
		system(cmd);

		sprintf(cmd, "uci -P /var/state revert bfd.%s.time", s->e.name);
		system(cmd);

		sprintf(cmd, "uci -P /var/state set bfd.%s.state=\"%s\"", s->e.name, state);
		system(cmd);

		sprintf(cmd, "uci -P /var/state set bfd.%s.diag=\"%s\"", s->e.name, diag2str(diag));
		system(cmd);

		sprintf(cmd, "uci -P /var/state set bfd.%s.time=\"%s\"", s->e.name, timestr);
		system(cmd);

		//call hotpulg
		sprintf(cmd, "env -i BFDSECTION=\"%s\" /sbin/hotplug-call  bfd", s->e.name);
		printf("to call hotplucall........\n");
		system(cmd);
		printf("over hotplug call .........\n");
	}

fail:
	if(p){
		uci_unload(uci_ctx, p);
	}
	uci_free_context(uci_ctx);

	return 0;

}

int  bfd_bcm_recv(struct bfd_control_msg *bcm, void *arg)
{
	uint16_t *id = arg;
	struct json_object *jo;
	const char *jsonstr;

	if (ntohs(bcm->bcm_id) != *id) {
		DBG_SYSLOG("BFDLISTEN : expected id %d, but got %d, Func=%s, Line=%d\n", *id, ntohs(bcm->bcm_id), __FUNCTION__, __LINE__);
	}

	printf("func=%s, line=%d, dcm type =%d\n", __FUNCTION__, __LINE__, bcm->bcm_type);
	switch (bcm->bcm_type) {
	case BMT_RESPONSE:
		jo = json_tokener_parse((const char *)bcm->bcm_data);
		if (jo == NULL) {
//			printf("Response:\n%s\n", bcm->bcm_data);
		} else {
			jsonstr = json_object_to_json_string_ext(jo, JSON_C_TO_STRING_PRETTY);
//			printf("Response:\n%s\n", jsonstr);
			json_object_put(jo);
		}
		break;

	case BMT_NOTIFY:
		jo = json_tokener_parse((const char *)bcm->bcm_data);
		if (jo == NULL) {
//			printf("Notification:\n%s\n", bcm->bcm_data);
		} else {
			jsonstr = json_object_to_json_string_ext(jo, JSON_C_TO_STRING_PRETTY);
			printf("Notification:\n%s\n", jsonstr);
			tcp_wr_state(jo);
			json_object_put(jo);
		}
		break;

	case BMT_NOTIFY_ADD:
	case BMT_NOTIFY_DEL:
	case BMT_REQUEST_ADD:
	case BMT_REQUEST_DEL:
	default:
		DBG_SYSLOG("BFDLISTEN : invalid response type (%d), Func=%s, Line=%d\n", bcm->bcm_type, __FUNCTION__, __LINE__);
		exit(1);
	}

	return 0;
}


/*
 * Control socket
 */
int bfd_control_init(const char *path)
{
	struct sockaddr_un sun = {.sun_family = AF_UNIX,
				  .sun_path = BFD_CONTROL_SOCK_PATH};
	int sd;

	if (path) {
		strncpy(sun.sun_path, path, sizeof(sun.sun_path));
		sun.sun_path[sizeof(sun.sun_path) - 1] = 0;
	}

	sd = socket(AF_UNIX, SOCK_STREAM, PF_UNSPEC);
	if (sd == -1) {
		ERR_SYSLOG("BFDLISTEN: socket : %s, func=%s, line=%d", strerror(errno), __FUNCTION__, __LINE__);
		exit(1);
	}

	if (connect(sd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		ERR_SYSLOG("BFDLISTEN: connect : %s, func=%s, line=%d", strerror(errno), __FUNCTION__, __LINE__);
		exit(1);
	}

	return sd;
}

/* send msg */
uint16_t bfd_control_send(int sd, enum bc_msg_type bmt, const void *data,
		      size_t datalen)
{
	static uint16_t id = 0;
	const uint8_t *dataptr = data;
	ssize_t sent;
	size_t cur = 0;
	struct bfd_control_msg bcm = {
		.bcm_length = htonl(datalen),
		.bcm_type = bmt,
		.bcm_ver = 1,
		.bcm_id = htons(++id),
	};

	sent = write(sd, &bcm, sizeof(bcm));
	if (sent == 0) {
		ERR_SYSLOG("BFDLISTEN: write error : %s. func=%s, line=%d", strerror(errno),  __FUNCTION__, __LINE__);
		exit(1);
	}
	if (sent < 0) {
		ERR_SYSLOG("BFDLISTEN: write erorr : %s, func=%s, line=%d", strerror(errno), __FUNCTION__, __LINE__);
		exit(1);
	}

	while (datalen > 0) {
		sent = write(sd, &dataptr[cur], datalen);
		if (sent == 0) {
			ERR_SYSLOG("BFDLISTEN: write erorr : %s, func=%s, line=%d", strerror(errno), __FUNCTION__, __LINE__);
			exit(1);
		}
		if (sent < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK
			    || errno == EINTR)
				continue;
		ERR_SYSLOG("BFDLISTEN: write error: %s, func=%s, line=%d", strerror(errno), __FUNCTION__, __LINE__);
			exit(1);
		}

		datalen -= sent;
		cur += sent;
	}

	return id;
}

/*recv msg*/
int bfd_control_recv(int sd, bfd_control_recv_cb cb, void *arg)
{
	size_t bufpos, bufremaining, plen;
	ssize_t bread;
	struct bfd_control_msg *bcm, bcmh;
	int ret;

	bread = read(sd, &bcmh, sizeof(bcmh));
	if (bread == 0) {
		ERR_SYSLOG("BFDLISTEN: read error: %s, func=%s, line=%d", strerror(errno), __FUNCTION__, __LINE__);
		exit(1);
	}
	if (bread < 0) {
		ERR_SYSLOG("BFDLISTEN: read error: %s, func=%s, line=%d", strerror(errno), __FUNCTION__, __LINE__);
		exit(1);
	}

	if (bcmh.bcm_ver != 1) {
		ERR_SYSLOG("BFDLISTEN: wrong protocol version : %d, func=%s, line=%d", bcmh.bcm_ver, __FUNCTION__, __LINE__);
		exit(1);
	}

	plen = ntohl(bcmh.bcm_length);
	if (plen > 0) {
		/* Allocate the space for NULL byte as well. */
		bcm = malloc(sizeof(bcmh) + plen + 1);
		if (bcm == NULL) {
			ERR_SYSLOG("BFDLISTEN: malloc error: %s, func=%s, line=%d", strerror(errno), __FUNCTION__, __LINE__);
			exit(1);
		}

		*bcm = bcmh;
		bufremaining = plen;
		bufpos = 0;
	} else {
		bcm = &bcmh;
		bufremaining = 0;
		bufpos = 0;
	}

	while (bufremaining > 0) {
		bread = read(sd, &bcm->bcm_data[bufpos], bufremaining);
		if (bread == 0) {
			ERR_SYSLOG("BFDLISTEN: read error: %s, func=%s, line=%d", strerror(errno), __FUNCTION__, __LINE__);
			return -1;
		}
		if (bread < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK
			    || errno == EINTR)
				continue;

			ERR_SYSLOG("BFDLISTEN: read error: %s, func=%s, line=%d", strerror(errno), __FUNCTION__, __LINE__);
			exit(1);
		}

		bufremaining -= bread;
		bufpos += bread;
	}

	/* Terminate possible JSON string with NULL. */
	if (bufpos > 0)
		bcm->bcm_data[bufpos] = 0;

	/* Use the callback, otherwise return success. */
	if (cb != NULL)
		ret = cb(bcm, arg);
	else
		ret = 0;

	/*
	 * Only try to free() memory that was allocated and not from
	 * heap. Use plen to find if we allocated memory.
	 */
	if (plen > 0)
		free(bcm);

	return ret;
}


void * tcp_get_state_op(void *arg){
	INFO_SYSLOG("func %s start, line=%d\n",  __FUNCTION__, __LINE__);

	const char *ctl_path = BFD_CONTROL_SOCK_PATH;
	int csock;
	uint16_t cur_id;
	uint64_t notify_flags = BCM_NOTIFY_ALL;

	if ((csock = bfd_control_init(ctl_path)) == -1) {
		exit(1);
	}

	cur_id = bfd_control_send(csock, BMT_NOTIFY, &notify_flags,
					      sizeof(notify_flags));
	if (cur_id == 0) {
			ERR_SYSLOG("BFDLISTEN : failed to send message,Error:%s,Func=%s,Line=%d",strerror(errno), __FUNCTION__, __LINE__);
			exit(1);
	}

	bfd_control_recv(csock, bfd_bcm_recv, &cur_id);

//	printf("Listening for events\n");
	/* Expect notifications only */
	cur_id = BCM_NOTIFY_ID;
	while (bfd_control_recv(csock, bfd_bcm_recv, &cur_id) == 0) {
		/* NOTHING */;
	}

	INFO_SYSLOG("BFDSTATAUS : func=%s end.", __FUNCTION__);
	exit(1);
}

/*parse status info, and write bfd status file*/
char * ovs_parse_state(char * sessionname, const char * ifname){
	char cmd[CMD_BUFF_SIZE] = {0};
	char diag[64] = {0};
	char state[8] = {0};

	char * diagtmp = NULL;
	char * statetmp = NULL;

	time_t t;
	time(&t);

	struct tm * lt = localtime(&t);;
	char timestr[64] = {0};
	sprintf(timestr, "%d/%d/%d %d:%d:%d", lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, \
					lt->tm_hour, lt->tm_min, lt->tm_sec);

	char * bfdresult = NULL;
	char *index = NULL;

	//get bfd of interface
	sprintf(cmd, "ovs-vsctl  list interface %s | grep \"bfd_status\"",ifname);
	bfdresult = gwtt_system(cmd);
	if((index = strstr(bfdresult, "diagnostic")) != NULL){
		index = index + strlen("diagnostic") + 2;
		strncpy(diag, index, strlen(index) - strlen(strstr(index, "\"")));
	}else{
		strcpy(diag, "Path Down");
	}

	if((index = strstr(bfdresult, " state")) != NULL){
		index = index + strlen(" state") + 1;
		strncpy(state, index, strlen(index) - strlen(strstr(index, "}")));
	}else{
		strcpy(state, "down");
	}

	//get bfd before state
	sprintf(cmd, "uci -P /var/state get bfd.%s.diag 2>/dev/null", sessionname);
	diagtmp = gwtt_system(cmd);

	sprintf(cmd, "uci -P /var/state get bfd.%s.state 2>/dev/null", sessionname);
	statetmp = gwtt_system(cmd);

	//write bfd status file
	if(NULL == diagtmp || strcmp(diag, diagtmp)){
		sprintf(cmd, "uci -P /var/state revert bfd.%s.diag", sessionname);
		system(cmd);

		sprintf(cmd, "uci -P /var/state set bfd.%s.diag=\"%s\"", sessionname, diag);
		system(cmd);
	}

	if(NULL == statetmp || strcmp(state, statetmp)){
		sprintf(cmd, "uci -P /var/state revert bfd.%s.state", sessionname);
		system(cmd);

		sprintf(cmd, "uci -P /var/state set bfd.%s.state=\"%s\"", sessionname, state);
		system(cmd);

		sprintf(cmd, "uci -P /var/state revert bfd.%s.time", sessionname);
		system(cmd);

		sprintf(cmd, "uci -P /var/state set bfd.%s.time=\"%s\"", sessionname, timestr);
		system(cmd);

		//call hotpulg
		sprintf(cmd, "env -i BFDSECTION=\"%s\" /sbin/hotplug-call  bfd ", sessionname);
		system(cmd);
	}

	if(bfdresult){
		free(bfdresult);
		bfdresult = NULL;
	}
	if(diagtmp){
		free(diagtmp);
		diagtmp = NULL;
	}
	if(statetmp){
		free(statetmp);
		statetmp = NULL;
	}

	return NULL;
}

void * ovs_get_state_op(void *arg){
//DBG_SYSLOG("func=%s", __FUNCTION__);
	INFO_SYSLOG("func=%s start, line=%d\n",  __FUNCTION__, __LINE__);

	int rc = UCI_OK;
	struct uci_package *p = NULL;
	struct uci_element *e = NULL;
	struct uci_section *s = NULL;


	const char *value = NULL;

	struct uci_context *uci_ctx = NULL;
	uci_ctx = uci_alloc_context();
	if(NULL == uci_ctx){
		ERR_SYSLOG("BFDLISTEN: failed in allocing context! Func=%s,Line=%d\n",__FUNCTION__, __LINE__);
		exit(1);
	}

	rc = uci_load(uci_ctx, BFD_CONFIG_FILE, &p);
    ERR_SYS_GOTO(rc, fail, "BFDLISTEN: failed in loading uci file bfd! Func=%s,Line=%d,Errno=%d\n",
               __FUNCTION__, __LINE__, rc);

	while(1){
	uci_foreach_element(&p->sections, e){
       		s = uci_to_section(e);
			if(NULL != (value = uci_lookup_option_string(uci_ctx, s, "type"))){
				if(strcmp(value, "2")){
					continue;
				}
				if(NULL != (value = uci_lookup_option_string(uci_ctx, s, "enabled"))){
					if(strcmp(value, "1")){
						continue;
					}
					if(NULL != (value = uci_lookup_option_string(uci_ctx, s, "interface"))){
						ovs_parse_state(s->e.name, value);
					}
				}
			}
	}

		sleep(3);
	}
fail:
	if(p){
		uci_unload(uci_ctx, p);
	}
	uci_free_context(uci_ctx);

	exit(1);
}

int main(){
	INFO_SYSLOG("BFDLISTEN: start, Func=%s\n", __FUNCTION__);
	pthread_t tcp_thread;
	pthread_t ovs_thread;

	//check /var/state/bfd status file, if not exist, create it
	FILE * fp = fopen(BFD_STATE_FILE, "a");
	if(fp == NULL){

		ERR_SYSLOG("BFDLISTEN : fopen %s error, Error: %s, Func=%s, Line=%d", BFD_STATE_FILE, strerror(errno), __FUNCTION__, __LINE__);
		return -1;
	}

	if(0 != pthread_create(&tcp_thread, NULL, tcp_get_state_op, NULL)){
		 ERR_SYSLOG("BFDLISTEN : create pthread fail! Error : %s, Func=%s, Line=%d\n", strerror(errno), __FUNCTION__, __LINE__);
	}

	if(0 != pthread_create(&tcp_thread, NULL, ovs_get_state_op, NULL)){
		 ERR_SYSLOG("BFDLISTEN : create pthread fail! Error : %s, Func=%sm Line=%d\n",strerror(errno),  __FUNCTION__, __LINE__);
	}

	 pthread_join(tcp_thread, NULL);
	 pthread_join(ovs_thread, NULL);
	 pclose(fp);

	return 0;
}

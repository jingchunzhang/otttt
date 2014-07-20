/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "common.h"
#include "global.h"
#include "vfs_so.h"
#include "myepoll.h"
#include "protocol.h"
#include "util.h"
#include "acl.h"
#include "list.h"
#include "global.h"
#include "vfs_init.h"
#include "vfs_task.h"
#include "common.h"

typedef struct {
	list_head_t alist;
	char fname[256];
	int fd;
	uint32_t hbtime;
	int nostandby; // 1: delay process 
} http_peer;

int vfs_http_log = -1;
static list_head_t activelist;  //”√¿¥ºÏ≤‚≥¨ ±

int svc_init() 
{
	char *logname = myconfig_get_value("log_server_logname");
	if (!logname)
		logname = "../log/http_log.log";

	char *cloglevel = myconfig_get_value("log_server_loglevel");
	int loglevel = LOG_DEBUG;
	if (cloglevel)
		loglevel = getloglevel(cloglevel);
	int logsize = myconfig_get_intval("log_server_logsize", 100);
	int logintval = myconfig_get_intval("log_server_logtime", 3600);
	int lognum = myconfig_get_intval("log_server_lognum", 10);
	vfs_http_log = registerlog(logname, loglevel, logsize, logintval, lognum);
	if (vfs_http_log < 0)
		return -1;
	INIT_LIST_HEAD(&activelist);
	LOG(vfs_http_log, LOG_DEBUG, "svc_init init log ok!\n");
	return 0;
}

int svc_initconn(int fd) 
{
	LOG(vfs_http_log, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		curcon->user = malloc(sizeof(http_peer));
	if (curcon->user == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "malloc err %m\n");
		return RET_CLOSE_MALLOC;
	}
	memset(curcon->user, 0, sizeof(http_peer));
	http_peer * peer = (http_peer *)curcon->user;
	peer->hbtime = time(NULL);
	peer->fd = fd;
	INIT_LIST_HEAD(&(peer->alist));
	list_move_tail(&(peer->alist), &activelist);
	LOG(vfs_http_log, LOG_DEBUG, "a new fd[%d] init ok!\n", fd);
	return 0;
}

static int check_request(int fd, char* data, int len) 
{
	if(len < 14)
		return 0;

	struct conn *c = &acon[fd];
	http_peer *peer = (http_peer *) c->user;
	if(!strncmp(data, "GET /", 5)) {
		char* p;
		if((p = strstr(data + 5, "\r\n\r\n")) != NULL) {
			char* q;
			int len;
			if((q = strstr(data + 5, " HTTP/")) != NULL) {
				len = q - data - 5;
				if(len < 1023) {
					strncpy(peer->fname, data + 5, len);
					char *t = strchr(peer->fname, '?');
					if (t)
						*t = 0x0;
					return p - data + 4;
				}
			}
			return -2;	
		}
		else
			return 0;
	}
	else
		return -1;
}

static int getsize(char *fname)
{
	FILE *fp = fopen(fname, "r");
	if (fp == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "open %s err %m\n", fname);
		return -1;
	}

	char buf[64] = {0x0};
	fgets(buf, sizeof(buf), fp);
	fclose(fp);
	return atol(buf);
}

static int handle_request(int cfd) 
{
	char httpheader[256] = {0};
	char filename[128] = {0};
	int fd;
	struct stat st;
	
	struct conn *c = &acon[cfd];
	http_peer *peer = (http_peer *) c->user;
	sprintf(filename, "%s/%s", g_config.docroot, peer->fname);
	LOG(vfs_http_log, LOG_NORMAL, "file = %s\n", filename);
	
	fd = open(filename, O_RDONLY);
	if(fd > 0) {
		strcat(filename, ".size");
		st.st_size = getsize(filename);
		if (st.st_size <= 0)
			fstat(fd, &st);
		sprintf(httpheader, "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %u\r\n\r\n", (unsigned)st.st_size);
		
	}
	if(fd > 0)
	{
		set_client_data(cfd, httpheader, strlen(httpheader));
		set_client_fd(cfd, fd, 0, (uint32_t)st.st_size);
		return 0;
	}
	return -1;
}

static int get_file_from_src(char *fname, char *data, int len)
{
	char *p = strstr(data, "srcip: ");
	if (p == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "fname[%s] no srcip!\n", fname);
		return -1;
	}
	p += 7;
	char *e = strchr(p, '\r');
	if (e == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "fname[%s] srcip error!\n", fname);
		return -1;
	}
	*e = 0x0;

	char srcip[16] = {0x0};
	snprintf(srcip, sizeof(srcip), "%s", p);
	*e = '\r';

	t_vfs_tasklist *task0 = NULL;
	if (vfs_get_task(&task0, TASK_HOME))
	{
		LOG(vfs_http_log, LOG_ERROR, "fname[%s:%s] do_newtask ERROR!\n", fname, srcip);
		return -1;
	}
	t_vfs_taskinfo *task = &(task0->task);
	memset(&(task->base), 0, sizeof(task->base));
	snprintf(task->base.srcip, sizeof(task->base.srcip), "%s", srcip);
	char *t = strchr(data, '?');
	if (t == NULL)
		strncpy(task->base.data, data, len);
	else
	{
		strncpy(task->base.data, data, t - data);
		t = strstr(t, "\r\n");
		t += 2;
		strcat(task->base.data, "\r\n");
		char tmp = *(data + len);
		*(data + len) = 0x0;
		strcat(task->base.data, t);
		*(data + len) = tmp;
	}
	strcpy(task->base.filename, fname);
	add_task_to_alltask(task0);
	vfs_set_task(task0, TASK_WAIT);
	LOG(vfs_http_log, LOG_NORMAL, "fname[%s:%s] do_newtask ok!\n", fname, srcip);
	return 0;
}

static int check_req(int fd)
{
	char *data;
	size_t datalen;
	if (get_client_data(fd, &data, &datalen))
	{
		LOG(vfs_http_log, LOG_DEBUG, "fd[%d] no data!\n", fd);
		return RECV_ADD_EPOLLIN;  /*no suffic data, need to get data more */
	}
	int clen = check_request(fd, data, datalen);
	if (clen < 0)
	{
		LOG(vfs_http_log, LOG_DEBUG, "fd[%d] data error ,not http!\n", fd);
		return RECV_CLOSE;
	}
	if (clen == 0)
	{
		LOG(vfs_http_log, LOG_DEBUG, "fd[%d] data not suffic!\n", fd);
		return RECV_ADD_EPOLLIN;
	}
	int ret = handle_request(fd);
	consume_client_data(fd, clen);
	if (ret == 0)
		return RECV_SEND;
	else
	{
		struct conn *curcon = &acon[fd];
		http_peer *peer = (http_peer *) curcon->user;
		if (check_task_from_alltask(peer->fname))
			if (get_file_from_src(peer->fname, data, clen))
				return RECV_CLOSE;
		peer->nostandby = 1;
		peer->hbtime = time(NULL);
		return SEND_SUSPEND;
	}
}

int svc_recv(int fd) 
{
	return check_req(fd);
}

int svc_send(int fd)
{
	struct conn *curcon = &acon[fd];
	http_peer *peer = (http_peer *) curcon->user;
	peer->hbtime = time(NULL);
	return SEND_ADD_EPOLLIN;
}

void svc_timeout()
{
	time_t now = time(NULL);
	http_peer *peer = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(peer, l, &activelist, alist)
	{
		if (peer == NULL)
			continue;   /*bugs */
		if (peer->nostandby)
		{
			if (handle_request(peer->fd) == 0)
			{
				peer->nostandby = 0;
				modify_fd_event(peer->fd, EPOLLOUT);
			}
			else
			{
				if (now - peer->hbtime > g_config.timeout)
					do_close(peer->fd);
			}
		}
	}
}

void svc_finiconn(int fd)
{
	LOG(vfs_http_log, LOG_DEBUG, "close %d\n", fd);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		return;
	http_peer *peer = (http_peer *) curcon->user;
	list_del_init(&(peer->alist));
}

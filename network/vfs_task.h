/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _VFS_TASK_H_
#define _VFS_TASK_H_


#include "list.h"
#include "vfs_init.h"
#include "nm_app_vfs.h"
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#define SYNCIP 256
#define DOMAIN_PREFIX "fcs"

enum {OPER_IDLE, OPER_GET_REQ, OPER_GET_RSP, OPER_PUT, SYNC_2_GROUP};

enum {TASK_DELAY = 0, TASK_WAIT, TASK_WAIT_SYNC, TASK_WAIT_SYNC_IP, TASK_WAIT_TMP, TASK_Q_SYNC_DIR, TASK_Q_SYNC_DIR_TMP, TASK_RUN, TASK_FIN, TASK_CLEAN, TASK_HOME, TASK_SEND, TASK_RECV, TASK_SYNC_VOSS, TASK_Q_SYNC_DIR_REQ, TASK_Q_SYNC_DIR_RSP, TASK_UNKNOWN}; /*任务队列*/  /*add TASK_Q_SYNC_DIR_REQ TASK_Q_SYNC_DIR_RSP for thread sync dir */

enum {TASK_ADDFILE = '0', TASK_DELFILE, TASK_LINKFILE, TASK_SYNCDIR};  /* 任务类型 */

enum {OVER_UNKNOWN = 0, OVER_OK, OVER_E_MD5, OVER_PEERERR, TASK_EXIST, OVER_PEERCLOSE, OVER_UNLINK, OVER_TIMEOUT, OVER_MALLOC, OVER_SRC_DOMAIN_ERR, OVER_SRC_IP_OFFLINE, OVER_E_OPEN_SRCFILE, OVER_E_OPEN_DSTFILE, OVER_E_IP, OVER_E_TYPE, OVER_SEND_LEN, OVER_TOO_MANY_TRY, OVER_DISK_ERR, OVER_LAST};  /*任务结束时的状态*/

enum {GET_TASK_ERR = -1, GET_TASK_OK, GET_TASK_NOTHING};  /*从指定队列取任务的结果*/

enum {TASK_DST = 0, TASK_SOURCE, TASK_SRC_NOSYNC, TASK_SYNC_ISDIR, TASK_SYNC_VOSS_FILE}; /*本次任务是否需要相同组机器同步 */

extern const char *task_status[TASK_UNKNOWN]; 
extern const char *over_status[OVER_LAST]; 
typedef struct {
	char filename[256];
	char tmpfile[256];
	char data[1024];
	char srcip[16];
	off_t fsize;
	off_t getlen;
	time_t stime;
	short overstatus;
	int8_t retry;     /*任务执行失败时，根据配置是否执行重新发起任务，已经重试次数，不能超过设定重试次数*/
}t_task_base;

typedef struct {
	t_task_base base;
	void *user;
}t_vfs_taskinfo;

typedef struct {
	t_vfs_taskinfo task;
	list_head_t llist;
	list_head_t hlist;
	list_head_t userlist;
	uint32_t upip;
	uint8_t status;
	uint8_t bk[3];
} t_vfs_tasklist;

typedef struct {
	char domain[64];
	int d1;
	int d2;
	uint32_t ip;
	time_t task_stime; /*for time_out */
	time_t starttime; /*同步开始时间点 对CS来说，填写 目录上次同步的时间戳，对FCS来说，填写 0*/ 
	time_t endtime; /*同步结束时间点，对CS来说 填写 0，对FCS来说，填写FCS最近一次启动的时间戳 */ 
	char type;  /*ADDFILE, DELFILE 删除同步，只有CS才会发出请求，FCS不会发出删除同步*/
} t_vfs_sync_task;

typedef struct {
	uint8_t trycount;
	t_vfs_sync_task sync_task;
	list_head_t list;
} t_vfs_sync_list;

typedef void (*timeout_task)(t_vfs_tasklist *task);

int vfs_get_task(t_vfs_tasklist **task, int status);

int vfs_set_task(t_vfs_tasklist *task, int status);

int init_task_info();

int add_task_to_alltask(t_vfs_tasklist *task);

int check_task_from_alltask(char *fname);

int get_task_from_alltask(t_vfs_tasklist **task, t_task_base *base);

int get_timeout_task_from_alltask(int timeout, timeout_task cb);

int scan_some_status_task(int status, timeout_task cb);

int get_task_count(int status);

void do_timeout_task();

void report_2_nm();

#endif

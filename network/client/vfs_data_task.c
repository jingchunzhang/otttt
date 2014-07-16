/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "vfs_data_task.h"
#include "vfs_data.h"
#include "vfs_task.h"
#include "common.h"
#include "global.h"
#include "vfs_so.h"
#include "myepoll.h"
#include "protocol.h"
#include "util.h"
#include "acl.h"

extern int vfs_sig_log;
extern t_ip_info self_ipinfo;

void dump_task_info (char *from, int line, t_task_base *task)
{
}

int do_prepare_recvfile(int fd, off_t fsize)
{
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	if (peer->sock_stat != RECV_HEAD_ING)
	{
		LOG(vfs_sig_log, LOG_ERROR, "fd[%d] status not recv [%x]\n", fd, peer->sock_stat);
		return RECV_CLOSE;
	}
	t_vfs_tasklist *task0 = peer->recvtask;
	t_task_base *base = &(task0->task.base);
	base->fsize = fsize;

	if (peer->local_in_fd > 0)
		close(peer->local_in_fd);
	if (open_tmp_localfile_4_write(base, &(peer->local_in_fd), fsize) != LOCALFILE_OK) 
	{
		LOG(vfs_sig_log, LOG_ERROR, "fd[%d] open_tmp_localfile_4_write error %s\n", fd);
		task0->task.base.overstatus = OVER_E_OPEN_DSTFILE;
		vfs_set_task(task0, TASK_FIN);
		return RECV_CLOSE;
	}
	else
		peer->sock_stat = RECV_BODY_ING;
	return RECV_ADD_EPOLLIN;
}


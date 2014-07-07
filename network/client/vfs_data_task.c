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
	if (peer->sock_stat != PREPARE_RECVFILE)
	{
		LOG(vfs_sig_log, LOG_ERROR, "fd[%d] status not recv [%x] file [%s]\n", fd, peer->sock_stat, base->filename);
		return RECV_CLOSE;
	}
	t_vfs_tasklist *task0 = peer->recvtask;
	t_task_base *base0 = &(task0->task.base);

	memcpy(base0, base, sizeof(t_task_base));
	if (peer->local_in_fd > 0)
		close(peer->local_in_fd);
	if (open_tmp_localfile_4_write(base0, &(peer->local_in_fd)) != LOCALFILE_OK) 
	{
		LOG(vfs_sig_log, LOG_ERROR, "fd[%d] file [%s] open file err %m\n", fd, base->filename);
		if (peer->recvtask)
		{
			base0->overstatus = OVER_E_OPEN_DSTFILE;
			vfs_set_task(peer->recvtask, TASK_FIN);
			peer->recvtask = NULL;
		}
		return RECV_CLOSE;
	}
	else
	{
		peer->sock_stat = RECVFILEING;
	}
	return RECV_ADD_EPOLLIN;
}


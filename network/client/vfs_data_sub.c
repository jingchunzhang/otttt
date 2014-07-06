/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "c_api.h"
int active_send(vfs_cs_peer *peer, t_vfs_sig_head *h, t_vfs_sig_body *b)
{
	LOG(vfs_sig_log, LOG_DEBUG, "send %d cmdid %x\n", peer->fd, h->cmdid);
	char obuf[2048] = {0x0};
	int n = 0;
	peer->hbtime = time(NULL);
	n = create_sig_msg(h->cmdid, h->status, b, obuf, h->bodylen);
	set_client_data(peer->fd, obuf, n);
	modify_fd_event(peer->fd, EPOLLOUT);
	return 0;
}

static int do_req(int fd, off_t fsize)
{

	return do_prepare_recvfile(fd, fsize);
}

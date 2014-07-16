/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "c_api.h"
int active_send(int fd, char *data)
{
	LOG(vfs_sig_log, LOG_DEBUG, "send %d cmdid %s\n", fd, data);
	set_client_data(fd, data, strlen(data));
	modify_fd_event(fd, EPOLLOUT);
	return 0;
}

static int do_req(int fd, off_t fsize)
{
	return do_prepare_recvfile(fd, fsize);
}

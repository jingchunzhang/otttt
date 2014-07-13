/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

/*
 *base�ļ�����ѯ����������Ϣ��������ػ���״̬������```
 *Tracker ��Ŀ���٣�����һ����̬����
 *CS��FCS��Ŀ�϶࣬����hash����
 *CS FCS ip��Ϣ����uint32_t �洢�����ڴ洢�Ͳ���
 */
volatile extern int maintain ;		//1-ά������ 0 -����ʹ��
extern t_vfs_up_proxy g_proxy;

static inline int isDigit(const char *ptr) 
{
	return isdigit(*(unsigned char *)ptr);
}

static int active_connect(char *ip, int port)
{
	int fd = createsocket(ip, port);
	if (fd < 0)
	{
		LOG(vfs_sig_log, LOG_ERROR, "connect %s:%d err %m\n", ip, port);
		return -1;
	}
	if (svc_initconn(fd))
	{
		LOG(vfs_sig_log, LOG_ERROR, "svc_initconn err %m\n");
		close(fd);
		return -1;
	}
	add_fd_2_efd(fd);
	LOG(vfs_sig_log, LOG_NORMAL, "connect %s:%d\n", ip, port);
	return fd;
}

/*find �������Ϣ */
int find_ip_stat(uint32_t ip, vfs_cs_peer **dpeer, int mode, int status)
{
	list_head_t *hashlist = &(online_list[ALLMASK&ip]);
	vfs_cs_peer *peer = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(peer, l, hashlist, hlist)
	{
		if (peer->ip == ip)
		{
			if (mode == peer->mode && status == peer->sock_stat)
			{
				*dpeer = peer;
				return 0;
			}
		}
	}
	return -1;
}

void check_task()
{
	t_vfs_tasklist *task = NULL;
	int ret = 0;
	while (1)
	{
		ret = vfs_get_task(&task, TASK_WAIT_TMP);
		if (ret != GET_TASK_OK)
		{
			LOG(vfs_sig_log, LOG_TRACE, "vfs_get_task get notihng %d\n", ret);
			break;
		}
		vfs_set_task(task, TASK_WAIT);
	}

	uint16_t once_run = 0;
	while (1)
	{
		once_run++;
		if (once_run >= g_config.cs_max_task_run_once)
			return;
		ret = vfs_get_task(&task, TASK_WAIT);
		if (ret != GET_TASK_OK)
		{
			LOG(vfs_sig_log, LOG_TRACE, "vfs_get_task get notihng %d\n", ret);
			ret = vfs_get_task(&task, TASK_Q_SYNC_DIR);
			if (ret != GET_TASK_OK)
				return ;
			else
				LOG(vfs_sig_log, LOG_DEBUG, "Process TASK_Q_SYNC_DIR!\n");
		}
		t_task_base *base = &(task->task.base);
		if (base->retry > g_config.retry)
		{
			real_rm_file(base->tmpfile);
			base->overstatus = OVER_TOO_MANY_TRY;
			vfs_set_task(task, TASK_FIN);  
			continue;
		}
		if (g_config.vfs_test)
		{
			base->overstatus = OVER_OK;
			vfs_set_task(task, TASK_FIN);
			continue;
		}
		int fd = active_connect(base->srcip, base->srcport);
		active_send(fd, base->data);
	}
}



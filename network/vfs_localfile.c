/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "vfs_localfile.h"
#include "vfs_time_stamp.h"
#include "vfs_file_filter.h"
#include "vfs_timer.h"
#include "common.h"
#include "vfs_del_file.h"
#include <sys/vfs.h>
#include <utime.h>
#include <libgen.h>
extern int glogfd;
extern t_g_config g_config;
extern t_ip_info self_ipinfo;

/*
 *本文件有优化余地
 */

static uint32_t g_index = 0;

static void get_tmpstr(char *d, int len)
{
	char t[16] = {0x0};
	get_strtime(t);
	snprintf(d, len, "_%s_%d", t, g_index++);
}

static int createdir(char *file)
{
	char *pos = file;
	int len = 1;
	while (1)
	{
		pos = strchr(file + len, '/');
		if (!pos)
			break;
		*pos = 0;
		if (access(file, F_OK) != 0)
		{
			if (mkdir(file, 0755) < 0)
			{
				LOG(glogfd, LOG_ERROR, "mkdir [%s] [%m][%d]!\n", file, errno);
				return -1;
			}
			if (chown(file, g_config.dir_uid, g_config.dir_gid))
				LOG(glogfd, LOG_ERROR, "chown [%s] %d %d [%m][%d]!\n", file, g_config.dir_uid, g_config.dir_gid, errno);
		}

		*pos = '/';
		len = pos - file + 1;
	}
	if (chown(file, g_config.dir_uid, g_config.dir_gid))
		LOG(glogfd, LOG_ERROR, "chown [%s] %d %d [%m][%d]!\n", file, g_config.dir_uid, g_config.dir_gid, errno);
	return 0;
}

static int get_localdir(t_task_base *task, char *outdir)
{
	char *datadir = "/diska";
	sprintf(outdir, "%s/", datadir);
	return 0;
}

void real_rm_file(char *file)
{
	int ret = unlink(file);
	if (ret && errno != ENOENT)
	{
		LOG(glogfd, LOG_ERROR, "file [%s] unlink err %m\n", file);
		return ;
	}
	LOG(glogfd, LOG_NORMAL, "file [%s] be unlink\n", file);
}

int delete_localfile(t_task_base *task)
{
	char outdir[256] = {0x0};
	if (get_localdir(task, outdir))
		return LOCALFILE_DIR_E;
	char *t = strrchr(task->filename, '/');
	if (t == NULL)
		return LOCALFILE_DIR_E;
	t++;
	strcat(outdir, t);
	char rmfile[256] = {0x0};
	snprintf(rmfile, sizeof(rmfile), "%s.tmp4rm", outdir);
	if (rename(outdir, rmfile))
	{
		LOG(glogfd, LOG_ERROR, "file [%s] rename [%s]err %m\n", outdir, rmfile);
		return LOCALFILE_RENAME_E;
	}
	t_vfs_timer vfs_timer;
	memset(&vfs_timer, 0, sizeof(vfs_timer));
	snprintf(vfs_timer.args, sizeof(vfs_timer.args), "%s", rmfile);
	vfs_timer.span_time = g_config.real_rm_time;
	vfs_timer.cb = real_rm_file;
	if (add_to_delay_task(&vfs_timer))
		LOG(glogfd, LOG_ERROR, "file [%s] rename [%s]add delay task err %m\n", outdir, rmfile);
	return LOCALFILE_OK;
}

int check_localfile_md5(t_task_base *task, int type)
{
	char outdir[256] = {0x0};
	if (get_localdir(task, outdir))
		return LOCALFILE_DIR_E;
	char *t = strrchr(task->filename, '/');
	if (t == NULL)
		return LOCALFILE_DIR_E;
	t++;
	if (type == VIDEOFILE)
		strcat(outdir, t);
	if (type == VIDEOTMP)  /*tmpfile = "." + file + ".vfs"; */
	{
		memset(outdir, 0, sizeof(outdir));
		snprintf(outdir, sizeof(outdir), "%s", task->tmpfile);
		LOG(glogfd, LOG_DEBUG, "file [%s:%s]\n", outdir, task->tmpfile);
	}
	struct utimbuf c_time;
	c_time.modtime = c_time.actime;
	utime(outdir, &c_time);
	chmod(outdir, 0644);
	return LOCALFILE_OK;
}

int open_localfile_4_read(t_task_base *task, int *fd)
{
	char outdir[256] = {0x0};
	if (get_localdir(task, outdir))
		return LOCALFILE_DIR_E;
	char *t = strrchr(task->filename, '/');
	if (t == NULL)
		return LOCALFILE_DIR_E;
	t++;
	strcat(outdir, t);
	*fd = open(outdir, O_RDONLY);
	if (*fd < 0)
	{
		LOG(glogfd, LOG_ERROR, "open %s err %m\n", outdir);
		return LOCALFILE_OPEN_E;
	}
	return LOCALFILE_OK;
}

int open_tmp_localfile_4_write(t_task_base *task, int *fd, off_t fsize)
{
	char outdir[256] = {0x0};
	snprintf(outdir, sizeof(outdir), "%s/%s", g_config.docroot, task->filename);
	*fd = open(outdir, O_CREAT | O_RDWR | O_LARGEFILE, 0644);
	if (*fd < 0)
	{
		LOG(glogfd, LOG_ERROR, "open %s err %m\n", outdir);
		return LOCALFILE_OPEN_E;
	}

	strcat(outdir, ".size");
	int tfd = open(outdir, O_CREAT | O_RDWR | O_LARGEFILE, 0644);
	if(tfd < 0)
	{
		LOG(glogfd, LOG_ERROR, "open %s err %m\n", outdir);
		close(*fd);
		return LOCALFILE_OPEN_E;
	}

	char buf[64] = {0x0};
	snprintf(buf, sizeof(buf), "%ld\n", fsize);
	write(tfd, buf, strlen(buf));
	close(tfd);
	return LOCALFILE_OK;
}

int close_tmp_check_mv(t_task_base *task, int fd)
{
	if (fd < 0)
	{
		LOG(glogfd, LOG_ERROR, "fd < 0 %d %s\n", fd, task->filename);
		return LOCALFILE_OPEN_E;
	}
	close(fd);
	return LOCALFILE_OK;
}

int check_disk_space(t_task_base *base)
{
	char path[256] = {0x0};
	if (get_localdir(base, path))
	{
		LOG(glogfd, LOG_ERROR, "get local dir err %s\n", base->filename);
		return LOCALFILE_DIR_E;
	}
	if (access(path, F_OK) != 0)
	{
		LOG(glogfd, LOG_DEBUG, "dir %s not exist, try create !\n", path);
		if (createdir(path))
		{
			LOG(glogfd, LOG_ERROR, "dir %s create %m!\n", path);
			return LOCALFILE_DIR_E;
		}
	}
	struct statfs sfs;
	if (statfs(path, &sfs))
	{
		LOG(glogfd, LOG_ERROR, "statfs %s err %m\n", path);
		return DISK_ERR;
	}

	uint64_t diskfree = sfs.f_bfree * sfs.f_bsize;
	if (g_config.mindiskfree > diskfree)
	{
		LOG(glogfd, LOG_ERROR, "statfs %s space not enough %llu:%llu\n", path, diskfree, g_config.mindiskfree);
		return DISK_SPACE_TOO_SMALL;
	}
	return DISK_OK;
}

void localfile_link_task(t_task_base *task)
{
}

int get_localfile_stat(t_task_base *task)
{
	char outdir[256] = {0x0};
	if (get_localdir(task, outdir))
		return LOCALFILE_DIR_E;
	char *t = strrchr(task->filename, '/');
	if (t == NULL)
		return LOCALFILE_DIR_E;
	t++;
	strcat(outdir, t);
	struct stat filestat;
	if (stat(outdir, &filestat))
	{
		LOG(glogfd, LOG_ERROR, "stat file %s err %m!\n", outdir);
		return LOCALFILE_DIR_E;
	}
	return LOCALFILE_OK;
}


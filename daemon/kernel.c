#include "kernel.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib.h>
#include <errno.h>

#include "helpers.h"
#include "log.h"

#include "xt_RTPENGINE.h"

#define PREFIX "/proc/rtpengine"

struct kernel_interface kernel;

static bool kernel_action_table(const char *action, unsigned int id) {
	char s[64];
	int saved_errno;
	int fd;
	int i;
	ssize_t ret;

	fd = open(PREFIX "/control", O_WRONLY | O_TRUNC);
	if (fd == -1)
		return false;
	i = snprintf(s, sizeof(s), "%s %u\n", action, id);
	if (i >= sizeof(s))
		goto fail;
	ret = write(fd, s, strlen(s));
	if (ret == -1)
		goto fail;
	close(fd);

	return true;

fail:
	saved_errno = errno;
	close(fd);
	errno = saved_errno;
	return false;
}

static bool kernel_create_table(unsigned int id) {
	return kernel_action_table("add", id);
}

static bool kernel_delete_table(unsigned int id) {
	return kernel_action_table("del", id);
}

static int kernel_open_table(unsigned int id) {
	char s[64];
	int saved_errno;
	int fd;
	struct rtpengine_command_noop cmd;
	ssize_t ret;

	sprintf(s, PREFIX "/%u/control", id);
	fd = open(s, O_RDWR | O_TRUNC);
	if (fd == -1)
		return -1;

	cmd.cmd = REMG_NOOP;

	cmd.noop = (struct rtpengine_noop_info) {
		.last_cmd = __REMG_LAST,
		.msg_size = {
			[REMG_NOOP] = sizeof(struct rtpengine_command_noop),
			[REMG_ADD_TARGET] = sizeof(struct rtpengine_command_add_target),
			[REMG_DEL_TARGET_STATS] = sizeof(struct rtpengine_command_del_target_stats),
			[REMG_ADD_DESTINATION] = sizeof(struct rtpengine_command_destination),
			[REMG_ADD_CALL] = sizeof(struct rtpengine_command_add_call),
			[REMG_DEL_CALL] = sizeof(struct rtpengine_command_del_call),
			[REMG_ADD_STREAM] = sizeof(struct rtpengine_command_add_stream),
			[REMG_DEL_STREAM] = sizeof(struct rtpengine_command_del_stream),
			[REMG_PACKET] = sizeof(struct rtpengine_command_packet),
			[REMG_GET_RESET_STATS] = sizeof(struct rtpengine_command_stats),
			[REMG_SEND_RTCP] = sizeof(struct rtpengine_command_send_packet),
			[REMG_INIT_PLAY_STREAMS] = sizeof(struct rtpengine_command_init_play_streams),
			[REMG_GET_PACKET_STREAM] = sizeof(struct rtpengine_command_get_packet_stream),
			[REMG_PLAY_STREAM_PACKET] = sizeof(struct rtpengine_command_play_stream_packet),
			[REMG_PLAY_STREAM] = sizeof(struct rtpengine_command_play_stream),
			[REMG_STOP_STREAM] = sizeof(struct rtpengine_command_stop_stream),
			[REMG_FREE_PACKET_STREAM] = sizeof(struct rtpengine_command_free_packet_stream),
		},
	};

	ret = write(fd, &cmd, sizeof(cmd));
	if (ret <= 0)
		goto fail;

	return fd;

fail:
	saved_errno = errno;
	close(fd);
	errno = saved_errno;
	return -1;
}

bool kernel_setup_table(unsigned int id) {
	if (kernel.is_wanted)
		abort();

	kernel.is_wanted = true;

	if (!kernel_delete_table(id) && errno != ENOENT) {
		ilog(LOG_ERR, "FAILED TO DELETE KERNEL TABLE %i (%s), KERNEL FORWARDING DISABLED",
				id, strerror(errno));
		return false;
	}
	if (!kernel_create_table(id)) {
		ilog(LOG_ERR, "FAILED TO CREATE KERNEL TABLE %i (%s), KERNEL FORWARDING DISABLED",
				id, strerror(errno));
		return false;
	}
	int fd = kernel_open_table(id);
	if (fd == -1) {
		ilog(LOG_ERR, "FAILED TO OPEN KERNEL TABLE %i (%s), KERNEL FORWARDING DISABLED",
				id, strerror(errno));
		return false;
	}

	kernel.fd = fd;
	kernel.table = id;
	kernel.is_open = true;

	return true;
}

void kernel_shutdown_table(void) {
	if (!kernel.is_open)
		return;
	// ignore errors
	close(kernel.fd);
	kernel_delete_table(kernel.table);
}


void kernel_add_stream(struct rtpengine_target_info *mti) {
	struct rtpengine_command_add_target cmd;
	ssize_t ret;

	if (!kernel.is_open)
		return;

	cmd.cmd = REMG_ADD_TARGET;
	cmd.target = *mti;

	ret = write(kernel.fd, &cmd, sizeof(cmd));
	if (ret == sizeof(cmd))
		return;

	ilog(LOG_ERROR, "Failed to push relay stream to kernel: %s", strerror(errno));
}

void kernel_add_destination(struct rtpengine_destination_info *mdi) {
	struct rtpengine_command_destination cmd;
	ssize_t ret;

	if (!kernel.is_open)
		return;

	cmd.cmd = REMG_ADD_DESTINATION;
	cmd.destination = *mdi;

	ret = write(kernel.fd, &cmd, sizeof(cmd));
	if (ret == sizeof(cmd))
		return;

	ilog(LOG_ERROR, "Failed to push relay stream destination to kernel: %s", strerror(errno));
}


bool kernel_del_stream_stats(struct rtpengine_command_del_target_stats *cmd) {
	ssize_t ret;

	if (!kernel.is_open)
		return false;

	cmd->cmd = REMG_DEL_TARGET_STATS;

	ret = read(kernel.fd, cmd, sizeof(*cmd));
	if (ret == sizeof(*cmd))
		return true;

	ilog(LOG_ERROR, "Failed to delete relay stream from kernel: %s", strerror(errno));
	return false;
}

kernel_slist *kernel_get_list(void) {
	char s[64];
	int fd;
	struct rtpengine_list_entry *buf;
	kernel_slist *li = NULL;
	ssize_t ret;

	if (!kernel.is_open)
		return NULL;

	sprintf(s, PREFIX "/%u/blist", kernel.table);
	fd = open(s, O_RDONLY);
	if (fd == -1)
		return NULL;


	for (;;) {
		buf = g_slice_alloc(sizeof(*buf));
		ret = read(fd, buf, sizeof(*buf));
		if (ret != sizeof(*buf))
			break;
		li = t_slist_prepend(li, buf);
	}

	g_slice_free1(sizeof(*buf), buf);
	close(fd);

	return li;
}

unsigned int kernel_add_call(const char *id) {
	struct rtpengine_command_add_call cmd;
	ssize_t ret;

	if (!kernel.is_open)
		return UNINIT_IDX;

	cmd.cmd = REMG_ADD_CALL;
	snprintf(cmd.call.call_id, sizeof(cmd.call.call_id), "%s", id);

	ret = read(kernel.fd, &cmd, sizeof(cmd));
	if (ret != sizeof(cmd))
		return UNINIT_IDX;
	return cmd.call.call_idx;
}

void kernel_del_call(unsigned int idx) {
	struct rtpengine_command_del_call cmd;
	ssize_t ret;

	if (!kernel.is_open)
		return;

	cmd.cmd = REMG_DEL_CALL;
	cmd.call_idx = idx;

	ret = write(kernel.fd, &cmd, sizeof(cmd));
	if (ret == sizeof(cmd))
		return;

	ilog(LOG_ERROR, "Failed to delete intercept call from kernel: %s", strerror(errno));
}

unsigned int kernel_add_intercept_stream(unsigned int call_idx, const char *id) {
	struct rtpengine_command_add_stream cmd;
	ssize_t ret;

	if (!kernel.is_open)
		return UNINIT_IDX;

	cmd.cmd = REMG_ADD_STREAM;
	cmd.stream.idx.call_idx = call_idx;
	snprintf(cmd.stream.stream_name, sizeof(cmd.stream.stream_name), "%s", id);

	ret = read(kernel.fd, &cmd, sizeof(cmd));
	if (ret != sizeof(cmd))
		return UNINIT_IDX;
	return cmd.stream.idx.stream_idx;
}

// cmd->local must be filled in
bool kernel_update_stats(struct rtpengine_command_stats *cmd) {
	ssize_t ret;

	if (!kernel.is_open)
		return false;

	cmd->cmd = REMG_GET_RESET_STATS;

	ret = read(kernel.fd, cmd, sizeof(*cmd));
	if (ret != sizeof(*cmd)) {
		ilog(LOG_ERROR, "Failed to get stream stats from kernel: %s", strerror(errno));
		return false;
	}

	return true;
}

void kernel_send_rtcp(struct rtpengine_send_packet_info *info, const char *buf, size_t len) {
	if (!kernel.is_open)
		return;

	size_t total_len = len + sizeof(struct rtpengine_command_send_packet);
	struct rtpengine_command_send_packet *cmd = alloca(total_len);
	cmd->cmd = REMG_SEND_RTCP;
	cmd->send_packet = *info;
	memcpy(&cmd->send_packet.data, buf, len);

	ssize_t ret = write(kernel.fd, cmd, total_len);

	if (ret != total_len) {
		if (ret == -1)
			ilog(LOG_ERR, "Failed to send RTCP via kernel interface: %s", strerror(errno));
		else
			ilog(LOG_ERR, "Failed to send RTCP via kernel interface (%zi != %zu)",
					ret, total_len);
	}
}

bool kernel_init_player(int num_media, int num_sessions) {
	if (num_media <= 0 || num_sessions <= 0)
		return false;
	if (!kernel.is_open)
		return false;

	struct rtpengine_command_init_play_streams ips = {
		.cmd = REMG_INIT_PLAY_STREAMS,
		.num_packet_streams = num_media,
		.num_play_streams = num_sessions,
	};
	ssize_t ret = write(kernel.fd, &ips, sizeof(ips));
	if (ret != sizeof(ips))
		return false;

	kernel.use_player = true;

	return true;
}

unsigned int kernel_get_packet_stream(void) {
	if (!kernel.use_player)
		return -1;

	struct rtpengine_command_get_packet_stream gps = { .cmd = REMG_GET_PACKET_STREAM };
	ssize_t ret = read(kernel.fd, &gps, sizeof(gps));
	if (ret != sizeof(gps))
		return -1;
	return gps.packet_stream_idx;
}

bool kernel_add_stream_packet(unsigned int idx, const char *buf, size_t len, unsigned long delay_ms,
		uint32_t ts, uint32_t dur)
{
	if (!kernel.use_player)
		return false;

	size_t total_len = len + sizeof(struct rtpengine_command_play_stream_packet);
	struct rtpengine_command_play_stream_packet *cmd = alloca(total_len);

	cmd->cmd = REMG_PLAY_STREAM_PACKET;
	cmd->play_stream_packet.packet_stream_idx = idx;
	cmd->play_stream_packet.delay_ms = delay_ms;
	cmd->play_stream_packet.delay_ts = ts;
	cmd->play_stream_packet.duration_ts = dur;

	memcpy(&cmd->play_stream_packet.data, buf, len);

	ssize_t ret = write(kernel.fd, cmd, total_len);
	if (ret != total_len)
		return false;
	return true;
}

unsigned int kernel_start_stream_player(struct rtpengine_play_stream_info *info) {
	if (!kernel.use_player)
		return -1;

	struct rtpengine_command_play_stream ps = {
		.cmd = REMG_PLAY_STREAM,
		.info = *info,
	};
	ssize_t ret = read(kernel.fd, &ps, sizeof(ps));
	if (ret == sizeof(ps))
		return ps.play_idx;
	return -1;
}

bool kernel_stop_stream_player(unsigned int idx) {
	if (!kernel.use_player)
		return false;

	struct rtpengine_command_stop_stream ss = {
		.cmd = REMG_STOP_STREAM,
		.play_idx = idx,
	};
	ssize_t ret = write(kernel.fd, &ss, sizeof(ss));
	if (ret == sizeof(ss))
		return true;
	return false;
}

bool kernel_free_packet_stream(unsigned int idx) {
	if (!kernel.use_player)
		return false;

	struct rtpengine_command_free_packet_stream fps = {
		.cmd = REMG_FREE_PACKET_STREAM,
		.packet_stream_idx = idx,
	};
	ssize_t ret = write(kernel.fd, &fps, sizeof(fps));
	if (ret == sizeof(fps))
		return true;
	return false;
}

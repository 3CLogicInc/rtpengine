#include "kernel.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <glib.h>
#include <errno.h>

#include "xt_RTPENGINE.h"

#include "aux.h"
#include "log.h"




#define PREFIX "/proc/rtpengine"




struct kernel_interface kernel;





static int kernel_action_table(const char *action, unsigned int id) {
	char str[64];
	int saved_errno;
	int fd;
	int i;

	fd = open(PREFIX "/control", O_WRONLY | O_TRUNC);
	if (fd == -1)
		return -1;
	i = snprintf(str, sizeof(str), "%s %u\n", action, id);
	if (i >= sizeof(str))
		goto fail;
	i = write(fd, str, strlen(str));
	if (i == -1)
		goto fail;
	close(fd);

	return 0;

fail:
	saved_errno = errno;
	close(fd);
	errno = saved_errno;
	return -1;
}

static int kernel_create_table(unsigned int id) {
	return kernel_action_table("add", id);
}

static int kernel_delete_table(unsigned int id) {
	return kernel_action_table("del", id);
}

static int kernel_open_table(unsigned int id) {
	char str[64];
	int saved_errno;
	int fd;
	struct rtpengine_message_noop msg;
	int i;

	sprintf(str, PREFIX "/%u/control", id);
	fd = open(str, O_RDWR | O_TRUNC);
	if (fd == -1)
		return -1;

	ZERO(msg);
	msg.cmd.cmd = REMG_NOOP;
	msg.noop.noop_size = sizeof(msg);
	msg.noop.target_size = sizeof(struct rtpengine_message_target);
	msg.noop.destination_size = sizeof(struct rtpengine_message_destination);
	msg.noop.last_cmd = __REMG_LAST;
	i = write(fd, &msg, sizeof(msg));
	if (i <= 0)
		goto fail;

	return fd;

fail:
	saved_errno = errno;
	close(fd);
	errno = saved_errno;
	return -1;
}

int kernel_setup_table(unsigned int id) {
	if (kernel.is_wanted)
		abort();

	kernel.is_wanted = 1;

	if (kernel_delete_table(id) && errno != ENOENT) {
		ilog(LOG_ERR, "FAILED TO DELETE KERNEL TABLE %i (%s), KERNEL FORWARDING DISABLED",
				id, strerror(errno));
		return -1;
	}
	if (kernel_create_table(id)) {
		ilog(LOG_ERR, "FAILED TO CREATE KERNEL TABLE %i (%s), KERNEL FORWARDING DISABLED",
				id, strerror(errno));
		return -1;
	}
	int fd = kernel_open_table(id);
	if (fd == -1) {
		ilog(LOG_ERR, "FAILED TO OPEN KERNEL TABLE %i (%s), KERNEL FORWARDING DISABLED",
				id, strerror(errno));
		return -1;
	}

	kernel.fd = fd;
	kernel.table = id;
	kernel.is_open = 1;

	return 0;
}


int kernel_add_stream(struct rtpengine_target_info *mti) {
	struct rtpengine_message_target msg;
	ssize_t ret;

	if (!kernel.is_open)
		return -1;

	msg.cmd.cmd = REMG_ADD_TARGET;
	msg.target = *mti;

	// coverity[uninit_use_in_call : FALSE]
	ret = write(kernel.fd, &msg, sizeof(msg));
	if (ret > 0)
		return 0;

	ilog(LOG_ERROR, "Failed to push relay stream to kernel: %s", strerror(errno));
	return -1;
}

int kernel_add_destination(struct rtpengine_destination_info *mdi) {
	struct rtpengine_message_destination msg;
	ssize_t ret;

	if (!kernel.is_open)
		return -1;

	msg.cmd.cmd = REMG_ADD_DESTINATION;
	msg.destination = *mdi;

	// coverity[uninit_use_in_call : FALSE]
	ret = write(kernel.fd, &msg, sizeof(msg));
	if (ret > 0)
		return 0;

	ilog(LOG_ERROR, "Failed to push relay stream destination to kernel: %s", strerror(errno));
	return -1;
}


int kernel_del_stream(const struct re_address *a) {
	struct rtpengine_message_target msg;
	ssize_t ret;

	if (!kernel.is_open)
		return -1;

	ZERO(msg);
	msg.cmd.cmd = REMG_DEL_TARGET;
	msg.target.local = *a;

	ret = write(kernel.fd, &msg, sizeof(msg));
	if (ret > 0)
		return 0;

	ilog(LOG_ERROR, "Failed to delete relay stream from kernel: %s", strerror(errno));
	return -1;
}

GList *kernel_list() {
	char str[64];
	int fd;
	struct rtpengine_list_entry *buf;
	GList *li = NULL;
	ssize_t ret;

	if (!kernel.is_open)
		return NULL;

	sprintf(str, PREFIX "/%u/blist", kernel.table);
	fd = open(str, O_RDONLY);
	if (fd == -1)
		return NULL;


	for (;;) {
		buf = g_slice_alloc(sizeof(*buf));
		ret = read(fd, buf, sizeof(*buf));
		if (ret != sizeof(*buf))
			break;
		li = g_list_prepend(li, buf);
	}

	g_slice_free1(sizeof(*buf), buf);
	close(fd);

	return li;
}

unsigned int kernel_add_call(const char *id) {
	struct rtpengine_message_call msg;
	ssize_t ret;

	if (!kernel.is_open)
		return UNINIT_IDX;

	ZERO(msg);
	msg.cmd.cmd = REMG_ADD_CALL;
	snprintf(msg.call.call_id, sizeof(msg.call.call_id), "%s", id);

	ret = read(kernel.fd, &msg, sizeof(msg));
	if (ret != sizeof(msg))
		return UNINIT_IDX;
	return msg.call.call_idx;
}

int kernel_del_call(unsigned int idx) {
	struct rtpengine_message_call msg;
	ssize_t ret;

	if (!kernel.is_open)
		return -1;

	ZERO(msg);
	msg.cmd.cmd = REMG_DEL_CALL;
	msg.call.call_idx = idx;

	ret = write(kernel.fd, &msg, sizeof(msg));
	if (ret != sizeof(msg))
		return -1;
	return 0;
}

unsigned int kernel_add_intercept_stream(unsigned int call_idx, const char *id) {
	struct rtpengine_message_stream msg;
	ssize_t ret;

	if (!kernel.is_open)
		return UNINIT_IDX;

	ZERO(msg);
	msg.cmd.cmd = REMG_ADD_STREAM;
	msg.stream.call_idx = call_idx;
	snprintf(msg.stream.stream_name, sizeof(msg.stream.stream_name), "%s", id);

	ret = read(kernel.fd, &msg, sizeof(msg));
	if (ret != sizeof(msg))
		return UNINIT_IDX;
	return msg.stream.stream_idx;
}

int kernel_update_stats(const struct re_address *a, struct rtpengine_stats_info *out) {
	struct rtpengine_message_stats msg;
	ssize_t ret;

	if (!kernel.is_open)
		return -1;

	ZERO(msg);
	msg.cmd.cmd = REMG_GET_RESET_STATS;
	msg.stats.local = *a;

	ret = read(kernel.fd, &msg, sizeof(msg));
	if (ret <= 0) {
		ilog(LOG_ERROR, "Failed to get stream stats from kernel: %s", strerror(errno));
		return -1;
	}

	*out = msg.stats;

	return 0;
}

int kernel_send_rtcp(struct rtpengine_send_packet_info *info, const char *buf, size_t len) {
	if (!kernel.is_open)
		return -1;

	size_t total_len = len + sizeof(struct rtpengine_message_send_packet);
	struct rtpengine_message_send_packet *msg = alloca(total_len);
	ZERO(*msg);
	msg->cmd.cmd = REMG_SEND_RTCP;
	msg->send_packet = *info;
	memcpy(&msg->data, buf, len);

	ssize_t ret = write(kernel.fd, msg, total_len);

	if (ret != total_len) {
		if (ret == -1)
			ilog(LOG_ERR, "Failed to send RTCP via kernel interface: %s", strerror(errno));
		else
			ilog(LOG_ERR, "Failed to send RTCP via kernel interface (%zi != %zu)",
					ret, total_len);
		return -1;
	}
	ilog(LOG_ERR, "XXXXXXXXXXXXXXX sent RTCP via kernel");

	return 0;
}

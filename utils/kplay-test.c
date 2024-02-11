#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "../kernel-module/xt_RTPENGINE.h"

int main() {
	int fd = open("/proc/rtpengine/control", O_WRONLY);
	assert(fd >= 0);
	write(fd, "add 0\n", 6); // ignore errors
	close(fd);

	fd = open("/proc/rtpengine/0/control", O_RDWR);
	assert(fd >= 0);

	struct rtpengine_command_noop noop = { .cmd = REMG_NOOP };

	noop.cmd = REMG_NOOP;

	noop.noop = (struct rtpengine_noop_info) {
		.last_cmd = __REMG_LAST,
		.msg_size = {
			[REMG_NOOP] = sizeof(struct rtpengine_command_noop),
			[REMG_ADD_TARGET] = sizeof(struct rtpengine_command_add_target),
			[REMG_DEL_TARGET] = sizeof(struct rtpengine_command_del_target),
			[REMG_DEL_TARGET_STATS] = sizeof(struct rtpengine_command_del_target_stats),
			[REMG_ADD_DESTINATION] = sizeof(struct rtpengine_command_destination),
			[REMG_ADD_CALL] = sizeof(struct rtpengine_command_add_call),
			[REMG_DEL_CALL] = sizeof(struct rtpengine_command_del_call),
			[REMG_ADD_STREAM] = sizeof(struct rtpengine_command_add_stream),
			[REMG_DEL_STREAM] = sizeof(struct rtpengine_command_del_stream),
			[REMG_PACKET] = sizeof(struct rtpengine_command_packet),
			[REMG_GET_STATS] = sizeof(struct rtpengine_command_stats),
			[REMG_GET_RESET_STATS] = sizeof(struct rtpengine_command_stats),
			[REMG_SEND_RTCP] = sizeof(struct rtpengine_command_send_packet),
			[REMG_INIT_PLAY_STREAMS] = sizeof(struct rtpengine_command_init_play_streams),
			[REMG_GET_PLAY_STREAM] = sizeof(struct rtpengine_command_get_play_stream),
			[REMG_PLAY_STREAM_PACKET] = sizeof(struct rtpengine_command_play_stream_packet),
			[REMG_PLAY_STREAM] = sizeof(struct rtpengine_command_play_stream),
		},
	};

	int ret = write(fd, &noop, sizeof(noop));
	assert(ret == sizeof(noop));

	struct rtpengine_command_init_play_streams ips = { .cmd = REMG_INIT_PLAY_STREAMS, .num_streams = 1000 };
	ret = write(fd, &ips, sizeof(ips));
	assert(ret == sizeof(ips));

	struct rtpengine_command_get_play_stream gps = { .cmd = REMG_GET_PLAY_STREAM };
	ret = read(fd, &gps, sizeof(gps));
	assert(ret == sizeof(gps));

	struct {
		struct rtpengine_command_play_stream_packet psp;
		char data[160];
	} psp = {
		.psp = {
			.cmd = REMG_PLAY_STREAM_PACKET,
			.play_stream_packet = {
				.stream_idx = gps.stream_idx,
			},
		},
	};

	for (unsigned int i = 0; i < 256; i++) {
		psp.psp.play_stream_packet.delay_ms = i * 20;
		memset(psp.data, i, sizeof(psp.data));
		ret = write(fd, &psp, sizeof(psp));
		assert(ret == sizeof(psp));
	}

	struct rtpengine_command_play_stream ps = { .cmd = REMG_PLAY_STREAM, .stream_idx = gps.stream_idx };
	ret = write(fd, &ps, sizeof(ps));
	assert(ret == sizeof(ps));

	return 0;
}

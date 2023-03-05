#include "codeclib.h"
#include "str.h"
#include <assert.h>

static void hexdump(const unsigned char *buf, int len) {
	for (int i = 0; i < len; i++)
		printf("%02x", buf[i]);
	printf("\n");
}

static int dec_cb(encoder_t *e, void *u1, void *u2) {
	char **expect = u1;
	int *expect_len = u2;
	assert(expect);
	assert(expect_len);
	assert(*expect);

	GString *buf = g_string_new("");
	int plen = 256;
	char payload[plen];
	str inout = { payload, plen };
	e->def->packetizer(&e->avpkt, buf, &inout, e);

	if (inout.len != *expect_len
			|| memcmp(inout.s, *expect, *expect_len))
	{
		printf(
				"packet content mismatch\n"
				"expected %i bytes, received %i bytes\n"
				"expected:\n",
				*expect_len, inout.len);
		hexdump((unsigned char *) *expect, *expect_len);
		printf("received:\n");
		hexdump((unsigned char *) inout.s, inout.len);
		exit(1);
	}

	*expect = NULL;
	*expect_len = 0;

	g_string_free(buf, TRUE);
	return 0;
}

static void do_test_amr_xx(const char *file, int line,
		char *fmtp_s, char *data_s, int data_len, char *expect_s, int expect_len,
		int bitrate, char *codec, int clockrate)
{
	printf("running test %s:%i\n", file, line);
	str codec_name;
	str_init(&codec_name, codec);
	codec_def_t *def = codec_find(&codec_name, MT_AUDIO);
	assert(def);
	if (!def->support_encoding || !def->support_decoding) {
		printf("AMR not fully supported - skipping test\n");
		exit(0);
	}
	const format_t fmt = { .clockrate = clockrate, .channels = 1, .format = 0 };
	str fmtp_str, *fmtp = NULL;
	char *fmtp_buf = NULL;
	if (fmtp_s) {
		fmtp_buf = strdup(fmtp_s);
		str_init(&fmtp_str, fmtp_buf);
		fmtp = &fmtp_str;
	}
	encoder_t *e = encoder_new();
	assert(e);
	format_t actual_fmt;
	int ret = encoder_config_fmtp(e, def, bitrate, 20, &fmt, &actual_fmt, fmtp);
	assert(actual_fmt.clockrate == clockrate);
	assert(actual_fmt.channels == 1);
	assert(actual_fmt.format == AV_SAMPLE_FMT_S16);

	AVFrame *frame = av_frame_alloc();
	assert(frame);
	frame->nb_samples = 20 * clockrate / 1000;
	frame->format = actual_fmt.format;
	frame->sample_rate = actual_fmt.clockrate;
	frame->channel_layout = av_get_default_channel_layout(actual_fmt.channels);
	ret = av_frame_get_buffer(frame, 0);
	assert(ret >= 0);

	assert(data_len == frame->nb_samples * 2);
	memcpy(frame->data[0], data_s, data_len);

	ret = encoder_input_data(e, frame, dec_cb, &expect_s, &expect_len);
	assert(!ret);
	assert(expect_s == NULL);

	encoder_free(e);
	free(fmtp_buf);

	printf("test ok: %s:%i\n", file, line);
}

static void do_test_amr_wb(const char *file, int line,
		char *fmtp_s, char *data_s, int data_len, char *expect_s, int expect_len,
		int bitrate)
{
	do_test_amr_xx(file, line, fmtp_s, data_s, data_len, expect_s, expect_len, bitrate,
			"AMR-WB", 16000);
}
static void do_test_amr_nb(const char *file, int line,
		char *fmtp_s, char *data_s, int data_len, char *expect_s, int expect_len,
		int bitrate)
{
	do_test_amr_xx(file, line, fmtp_s, data_s, data_len, expect_s, expect_len, bitrate,
			"AMR", 8000);
}

#define do_test_wb(in, out, fmt, bitrate) \
	do_test_amr_wb(__FILE__, __LINE__, fmt, in, sizeof(in)-1, out, sizeof(out)-1, bitrate)
#define do_test_nb(in, out, fmt, bitrate) \
	do_test_amr_nb(__FILE__, __LINE__, fmt, in, sizeof(in)-1, out, sizeof(out)-1, bitrate)

#define test_320_samples_16_bits \
			"\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01"
#define test_160_samples_16_bits \
			"\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01\x00\x00\x01\x00\x01\x00\x01\x01"

int main(void) {
	codeclib_init(0);

	do_test_wb(
			test_320_samples_16_bits,
			"\xf0\x44\xf1\x46\x18\x1d\xd1\x57\x23\x13\x42\xf0\x00\x0c\x50\x33\xdd\xff\x0b\x99\x89\x2c\x68\x52\xf8\xf8\xd9\x59\x16\xd7\x45\xe7\x01\xec\x1f\xfe\x5b\xc6\xf9\x01\xa4\xb5\xe0\x6c\x91\x41\xfe\x52\x2c\xce\x44\xbb\x5a\xdf\x76\x29\xf8\xdb\xca\x18\xd6\x50",
			"octet-align=1",
			23850);
	do_test_wb(
			test_320_samples_16_bits,
			"\xf0\x00\x44\xf1\x46\x18\x1d\xd1\x57\x23\x13\x42\xf0\x00\x0c\x50\x33\xdd\xff\x0b\x99\x89\x2c\x68\x52\xf8\xf8\xd9\x59\x16\xd7\x45\xe7\x01\xec\x1f\xfe\x5b\xc6\xf9\x01\xa4\xb5\xe0\x6c\x91\x41\xfe\x52\x2c\xce\x44\xbb\x5a\xdf\x76\x29\xf8\xdb\xca\x18\xd6\x50",
			"octet-align=1;interleaving=4",
			23850);
	do_test_wb(
			test_320_samples_16_bits,
			"\xf4\x7c\x51\x86\x07\x74\x55\xc8\xc4\xd0\xbc\x00\x03\x14\x0c\xf7\x7f\xc2\xe6\x62\x4b\x1a\x14\xbe\x3e\x36\x56\x45\xb5\xd1\x79\xc0\x7b\x07\xff\x96\xf1\xbe\x40\x69\x2d\x78\x1b\x24\x50\x7f\x94\x8b\x33\x91\x2e\xd6\xb7\xdd\x8a\x7e\x36\xf2\x86\x35\x94",
			NULL,
			23850);

	do_test_nb(
			test_160_samples_16_bits,
			"\xf0\x3c\x53\xff\x3a\xe8\x30\x41\xa5\xa8\xa4\x1d\x2f\xf2\x03\x60\x35\xc0\x00\x07\xc5\x53\xf4\xbc\x98\x00\x01\x14\x2f\xf0\x00\x0f\x70",
			"octet-align=1",
			12200);

	return 0;
}
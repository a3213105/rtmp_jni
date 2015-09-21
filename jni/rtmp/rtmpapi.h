#ifndef _RTMP_API_H_
#define _RTMP_API_H_

#include <librtmp/rtmp.h>
#include <librtmp/log.h>
#include <pthread.h>

#include <android/log.h>

#define JNI_CLASS_RTMP     "com/a3213105/publisher/RTMPSender"
#define RTMP_LOG_TAG "RTMP"
#define ALOG(level, TAG, ...)    ((void)__android_log_print(level, TAG, __VA_ARGS__))
#define ALOGE(...)  ALOG(ANDROID_LOG_ERROR, RTMP_LOG_TAG, __VA_ARGS__)

#define READ_FLAGS 1
#define WRITE_FLAGS 2

#define ERROR_UNKNOWN (-1)
#define ERROR_CONNECT (-2)
#define ERROR_SETUP   (-3)
#define ERROR_MEM     (-4)

//#define _DEBUG_NO_SEND_
typedef struct H264SPSPPS
{
    int sended;
    char* sps;
    int sps_size;
    char* pps;
    int pps_size;
} H264SPSPPS;

typedef struct AACHEADER
{
    char sended;
    char header[2];
    char type;
    int ph_len;
    int pb_len;
} AACHEADER;

typedef struct LibRTMPContext {
    RTMP rtmp;
    char *app;
    char *conn;
    char *subscribe;
    char *playpath;
    char *tcurl;
    char *flashver;
    char *swfurl;
    char *swfverify;
    char *pageurl;
    char *client_buffer_time;
    int live;
    char *filename;
    char* temp_filename;
    int buffer_size;
    int64_t first_pts;
    H264SPSPPS sps_pps;
    AACHEADER aac;
#ifdef _DEBUG_NO_SEND_
    int fd;
#endif // _DEBUG_NO_SEND_
} LibRTMPContext;

int rtmp_init(LibRTMPContext **ctx);
int rtmp_unit(LibRTMPContext **ctx);
int rtmp_seturl(LibRTMPContext *ctx,  const char* url);
int rtmp_open(LibRTMPContext *ctx,  int flags);
int rtmp_write_video_annexb(LibRTMPContext *ctx, const char *buf, int size, int64_t pts, int drop);
int rtmp_write_video_mp4(LibRTMPContext *ctx, const char *buf, int size, int64_t pts, int drop);
int rtmp_write_audio_raw(LibRTMPContext *ctx, const char *buf, int size, int64_t pts);
int rtmp_write_audio_adts(LibRTMPContext *ctx, const char *buf, int size, int64_t pts);
int rtmp_write(LibRTMPContext *ctx, const char *buf, int size);
int rtmp_read(LibRTMPContext *ctx, char *buf, int size);
int rtmp_close(LibRTMPContext *ctx);
int rtmp_read_pause(LibRTMPContext *ctx, int pause);
int64_t rtmp_read_seek(LibRTMPContext *ctx, int64_t timestamp);

#endif // _RTMP_API_H_

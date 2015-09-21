#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <jni.h>
#include "../rtmpapi.h"

#ifndef NELEM
# define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))
#endif

static JavaVM* g_jvm;

typedef struct data_pack
{
    char* buf;
    int len;
    int size;
    int dts;
    void* next;
} data_pack;

#define video_threshold 25
#define audio_threshold 44

typedef struct rtmp_fields_t
{
    pthread_mutex_t mutex;
    jclass clazz;
    LibRTMPContext	 * ctx;

    data_pack* video;
    data_pack* video_last;
    data_pack* video_unused;
    int video_size;
    int has_video;
    int is_annexb;

    data_pack* audio;
    data_pack* audio_last;
    data_pack* audio_unused;
    int audio_size;
    int has_audio;
    int is_adts;
} rtmp_fields_t;

static rtmp_fields_t g_clazz;

int open_rtmp(JNIEnv *env, jobject thiz)
{
    int ret = -1;
    pthread_mutex_lock(&g_clazz.mutex);
    if(g_clazz.ctx==NULL)
        goto LABEL_RETURN;
//open
    ret = rtmp_open(g_clazz.ctx, WRITE_FLAGS);

LABEL_RETURN:
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int clear_buffer_list()
{
    data_pack*tmp = NULL;

    while( (tmp=g_clazz.audio) != NULL) {
        g_clazz.audio = tmp->next;
        free(tmp->buf);
        free(tmp);
    }
    g_clazz.audio_last = NULL;

    while( (tmp=g_clazz.audio_unused) != NULL) {
        g_clazz.audio_unused = tmp->next;
        free(tmp->buf);
        free(tmp);
    }

    while( (tmp=g_clazz.video) != NULL) {
        g_clazz.video = tmp->next;
        free(tmp->buf);
        free(tmp);
    }
    g_clazz.video_last = NULL;

    while( (tmp=g_clazz.video_unused) != NULL) {
        g_clazz.video_unused = tmp->next;
        free(tmp->buf);
        free(tmp);
    }
}

int close_rtmp(JNIEnv *env, jobject thiz)
{
    int ret = -1;
    pthread_mutex_lock(&g_clazz.mutex);
    if(g_clazz.ctx==NULL)
        goto end;
//close
    rtmp_close(g_clazz.ctx);

end:
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int write_rtmp(JNIEnv *env, jobject thiz, jbyteArray data, jint len)
{
    int ret = -1;
    jbyte * buf = NULL;
    pthread_mutex_lock(&g_clazz.mutex);
    if(g_clazz.ctx==NULL)
        goto end;
//write
    buf = malloc(len);
    (*env)-> GetByteArrayRegion(env,data,0,len,buf);
    ret = rtmp_write(g_clazz.ctx, (char*)buf,len);
    free(buf);
end:
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int write_rtmp_video(JNIEnv *env, jobject thiz, jbyteArray data, jint len,jint dts)
{
    int ret = 0;
    pthread_mutex_lock(&g_clazz.mutex);
    if(g_clazz.video_size>video_threshold*2) {
            ret = -1;
            goto end;
    }

    if(g_clazz.video_unused==NULL) {
        g_clazz.video_unused = malloc(sizeof(data_pack));
        g_clazz.video_unused->buf = NULL;
        g_clazz.video_unused->dts = 0;
        g_clazz.video_unused->len = 0;
        g_clazz.video_unused->size = 0;
        g_clazz.video_unused->next = NULL;
        ALOGE("malloc new video data packet used size:%d",g_clazz.video_size);
    }

    data_pack* cur = g_clazz.video_unused;
    g_clazz.video_unused = g_clazz.video_unused->next;

    if(len > cur->size) {
            if(cur->buf!=NULL) {
                free(cur->buf);
            }
            cur->buf = malloc(len);
            cur->size = len;
    }
    cur->len = len;
    cur->dts = dts;

    (*env)-> GetByteArrayRegion(env,data,0,len,cur->buf);

    if(g_clazz.video_last!=NULL)
        g_clazz.video_last->next = cur;
    if(g_clazz.video==NULL)
        g_clazz.video = cur;

    cur->next = NULL;
    g_clazz.video_last = cur;
    g_clazz.video_size++;

end:
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int write_rtmp_audio(JNIEnv *env, jobject thiz, jbyteArray data, jint len,jint dts)
{
    int ret = 0;
    pthread_mutex_lock(&g_clazz.mutex);
    if(g_clazz.audio_size>audio_threshold*2){
            ret = -1;
            goto end;
    }

    if(g_clazz.audio_unused==NULL) {
        g_clazz.audio_unused = malloc(sizeof(data_pack));
        g_clazz.audio_unused->buf = NULL;
        g_clazz.audio_unused->dts = 0;
        g_clazz.audio_unused->len = 0;
        g_clazz.audio_unused->size = 0;
        g_clazz.audio_unused->next = NULL;
        ALOGE("malloc new audio data packet used size:%d",g_clazz.audio_size);
    }

    data_pack* cur = g_clazz.audio_unused;
    g_clazz.audio_unused = cur->next;

    if(len > cur->size) {
            if(cur->buf!=NULL) {
                free(cur->buf);
            }
            cur->buf = malloc(len);
            cur->size = len;
    }

    cur->len = len;
    cur->dts = dts;
    (*env)-> GetByteArrayRegion(env,data,0,len,cur->buf);

    if(g_clazz.audio_last!=NULL)
        g_clazz.audio_last->next = cur;
    if(g_clazz.audio==NULL)
        g_clazz.audio = cur;

    cur->next = NULL;
    g_clazz.audio_last = cur;

    g_clazz.audio_size++;
    //ALOGE("add new audio to list :%d",g_clazz.audio_size);

end:
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int use_video_data(data_pack* video)
{
    if(video!=g_clazz.video) {
            ALOGE("use_video_data video!=video_head");
    }
    g_clazz.video = g_clazz.video->next;
    if(g_clazz.video==NULL)
        g_clazz.video_last = NULL;
    g_clazz.video_size--;
}

int unuse_video_data(data_pack* video)
{
    if(g_clazz.video_unused != NULL)
        video->next = g_clazz.video_unused;
    else
        video->next = NULL;
    g_clazz.video_unused = video;
}

int use_audio_data(data_pack* audio)
{
    if(audio!=g_clazz.audio) {
            ALOGE("use_audio_data audio!=audio_head");
    }
    g_clazz.audio = g_clazz.audio->next;
    if(g_clazz.audio==NULL)
        g_clazz.audio_last = NULL;
    g_clazz.audio_size--;
}

int unuse_audio_data(data_pack* audio)
{
    if(g_clazz.audio_unused != NULL)
        audio->next = g_clazz.audio_unused;
    else
        audio->next = NULL;
    g_clazz.audio_unused = audio;
}

int peak_pack_data(data_pack** data) {
    int ret = 0;
    pthread_mutex_lock(&g_clazz.mutex);
    data_pack* video = g_clazz.video;
    data_pack* audio = g_clazz.audio;
    if ( video != NULL && audio != NULL ) {
            if(video->dts < audio->dts) {
                *data = video;
                ret = 1;
            } else {
                *data = audio;
                ret = 2;
            }
    } else if(video!=NULL && (!g_clazz.has_audio || (g_clazz.video_size > video_threshold && g_clazz.audio_size==0))) {
            *data = video;
            ret = 1;
    } else if(audio!=NULL && (!g_clazz.has_video || (g_clazz.video_size == 0 && g_clazz.audio_size > audio_threshold))) {
            *data = audio;
            ret = 2;
    } else {
            if(0) {
            ALOGE("peak_pack_data video_dts:%d, video_size:%d audio_dts:%d, audio_size:%d",
                    video!=NULL?video->dts:-1, g_clazz.video_size, audio!=NULL?audio->dts:-1, g_clazz.audio_size);
                video = g_clazz.video;
                char tmp[40960] = {0};
                tmp[0] = 0;
                int i = 0;
                sprintf(tmp+strlen(tmp), "Video [");
                while(video!=NULL) {
                    sprintf(tmp+strlen(tmp), "i:%d dts:%d ", i++, video->dts);
                    video = video->next;
                }
                 sprintf(tmp+strlen(tmp), "]\nAudio [");
                audio = g_clazz.audio;
                while(audio!=NULL) {
                    sprintf(tmp+strlen(tmp), "i:%d dts:%d ", i++, audio->dts);
                    audio = audio->next;
                }
                sprintf(tmp+strlen(tmp), "]");
                 ALOGE("%s",tmp);
            }
    }
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int recycle_data(int type, data_pack* data)
{
     pthread_mutex_lock(&g_clazz.mutex);
     if(type==1) {
            use_video_data(data);
            unuse_video_data(data);
     } else if(type==2) {
            use_audio_data(data);
            unuse_audio_data(data);
     }
     //ALOGE("recycle_data type:%d video_size %d, audio_size:%d",type, g_clazz.video_size,g_clazz.audio_size);
     pthread_mutex_unlock(&g_clazz.mutex);
     return 0;
}

int rtmp_loop()
{
    int ret = 0, sz = 0;
    int type = 0;
    data_pack* data = NULL;
    while (  (type = peak_pack_data(&data)) > 0 ) {
            if(type==1) {
            //video
            if(g_clazz.is_annexb)
                ret = rtmp_write_video_annexb(g_clazz.ctx,data->buf,data->len,data->dts, g_clazz.video_size>video_threshold?1:0);
            else
                ret = rtmp_write_video_mp4(g_clazz.ctx,data->buf,data->len,data->dts, g_clazz.video_size>video_threshold?1:0);
        } else if(type==2) {
        //audio
            if(g_clazz.is_adts)
                ret = rtmp_write_audio_adts(g_clazz.ctx,data->buf,data->len,data->dts);
            else
                ret = rtmp_write_audio_raw(g_clazz.ctx,data->buf,data->len,data->dts);
        } else {
                usleep(1234);
                if(g_clazz.video_size>0 &&g_clazz.audio_size>0)
                    ALOGE("data list is not empty, video:%d, audio:%d", g_clazz.video_size, g_clazz.audio_size);
                break;
        }
        recycle_data(type,data);
        if(ret>=0)
            sz += ret;
        else
            break;
    }
     return sz;
}

/*
int rtmp_loop()
{
    pthread_mutex_lock(&g_clazz.mutex);
    data_pack* video = g_clazz.video;
    data_pack* audio = g_clazz.audio;
    if ( video != NULL && audio != NULL ) {
       //     ALOGE("in rtmp_loop has packet to send");
        if(video->dts < audio->dts) {
            //send video
            use_video_data();
            pthread_mutex_unlock(&g_clazz.mutex);

            if(g_clazz.is_annexb)
                ret = rtmp_write_video_annexb(g_clazz.ctx,video->buf,video->len,video->dts, g_clazz.video_size>25?1:0);
            else
                ret = rtmp_write_video_mp4(g_clazz.ctx,video->buf,video->len,video->dts, g_clazz.video_size>25?1:0);
            ALOGE("in rtmp_loop send video 1 dts:%d, len:%d ret %d",video->dts, video->len, ret);

            pthread_mutex_lock(&g_clazz.mutex);
            unuse_video_data(video);

        } else {
            //send audio;
            use_audio_data();
            pthread_mutex_unlock(&g_clazz.mutex);

            if(g_clazz.is_adts)
                ret = rtmp_write_audio_adts(g_clazz.ctx,audio->buf,audio->len,audio->dts);
            else
                ret = rtmp_write_audio_raw(g_clazz.ctx,audio->buf,audio->len,audio->dts);

            ALOGE("in rtmp_loop send audio 1 dts:%d, len:%d, ret %d",audio->dts, audio->len, ret);

            pthread_mutex_lock(&g_clazz.mutex);
            unuse_audio_data(audio);

        }
        pthread_mutex_unlock(&g_clazz.mutex);
    } else if( video!=NULL && !g_clazz.has_audio)  {
            use_video_data();
            pthread_mutex_unlock(&g_clazz.mutex);

            if(g_clazz.is_annexb)
                ret = rtmp_write_video_annexb(g_clazz.ctx,video->buf,video->len,video->dts, g_clazz.video_size>25?1:0);
            else
                ret = rtmp_write_video_mp4(g_clazz.ctx,video->buf,video->len,video->dts, g_clazz.video_size>25?1:0);

            ALOGE("in rtmp_loop send video 2 dts:%d, len:%d ret %d",video->dts, video->len, ret);

            pthread_mutex_lock(&g_clazz.mutex);
            unuse_video_data(video);
        pthread_mutex_unlock(&g_clazz.mutex);
    } else if( audio!=NULL && !g_clazz.has_video)  {

            use_audio_data();
            pthread_mutex_unlock(&g_clazz.mutex);

            if(g_clazz.is_adts)
                ret = rtmp_write_audio_adts(g_clazz.ctx,audio->buf,audio->len,audio->dts);
            else
                ret = rtmp_write_audio_raw(g_clazz.ctx,audio->buf,audio->len,audio->dts);

            ALOGE("in rtmp_loop send audio 2 dts:%d, len:%d, ret %d",audio->dts, audio->len, ret);

//            char tmp[128]  = {0};
//                for(int i=0;i<audio->len && i < 32 ;i++) {
//                  sprintf(tmp + strlen(tmp), "%02x ", audio->buf[i]);
//         }
//               ALOGE("in rtmp_loop send audio 2 len:%d, ret:%d, list size:%d buf:%s ",
//                     audio->len, ret,g_clazz.audio_size, tmp);
            pthread_mutex_lock(&g_clazz.mutex);
            unuse_audio_data(audio);
            pthread_mutex_unlock(&g_clazz.mutex);
    } else {
        pthread_mutex_unlock(&g_clazz.mutex);
        //ALOGE("in rtmp_loop no packet to send, sleep 10 ms for wait");
        usleep(10000);
    }
    return ret;
}
*/

int init_rtmp()
{
    int ret = -1;
    pthread_mutex_lock(&g_clazz.mutex);
    rtmp_init(&g_clazz.ctx);
    g_clazz.video  = g_clazz.video_last = NULL;
    g_clazz.audio = g_clazz.audio_last = NULL;
    g_clazz.video_unused  = g_clazz.audio_unused = NULL;
    g_clazz.audio_size = g_clazz.video_size = 0;
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int unit_rtmp()
{
    int ret = -1;
    pthread_mutex_lock(&g_clazz.mutex);
    rtmp_unit(&g_clazz.ctx);
    while (g_clazz.audio != NULL) {
        data_pack* tmp = g_clazz.audio;
        g_clazz.audio = tmp->next;
        free(tmp->buf);
        free(tmp);
    }
    while (g_clazz.audio_unused != NULL) {
        data_pack* tmp = g_clazz.audio_unused;
        g_clazz.audio_unused = tmp->next;
        free(tmp->buf);
        free(tmp);
    }
    while (g_clazz.video != NULL) {
        data_pack* tmp = g_clazz.video;
        g_clazz.video = tmp->next;
        free(tmp->buf);
        free(tmp);
    }
    while (g_clazz.video_unused != NULL) {
        data_pack* tmp = g_clazz.video_unused;
        g_clazz.video_unused = tmp->next;
        free(tmp->buf);
        free(tmp);
    }
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int set_output_url(JNIEnv *env, jobject thiz, jint hasV,  jint is_annexb, jint hasA,  jint is_adts, jobject url)
{
    int ret = -1;
    const char* uri = NULL;
    pthread_mutex_lock(&g_clazz.mutex);
    if(g_clazz.ctx==NULL)
        goto LABEL_RETURN;
//open
    uri = (*env)->GetStringUTFChars(env, url, NULL );
    if(uri==NULL)
        goto LABEL_RETURN;

    rtmp_seturl(g_clazz.ctx,uri);

    g_clazz.has_audio = hasA;
    g_clazz.is_adts = is_adts;

    g_clazz.has_video = hasV;
    g_clazz.is_annexb = is_annexb;

LABEL_RETURN:
    if (uri)
        (*env)->ReleaseStringUTFChars(env, url, uri);
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int set_app(JNIEnv *env, jobject thiz, jobject app)
{
    int ret = -1;
    const char* uri = NULL;
    pthread_mutex_lock(&g_clazz.mutex);
    if(g_clazz.ctx==NULL)
        goto LABEL_RETURN;
    if(g_clazz.ctx->app!=NULL)
        free(g_clazz.ctx->app);
//open
    uri = (*env)->GetStringUTFChars(env, app, NULL );
    if(uri==NULL)
        goto LABEL_RETURN;

    g_clazz.ctx->app = strdup(app);

LABEL_RETURN:
    if (uri)
        (*env)->ReleaseStringUTFChars(env, app, uri);
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int set_tcurl(JNIEnv *env, jobject thiz, jobject tcurl)
{
    int ret = -1;
    const char* uri = NULL;
    pthread_mutex_lock(&g_clazz.mutex);
    if(g_clazz.ctx==NULL)
        goto LABEL_RETURN;
    if(g_clazz.ctx->tcurl!=NULL)
        free(g_clazz.ctx->tcurl);
//open
    uri = (*env)->GetStringUTFChars(env, tcurl, NULL );
    if(uri==NULL)
        goto LABEL_RETURN;

    g_clazz.ctx->tcurl = strdup(uri);

LABEL_RETURN:
    if (uri)
        (*env)->ReleaseStringUTFChars(env, tcurl, uri);
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int set_pageurl(JNIEnv *env, jobject thiz, jobject pageurl)
{
    int ret = -1;
    const char* uri = NULL;
    pthread_mutex_lock(&g_clazz.mutex);
    if(g_clazz.ctx==NULL)
        goto LABEL_RETURN;
    if(g_clazz.ctx->pageurl!=NULL)
        free(g_clazz.ctx->pageurl);
//open
    uri = (*env)->GetStringUTFChars(env, pageurl, NULL );
    if(uri==NULL)
        goto LABEL_RETURN;

    g_clazz.ctx->pageurl = strdup(uri);

LABEL_RETURN:
    if (uri)
        (*env)->ReleaseStringUTFChars(env, pageurl, uri);
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int set_flashver(JNIEnv *env, jobject thiz, jobject flashver)
{
    int ret = -1;
    const char* uri = NULL;
    pthread_mutex_lock(&g_clazz.mutex);
    if(g_clazz.ctx==NULL)
        goto LABEL_RETURN;
    if(g_clazz.ctx->flashver!=NULL)
        free(g_clazz.ctx->flashver);
//open
    uri = (*env)->GetStringUTFChars(env, flashver, NULL );
    if(uri==NULL)
        goto LABEL_RETURN;

    g_clazz.ctx->flashver = strdup(uri);

LABEL_RETURN:
    if (uri)
        (*env)->ReleaseStringUTFChars(env, flashver, uri);
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int set_conn(JNIEnv *env, jobject thiz, jobject conn)
{
    int ret = -1;
    const char* uri = NULL;
    pthread_mutex_lock(&g_clazz.mutex);
    if(g_clazz.ctx==NULL)
        goto LABEL_RETURN;
    if(g_clazz.ctx->conn!=NULL)
        free(g_clazz.ctx->conn);
//open
    uri = (*env)->GetStringUTFChars(env, conn, NULL );
    if(uri==NULL)
        goto LABEL_RETURN;

    g_clazz.ctx->conn = strdup(uri);

LABEL_RETURN:
    if (uri)
        (*env)->ReleaseStringUTFChars(env, conn, uri);
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int set_playpath(JNIEnv *env, jobject thiz, jobject playpath)
{
    int ret = -1;
    const char* uri = NULL;
    pthread_mutex_lock(&g_clazz.mutex);
    if(g_clazz.ctx==NULL)
        goto LABEL_RETURN;
    if(g_clazz.ctx->playpath!=NULL)
        free(g_clazz.ctx->playpath);
//open
    uri = (*env)->GetStringUTFChars(env, playpath, NULL );
    if(uri==NULL)
        goto LABEL_RETURN;

    g_clazz.ctx->playpath = strdup(uri);

LABEL_RETURN:
    if (uri)
        (*env)->ReleaseStringUTFChars(env, playpath, uri);
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int set_subscribe(JNIEnv *env, jobject thiz, jobject subscribe)
{
    int ret = -1;
    const char* uri = NULL;
    pthread_mutex_lock(&g_clazz.mutex);
    if(g_clazz.ctx==NULL)
        goto LABEL_RETURN;
    if(g_clazz.ctx->subscribe!=NULL)
        free(g_clazz.ctx->subscribe);
//open
    uri = (*env)->GetStringUTFChars(env, subscribe, NULL );
    if(uri==NULL)
        goto LABEL_RETURN;

    g_clazz.ctx->subscribe = strdup(uri);

LABEL_RETURN:
    if (uri)
        (*env)->ReleaseStringUTFChars(env, subscribe, uri);
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int set_client_buffer_time(JNIEnv *env, jobject thiz, jobject client_buffer_time)
{
    int ret = -1;
    const char* uri = NULL;
    pthread_mutex_lock(&g_clazz.mutex);
    if(g_clazz.ctx==NULL)
        goto LABEL_RETURN;
    if(g_clazz.ctx->client_buffer_time!=NULL)
        free(g_clazz.ctx->client_buffer_time);
//open
    uri = (*env)->GetStringUTFChars(env, client_buffer_time, NULL );
    if(uri==NULL)
        goto LABEL_RETURN;

    g_clazz.ctx->client_buffer_time = strdup(uri);

LABEL_RETURN:
    if (uri)
        (*env)->ReleaseStringUTFChars(env, client_buffer_time, uri);
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int set_live(JNIEnv *env, jobject thiz, int live)
{
    int ret = -1;
    pthread_mutex_lock(&g_clazz.mutex);
    if(g_clazz.ctx==NULL)
        goto LABEL_RETURN;
    g_clazz.ctx->live =live;

LABEL_RETURN:
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int set_swfverify(JNIEnv *env, jobject thiz, jobject swfverify)
{
    int ret = -1;
    const char* uri = NULL;
    pthread_mutex_lock(&g_clazz.mutex);
    if(g_clazz.ctx==NULL)
        goto LABEL_RETURN;
    if(g_clazz.ctx->swfverify!=NULL)
        free(g_clazz.ctx->swfverify);
//open
    uri = (*env)->GetStringUTFChars(env, swfverify, NULL );
    if(uri==NULL)
        goto LABEL_RETURN;

    g_clazz.ctx->swfverify = strdup(uri);

LABEL_RETURN:
    if (uri)
        (*env)->ReleaseStringUTFChars(env, swfverify, uri);
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

int set_swfurl(JNIEnv *env, jobject thiz, jobject swfurl)
{
    int ret = -1;
    const char* uri = NULL;
    pthread_mutex_lock(&g_clazz.mutex);
    if(g_clazz.ctx==NULL)
        goto LABEL_RETURN;
    if(g_clazz.ctx->swfurl!=NULL)
        free(g_clazz.ctx->swfurl);
//open
    uri = (*env)->GetStringUTFChars(env, swfurl, NULL );
    if(uri==NULL)
        goto LABEL_RETURN;

    g_clazz.ctx->swfurl = strdup(uri);

LABEL_RETURN:
    if (uri)
        (*env)->ReleaseStringUTFChars(env, swfurl, uri);
    pthread_mutex_unlock(&g_clazz.mutex);
    return ret;
}

static JNINativeMethod g_methods[] =
{
    /**
    * setting API
    **/
    { "_set_output_url", "(IIIILjava/lang/String;)I",(void*)set_output_url},
//    { "_set_input_handle", "(I)I",(void*)set_input_handle},
    /**
     * native API
    **/
    { "_open", "()I", (void*)open_rtmp},
    { "_close", "()I",(void*)close_rtmp},
    { "_loop", "()I", (void*)rtmp_loop},
   // { "_write", "([BI)I",(void*)write_rtmp},
    { "_write_rtmp_video", "([BII)I", (void*)write_rtmp_video},//write video list
    { "_write_rtmp_audio", "([BII)I", (void*)write_rtmp_audio},//write audio list
    /**
     * framework API
    **/
//    { "_start_session", "(V)I",(void*)start_session},
//    { "_stop_session", "(V)I",(void*)stop_session},
};

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv* env = NULL;

    g_jvm = vm;
    if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        return -1;
    }
    assert(env != NULL);

    pthread_mutex_init(&g_clazz.mutex, NULL );
    do
    {
        jclass clazz = (*env)->FindClass(env, JNI_CLASS_RTMP);
        if (!(clazz))
        {
            ALOGE("FindClass failed: %s", JNI_CLASS_RTMP);
            return -1;
        }
        g_clazz.clazz = (*env)->NewGlobalRef(env, clazz);
        if (!(g_clazz.clazz))
        {
            ALOGE("FindClass::NewGlobalRef failed: %s", JNI_CLASS_RTMP);
            (*env)->DeleteLocalRef(env, clazz);
            return -1;
        }
        (*env)->DeleteLocalRef(env, clazz);
    }
    while(0);

    (*env)->RegisterNatives(env, g_clazz.clazz, g_methods, NELEM(g_methods) );

    ALOGE("before init_rtmp\n");

    init_rtmp();
    return JNI_VERSION_1_4;
}

JNIEXPORT void JNI_OnUnload(JavaVM *jvm, void *reserved)
{
    unit_rtmp();
    pthread_mutex_destroy(&g_clazz.mutex);
}

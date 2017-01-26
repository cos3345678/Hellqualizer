#define __STDC_CONSTANT_MACROS

extern "C" {
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
}

#include <pthread.h>
#include <ring_buffer.h>

#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000


class DemuxDecode
{
public:
    DemuxDecode(const char* src_file_name, pthread_mutex_t *mutex, pthread_cond_t *signal, RingBuffer *Buffer_decode_process, int *endofdecoding);
    ~DemuxDecode();
    void decode_packet(int *got_frame, int *bytes_read,int cached);
    void *decode_thread(void *x_void_ptr);
private:
     AVFormatContext *fmt_ctx;// = NULL;
     AVCodecContext *video_dec_ctx; //= NULL, ;
     AVCodecContext *audio_dec_ctx;
     AVStream *audio_stream; //= NULL;
     const char *m_src_filename;// = NULL;


     int audio_stream_idx;// = -1;
     uint8_t *video_dst_data[4]; //= {NULL};
     AVFrame *frame; //= NULL;
     AVPacket pkt;
     int data_size;//=0;
     int plane_size;//=0;
    /* Enable or disable frame reference counting. You are not supposed to support
     * both paths in your application but pick the one most appropriate to your
     * needs. Look for the use of refcount in this example to see what are the
     * differences of API usage between them. */
     int refcount;// = 0;
     pthread_mutex_t *m_mutex;
     pthread_cond_t *m_signal;
     RingBuffer *m_buffer;
     int *m_endofdecoding;
     int open_codec_context(int *stream_idx,
         AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type);

};

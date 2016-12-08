extern "C" {
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
}

#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000

#include <ao/ao.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>
#include <ring_buffer.h>
#include <limits>

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx;
static int width, height;
static enum AVPixelFormat pix_fmt;
static AVStream *video_stream = NULL, *audio_stream = NULL;
static const char *src_filename = NULL;
static const char *video_dst_filename = NULL;
static const char *audio_dst_filename = NULL;
static FILE *video_dst_file = NULL;
static FILE *audio_dst_file = NULL;

static uint8_t *video_dst_data[4] = {NULL};
static int      video_dst_linesize[4];
static int video_dst_bufsize;

static int video_stream_idx = -1, audio_stream_idx = -1;
static AVFrame *frame = NULL;
static AVPacket pkt;
static int video_frame_count = 0;
static int audio_frame_count = 0;
static int data_size=0;
static int plane_size=0;

/* Enable or disable frame reference counting. You are not supposed to support
 * both paths in your application but pick the one most appropriate to your
 * needs. Look for the use of refcount in this example to see what are the
 * differences of API usage between them. */
static int refcount = 0;

/*############################LIBABO############*/


static ao_device *device;
static ao_sample_format ao_format;
static int default_driver;
static char *buffer;
static int buf_size;
static int sample;
static float freq = 440.0;
static int i;

const int buffer_size=AVCODEC_MAX_AUDIO_FRAME_SIZE+ FF_INPUT_BUFFER_PADDING_SIZE;

//#include <queue>
//std::queue <uint8_t*> myqueue;

RingBuffer* toto= new RingBuffer(441000*2);


#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000


static int decode_packet(int *got_frame, int cached)
{
    int ret = 0;
    int decoded = pkt.size;

    *got_frame = 0;

    if (pkt.stream_index == audio_stream_idx) {
        /* decode audio frame */

        ret = avcodec_decode_audio4(audio_dec_ctx, frame, got_frame, &pkt);
        if (ret < 0) {
           /*fprintf(stderr, "Error decoding audio frame (%s)\n", av_err2str(ret));*/
            return ret;
        }

        data_size = av_samples_get_buffer_size(&plane_size, audio_dec_ctx->channels,
                                               frame->nb_samples,
                                               audio_dec_ctx->sample_fmt, 1);


        /* Some audio decoders decode only part of the packet, and have to be
         * called again with the remainder of the packet data.
         * Sample: fate-suite/lossless-audio/luckynight-partial.shn
         * Also, some decoders might over-read the packet. */
        decoded = FFMIN(ret, pkt.size);

        uint8_t *samples= new uint8_t[buffer_size];
        uint16_t *out = (uint16_t *)samples;

        if (*got_frame) {
            if(audio_dec_ctx->sample_fmt==AV_SAMPLE_FMT_FLTP){
                for (int j=0; j<audio_dec_ctx->channels; j++) {
                    float* inputChannel = (float*)frame->extended_data[j];
                    for (int i=0 ; i<frame->nb_samples ; i++) {
                        float sample = inputChannel[i];
                        if (sample<-1.0f) sample=-1.0f;
                        else if (sample>1.0f) sample=1.0f;

                        out[i*audio_dec_ctx->channels + j] = (int16_t) (sample * 32767.0f );
                    }
                }
                toto->Write(samples, ( plane_size/sizeof(float) )* sizeof(uint16_t) * audio_dec_ctx->channels);
            }
        }
    }

    /* If we use frame reference counting, we own the data and need
     * to de-reference it when we don't use it anymore */
    if (*got_frame && refcount)
        av_frame_unref(frame);

    return decoded;
}

static int open_codec_context(int *stream_idx,
                              AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(type), src_filename);
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];

        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }

        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }

        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type));
            return ret;
        }


        /* Init the decoders, with or without reference counting */
        av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
        if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;

    }

    return 0;
}

int main (int argc, char **argv)
{
    int ret = 0, got_frame;

    if (argc != 2) {
        fprintf(stderr, "Tu fais de la merde ma gueule \n");
        exit(1);
    }
    src_filename = argv[1];

    /* register all formats and codecs */
    av_register_all();

    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", src_filename);
        exit(1);
    }

    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        exit(1);
    }


    if (open_codec_context(&audio_stream_idx, &audio_dec_ctx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
        audio_stream = fmt_ctx->streams[audio_stream_idx];
    }

    /* dump input information to stderr */
    av_dump_format(fmt_ctx, 0, src_filename, 0);

    if (!audio_stream) {
        fprintf(stderr, "Could not find audio  stream in the input, aborting\n");
        ret = 1;
        goto end;
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate frame\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* initialize packet, set data to NULL, let the demuxer fill it */
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

   //LIBAO INIT
    ao_initialize();

    /* -- Setup for default driver -- */

    default_driver = ao_default_driver_id();

    memset(&ao_format, 0, sizeof(ao_format));
    if(audio_dec_ctx->sample_fmt==AV_SAMPLE_FMT_FLT || audio_dec_ctx->sample_fmt==AV_SAMPLE_FMT_FLTP) {
        ao_format.bits = 16;
        ao_format.channels = audio_dec_ctx->channels;
        ao_format.rate = audio_dec_ctx->sample_rate;
        ao_format.byte_format = AO_FMT_NATIVE;
        ao_format.matrix=0;
    }
    else
    {
        exit(1);
    }

    /* -- Open driver -- */
    device = ao_open_live(default_driver, &ao_format, NULL /* no options */);
    if (device == NULL) {
        fprintf(stderr, "Error opening device.\n");
        return 1;
    }

    /* read frames from the file */
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        AVPacket orig_pkt = pkt;
        do {
            ret = decode_packet(&got_frame, 0);
            if (ret < 0)
                break;
            pkt.data += ret;
            pkt.size -= ret;
        } while (pkt.size > 0);
        av_packet_unref(&orig_pkt);
    }

    /* flush cached frames */
    pkt.data = NULL;
    pkt.size = 0;
    do {
        decode_packet(&got_frame, 1);
    } while (got_frame);



    printf("Demuxing succeeded.\n");
    printf("toto.size = %d \n",toto->GetReadAvail());

    uint8_t samples[buffer_size];

    while(toto->Read(samples,( plane_size/sizeof(float) )* sizeof(uint16_t) * audio_dec_ctx->channels)){
        printf("#");
        ao_play(device,(char*)samples, ( plane_size/sizeof(float) )* sizeof(uint16_t) * audio_dec_ctx->channels);
    }

    /* -- Close and shutdown -- */
    ao_close(device);

    ao_shutdown();

end:
    avcodec_free_context(&video_dec_ctx);
    avcodec_free_context(&audio_dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_frame_free(&frame);
    av_free(video_dst_data[0]);

    return ret < 0;
}
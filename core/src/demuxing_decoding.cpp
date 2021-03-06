#include <demuxing_decoding.h>
#include <unistd.h>

static const int buffer_size=AVCODEC_MAX_AUDIO_FRAME_SIZE+ FF_INPUT_BUFFER_PADDING_SIZE;


DemuxDecode::DemuxDecode(const char* src_file_name, HQ_Context *ctx)
{
    m_src_filename=src_file_name;
    /* register all formats and codecs */
    av_register_all();

    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, m_src_filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", m_src_filename);
        exit(EXIT_FAILURE);
    }

    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        exit(EXIT_FAILURE);
    }


    if (open_codec_context(&audio_stream_idx, &audio_dec_ctx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
        audio_stream = fmt_ctx->streams[audio_stream_idx];
    }

    /* dump input information to stderr */
    av_dump_format(fmt_ctx, 0, m_src_filename, 0);

    if (!audio_stream) {
        fprintf(stderr, "Could not find audio  stream in the input, aborting\n");
        exit(EXIT_FAILURE);
        //ret = 1;
        //goto end;
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate frame\n");
        exit(EXIT_FAILURE);
        //ret = AVERROR(ENOMEM);
        //goto end;
    }

    /* initialize packet, set data to NULL, let the demuxer fill it */
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    m_ctx=ctx;
    m_ctx->Sampling_rate=audio_dec_ctx->sample_rate;
    m_ctx->channels=audio_dec_ctx->channels;

    m_mutex=&ctx->m_mutex_decode_to_process;//mutex;
    m_signal=&ctx->m_signal_decode_to_process;
    m_buffer=ctx->Buffer_decode_process;


}

DemuxDecode::~DemuxDecode(){
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
    avcodec_close(audio_dec_ctx);
    av_free(&audio_dec_ctx);
#else
    avcodec_free_context(&audio_dec_ctx);
#endif
    avformat_close_input(&fmt_ctx);
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55,28,1)
    av_frame_free(&frame);
#else
    av_free(&frame);
#endif
    av_free(video_dst_data[0]);
}

void DemuxDecode::InternalThreadEntry(){   
    this->decode_thread(NULL);
    //return;
}

int DemuxDecode::open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        /*fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(type), m_src_filename);*/
        printf("Could not find stream in input file");
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];
        /* find decoder for the stream */
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
        dec = avcodec_find_decoder(st->codec->codec_id);
#else
        dec = avcodec_find_decoder(st->codecpar->codec_id);
#endif
        if (!dec) {
            fprintf(stderr, "Failed to find codec\n");//,
            //av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }

        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            fprintf(stderr, "Failed to allocate the codec context\n");//,
            // av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55,28,1)
                /* Copy codec parameters from input stream to output codec context */
                if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
                    fprintf(stderr, "Failed to copy codec parameters to decoder context\n");//,
                            //av_get_media_type_string(type));
                    return ret;
                }
#endif

        /* Init the decoders, with or without reference counting */
        av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
#ifdef USE_AVCTX3
        if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0) {
#else
        if ((ret = avcodec_open(*dec_ctx, dec)) < 0) {
#endif

            fprintf(stderr, "Failed to open codec\n");//,
            // av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;

    }

    return 0;
}



void DemuxDecode::decode_audio_packet(int *got_frame, int *bytes_read,int cached)
{
    int ret = 0;

    *got_frame = 0;
    int decoded = pkt.size;

    /* decode audio frame */

    ret = avcodec_decode_audio4(audio_dec_ctx, frame, got_frame, &pkt);
    if (ret < 0) {
        /*fprintf(stderr, "Error decoding audio frame (%s)\n", av_err2str(ret));*/
        //return ret;
        *bytes_read=-1;
        //goto endofdecoding;
        return;
    }

    data_size = av_samples_get_buffer_size(&plane_size, audio_dec_ctx->channels,
                                           frame->nb_samples,
                                           audio_dec_ctx->sample_fmt, 1);




    /* Some audio decoders decode only part of the packet, and have to be
     * called again with the remainder of the packet data.
     * Sample: fate-suite/lossless-audio/luckynight-partial.shn
     * Also, some decoders might over-read the packet. */
    decoded = FFMIN(ret, pkt.size);

    uint8_t samples[buffer_size];
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
        } else if (audio_dec_ctx->sample_fmt==AV_SAMPLE_FMT_S16P) {
            int i, j;
            for (j=0; j<audio_dec_ctx->channels; j++) {
                int16_t* inputChannel = (int16_t*)frame->extended_data[j];
                for (i=0 ; i<frame->nb_samples ; i++) {
                    float sample = inputChannel[i];
                    out[i*audio_dec_ctx->channels + j] = (int16_t) (sample );
                }
            }
        } else
        {
            printf("ffmpeg audio output format not (yet) supported \n");
            return;
        }

        pthread_mutex_lock(m_mutex);
        m_buffer->Write(samples, 2*audio_dec_ctx->channels*frame->nb_samples);
        //printf("writing %d samples \n",2*audio_dec_ctx->channels*frame->nb_samples);
        pthread_mutex_unlock(m_mutex);
        pthread_cond_signal(m_signal);
    }



    if(!(ret<0))
        *bytes_read=ret;

}

void *DemuxDecode::decode_thread(void *x_void_ptr)
{
    int num_bytes=0,got_frame;
    int got_space;
    while (1) {
        pthread_mutex_lock(m_mutex);
        got_space=m_buffer->GetWriteAvail();
        pthread_mutex_unlock(m_mutex);
        //printf("got_space= %d \n",got_space );
        if(got_space>44100){/*Initial buffering of 1s at 44100KHz*/
            if(av_read_frame(fmt_ctx, &pkt) >= 0){
                /*We decode only audio, skipping other packets*/
                if(pkt.stream_index==audio_stream_idx){
                    AVPacket orig_pkt = pkt;
                    do {
                        decode_audio_packet(&got_frame, &num_bytes,0);
                        if (num_bytes < 0)
                            break;
                        pkt.data += num_bytes;
                        pkt.size -= num_bytes;
                    } while (pkt.size > 0);
                    av_packet_unref(&orig_pkt);
                }
            }
            else
            {
                pthread_mutex_lock(m_mutex);
                m_ctx->state=END_OF_DECODING;
                pthread_mutex_unlock(m_mutex);
                break;
            }
        }
        else if(m_ctx->state==PAUSE){
            usleep(1000000);
            //if(log_level)
            //printf("Input Buffer overflow !!! \n");
        }
        else {
            usleep(1000);
            //printf("#");
            
        }
    }

    /* the function must return something - NULL will do */
    return NULL;

}

AVFormatContext* DemuxDecode::GetFormatCtx(void){
    return fmt_ctx;
}

AVCodecContext* DemuxDecode::GetAVCtx(void){
    return audio_dec_ctx;
}






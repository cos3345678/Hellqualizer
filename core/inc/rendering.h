#ifndef __PROCESSING_H
#define __PROCESSING_H

#include <Hellqualizer.h>

#include <pthread.h>
#include <ring_buffer.h>
#include <processing.h>
#include <threadclass.h>
#include <rendering.h>

#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000


class Rendering:public MyThreadClass
{
public:
    Rendering(HQ_Context *ctx);
    ~Rendering();
    //void decode_packet(int *got_frame, int *bytes_read,int cached);
    void *play_thread(void *x_void_ptr);
private:
    void InternalThreadEntry();
    pthread_mutex_t *m_mutex;
    pthread_cond_t *m_signal;
    RingBuffer *m_buffer_input;
    //processing_options *m_processing_options;
    HQ_Context *m_ctx;

};

#endif


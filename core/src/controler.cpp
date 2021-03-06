#include <controler.h>
#include <stdio.h>
#include <iostream>
#include <termios.h>
#include <unistd.h>

using namespace std;


Controler::Controler(char* src_file_name, HQ_Context *ctx)
{
   m_src_file_name=src_file_name;
   m_ctx=ctx;
}

void *Controler::control_thread(void *x_void_ptr)
{
    static struct termios oldt, newt;

    /*tcgetattr gets the parameters of the current terminal
    STDIN_FILENO will tell tcgetattr that it should write the settings
    of stdin to oldt*/
    tcgetattr( STDIN_FILENO, &oldt);
    /*now the settings will be copied*/
    newt = oldt;

    /*ICANON normally takes care that one line at a time will be processed
    that means it will return if it sees a "\n" or an EOF or an EOL*/
    newt.c_lflag &= ~(ICANON | ECHO);

    /*Those new settings will be set to STDIN
    TCSANOW tells tcsetattr to change attributes immediately. */
    tcsetattr( STDIN_FILENO, TCSANOW, &newt);

    while(1){
        if(m_ctx->state==END_OF_DECODING)
            break;
        char event=getchar();

        switch(event){
        case 'a':
            m_ctx->proc_opt.GAIN[0]+=0.1;
            printf("G[0]=%f \n", m_ctx->proc_opt.GAIN[0]);
            break;
        case 'q':
            m_ctx->proc_opt.GAIN[0]-=0.1;
            printf("G[0]=%f \n", m_ctx->proc_opt.GAIN[0]);
            break;
        case 'z':
            m_ctx->proc_opt.GAIN[1]+=0.1;
            printf("G[1]=%f \n", m_ctx->proc_opt.GAIN[1]);
            break;
        case 's':
            m_ctx->proc_opt.GAIN[1]-=0.1;
            printf("G[1]=%f \n", m_ctx->proc_opt.GAIN[1]);
            break;
        case 'e':
            m_ctx->proc_opt.GAIN[2]+=0.1;
            printf("G[2]=%f \n", m_ctx->proc_opt.GAIN[2]);
            break;
        case 'd':
            m_ctx->proc_opt.GAIN[2]-=0.1;
            printf("G[2]=%f \n", m_ctx->proc_opt.GAIN[2]);
            break;
        case 'r':
            m_ctx->proc_opt.GAIN[3]+=0.1;
            printf("G[3]=%f \n", m_ctx->proc_opt.GAIN[3]);
            break;
        case 'f':
            m_ctx->proc_opt.GAIN[3]-=0.1;
            printf("G[3]=%f \n", m_ctx->proc_opt.GAIN[3]);
            break;
        case 't':
            m_ctx->proc_opt.GAIN[4]+=0.1;
            printf("G[4]=%f \n", m_ctx->proc_opt.GAIN[4]);
            break;
        case 'g':
            m_ctx->proc_opt.GAIN[4]-=0.1;
            printf("G[4]=%f \n", m_ctx->proc_opt.GAIN[4]);
            break;
        case ' ':
            if(m_ctx->state==PLAY){
                printf("Pause \n");
                m_ctx->state=PAUSE;
            }else if(m_ctx->state==PAUSE){
                printf("Play \n");
                m_ctx->state=PLAY;
            }
            break;
        case 'x':
            //m_ctx->proc_opt.do_process=(m_ctx->proc_opt.do_process==1)?0:1;
            if(m_ctx->proc_opt.do_process){
                printf("Disable processing \n");
                m_ctx->proc_opt.do_process=0;
            }
            else
            {
                printf("Enable processing \n");
                m_ctx->proc_opt.do_process=1;
            }    
            break;
        default:
            break;
        }

    }

    /*restore the old settings*/
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt);

    return NULL;

}

Controler::~Controler(){

}

void Controler::InternalThreadEntry(){
   this->control_thread(NULL);
}

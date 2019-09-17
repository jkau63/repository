extern "C" {
    #include <libavutil/imgutils.h>
    #include <libavformat/avformat.h>
}
#include <pthread.h>

class decodeObject
{
    public:
        decodeObject(char *filename,int tnum);
        ~decodeObject();
        void createThread() { pthread_create(&pt,NULL,&decodeObject::threadHelper,this); }
        void joinThread() { pthread_join(pt,NULL); }
        double getSequenceLumaAve() { return sequenceLumaTotal/(double)videoFrameCount; }
    private:
        static void *threadHelper(void *context) { return ((decodeObject *)context)->decodeFile(); }
        void *decodeFile();
        int decodePacket(AVPacket *packet);
        int decodeFrame();
        int openCodec();
        void calculateFrameLuma();

        // ffmpeg variables
        AVFormatContext *fmtCtx;
        AVCodecContext *videoDecCtx;
        AVCodecContext saveCtx;
        AVFrame *theFrame;

        int videoDstLinesize[4];
        int lumaSize;
        uint8_t *videoDstData[4];
        int videoStreamIdx; 
        char srcFilename[100];

        double sequenceLumaTotal;

        pthread_t pt;
        int threadNum;
        int videoFrameCount;
};

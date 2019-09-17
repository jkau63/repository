#define __STDC_CONSTANT_MACROS
#include "decodeObject.h"
#define PRINT_HERE    printf("%s:%d:%d:%d\n",__FUNCTION__,__LINE__,threadNum,videoFrameCount);
#define RETURN_FAIL(str) { fprintf(stderr,"%s\n",str); return -1; }

decodeObject::decodeObject(char *filename,int tnum)
{
    strncpy(srcFilename,filename,sizeof(srcFilename));
    threadNum = tnum;
    fmtCtx=NULL;
    videoDecCtx=NULL;
    theFrame=NULL;
    memset(videoDstData,0,sizeof(videoDstData));
    videoFrameCount=0;
    sequenceLumaTotal=0.0;
}

decodeObject::~decodeObject()
{
    if (videoDecCtx) avcodec_free_context(&videoDecCtx);
    if (fmtCtx) avformat_close_input(&fmtCtx);
    if (theFrame) av_frame_free(&theFrame);
    if (videoDstData[0]) av_free(videoDstData[0]);
}

//#define DEBUG_CALCS1 1
#define COLORSPACE_IS_YUV 1
void decodeObject::calculateFrameLuma()
{
    double component[3];
    double currentFrameSum[3];
#ifdef DEBUG_CALCS1
    for (int i=0;i<3;i++)
#else
    for (int i=0;i<1;i++)
#endif
    {
        currentFrameSum[i] = 0.0;
        uint8_t *ptr = videoDstData[i];
        for (int j=0;j<lumaSize;j++) currentFrameSum[i] += *ptr++;
        component[i] = currentFrameSum[i]/(double)lumaSize;
    }
#ifdef DEBUG_CALCS1
    printf("%f %f %f\n",component[0],component[1],component[2]);
#endif
#ifdef COLORSPACE_IS_YUV
    double frameLumaAve = component[0];
#else
    double frameLumaAve = (0.257 * component[0]) + (0.504 * component[1]) + (0.098 * component[2]) + 16;
#endif
    sequenceLumaTotal += frameLumaAve;
}

int decodeObject::openCodec()
{
    if (avformat_open_input(&fmtCtx, srcFilename, NULL, NULL) < 0) {
        fprintf(stderr,"Could not open source file %s\n",srcFilename);
        return -1;
    }
    if (avformat_find_stream_info(fmtCtx, NULL) < 0) RETURN_FAIL("Could not find stream information")
    videoStreamIdx = av_find_best_stream(fmtCtx,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0);
    if (videoStreamIdx < 0) {
        fprintf(stderr,"Could not find stream\n");
        return videoStreamIdx;
    } 
    AVCodec *theDecoder = avcodec_find_decoder(fmtCtx->streams[videoStreamIdx]->codecpar->codec_id);
    if (!theDecoder) {
        fprintf(stderr,"Failed to find codec\n");
        return AVERROR(EINVAL);
    }
    videoDecCtx = avcodec_alloc_context3(theDecoder); // Allocate a codec context for the decoder
    if (!videoDecCtx) {
        fprintf(stderr, "Failed to allocate codec context\n");
        return AVERROR(ENOMEM);
    }
    int ret;
    if ((ret = avcodec_parameters_to_context(videoDecCtx,fmtCtx->streams[videoStreamIdx]->codecpar)) < 0) {
        fprintf(stderr, "Failed to copy codec parameters to decoder context\n");
        return ret;
    }
    AVDictionary *opts = NULL;
    av_dict_set(&opts,"refcounted_frames",/*with reference counting*/"1",0);
    if ((ret = avcodec_open2(videoDecCtx,theDecoder,&opts)) < 0) {
        fprintf(stderr, "Failed to open codec\n");
        return ret;
    }
    if (!fmtCtx->streams[videoStreamIdx]) RETURN_FAIL("Could not find video stream in the input, aborting")
    //av_dump_format(fmtCtx, 0, srcFilename, 0); // dump input information to stderr
    saveCtx = *videoDecCtx;
    lumaSize = saveCtx.width*saveCtx.height;
    if (av_image_alloc(videoDstData,videoDstLinesize,saveCtx.width,saveCtx.height,saveCtx.pix_fmt,1) < 0)
        RETURN_FAIL("Could not allocate raw video buffer")
    theFrame = av_frame_alloc();
    if (!theFrame) RETURN_FAIL("Could not allocate frame")
    return 0;
}

int decodeObject::decodeFrame()
{
    if (avcodec_receive_frame(videoDecCtx, theFrame) < 0) return -1;
    if (theFrame->width != saveCtx.width || theFrame->height != saveCtx.height || theFrame->format != saveCtx.pix_fmt)
        RETURN_FAIL("Error: Width, height or pixel format changed\n")
    videoFrameCount++;
    av_image_copy(videoDstData,videoDstLinesize,(const uint8_t **)(theFrame->data),theFrame->linesize,saveCtx.pix_fmt,saveCtx.width,saveCtx.height);
    av_frame_unref(theFrame);   //need to be ureferenced after use
    calculateFrameLuma();
    return 0;
}

int decodeObject::decodePacket(AVPacket *packet)
{
    if (packet->stream_index != videoStreamIdx) return 0;      //skip non-video packet
    int ret = avcodec_send_packet(videoDecCtx,packet);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        fprintf(stderr, "Error decoding video frame (%s)\n", av_err2str(ret));
        return ret;
    } 
    return decodeFrame();
}

void *decodeObject::decodeFile()
{
    if (openCodec() < 0) return NULL;
    AVPacket thePacket;
    av_init_packet(&thePacket);
    while (av_read_frame(fmtCtx,&thePacket) >= 0)
    {
        AVPacket origPacket = thePacket;
        while (decodePacket(&thePacket) > 0);
        av_packet_unref(&origPacket);
    }
    return NULL;
}

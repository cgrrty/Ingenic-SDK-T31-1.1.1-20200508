/*
 * sample-Encoder-video-IVS-exalgo.c
 *
 * Copyright (C) 2016 Ingenic Semiconductor Co.,Ltd
 */

#include <string.h>
#include <stdlib.h>
#include <imp/imp_log.h>
#include <imp/imp_common.h>
#include <imp/imp_system.h>
#include <imp/imp_framesource.h>
#include <imp/imp_encoder.h>
#include <imp/imp_ivs.h>

#include "sample-common.h"

#define TAG "Sample-Encoder-video-IVS-Exalgo"
#define MAX_EXALGO_RECTNUM      5
#define MAX_EXALGO_RESULTNUM    5

extern struct chn_conf chn[];

static IMPRgnHandle osd_algo_handle[MAX_EXALGO_RECTNUM];
static int ivsExalgoChnNum = 0;
static int ivsExalgoGrpNum = 0;
static int osdExalgoGrpNum = 0;

typedef struct {
	uint32_t    width;			/**< 帧宽 */
	uint32_t    height;			/**< 帧高 */
} IMP_IVS_ExalgoParam;

typedef struct {
    uint32_t    rectCnt;
    IMPRect     rect[MAX_EXALGO_RECTNUM];
} IMP_IVS_ExalgoOutput;

typedef struct {
	int index_read;
	int index_write;
	IMP_IVS_ExalgoOutput *result;
} IMP_IVS_ExalgoSem;

typedef struct {
    IMP_IVS_ExalgoParam param;  /**< 输入数据 */
    IMP_IVS_ExalgoSem algosem;  /**< 输出数据 */
} IMP_IVS_ExalgoHandler;

static int  ExalgoInit(IMPIVSInterface *inf)
{
	IMP_IVS_ExalgoHandler *handler = NULL;

	if (!inf || !inf->param) {
		IMP_LOG_ERR(TAG, "%s:param is NULL!\n", __func__);
		goto err_param_null;
	}

    /* you can init you own handler according to you algorithm */
    handler = calloc(1, sizeof(IMP_IVS_ExalgoHandler));
    if (handler == NULL) {
        IMP_LOG_ERR(TAG, "calloc() exalgo handler error !\n");
        goto err_calloc_handler;
    }

	handler->algosem.result = calloc(MAX_EXALGO_RESULTNUM, sizeof(IMP_IVS_ExalgoOutput));
	if (handler->algosem.result == NULL) {
		IMP_LOG_ERR(TAG, "calloc() algosem.result error !\n");
		goto err_calloc_algosem_result;
	}

	handler->algosem.index_write = 0;
	handler->algosem.index_read = 0;

	inf->priv = handler;

	return 0;

err_calloc_algosem_result:
	free(handler);
err_calloc_handler:
	inf->priv = NULL;
err_param_null:
    return -1;
}

static void ExalgoExit(IMPIVSInterface *inf)
{
    IMP_IVS_ExalgoHandler *handler = NULL;


    if (!inf || !inf->priv) {
        IMP_LOG_ERR(TAG, "%s:handler is NULL !\n", __func__);
        return;
    }

    handler = inf->priv;

    if (handler) {
        if (handler->algosem.result) {
            free(handler->algosem.result);
        }

        /* you can deinit you own handler according to you algorithm */
        free(handler);
    }
}

static int ExalgoPreprocessSync(IMPIVSInterface *inf, IMPFrameInfo *frame)
{
    //IMP_IVS_ExalgoHandler *handler = NULL;

	if (!inf || !inf->priv || !frame) {
		IMP_LOG_ERR(TAG, "%s:exalgo is NULL!\n", __func__);
		return -1;
	}

    //handler = inf->priv;

    //printf("%s:frame:virAddr=%u,width=%u,height=%u,pixfmt=%x,size=%u,index_write=%d, result=%p\n", __func__, frame->virAddr, frame->width, frame->height, frame->pixfmt, frame->size, handler->algosem.index_write, output);

    return 0;
}

/* IMP_IVS_ReleaseData must be called in this function whether what is the return value, unless this function is null */
static int ExalgoProcessAsync(IMPIVSInterface *inf, IMPFrameInfo *frame)
{
    IMP_IVS_ExalgoHandler *handler = NULL;
    IMP_IVS_ExalgoOutput  *output = NULL;

	if (!inf || !inf->priv || !frame) {
		IMP_LOG_ERR(TAG, "%s:exalgo is NULL!\n", __func__);
        if (frame) {
            IMP_IVS_ReleaseData((void *)frame->virAddr);
        }
		return -1;
	}

    handler = inf->priv;

    output = &handler->algosem.result[handler->algosem.index_write];
    output->rectCnt = 2;
    output->rect[0].p0.x = 1;
    output->rect[0].p0.y = 1;
    output->rect[0].p1.x = frame->width / 4;
    output->rect[0].p1.y = frame->height / 4;

    output->rect[1].p0.x = frame->width * 3 / 4;
    output->rect[1].p0.y = frame->height * 3 / 4;
    output->rect[1].p1.x = frame->width - 1;
    output->rect[1].p1.y = frame->height -1;

    //printf("%s:frame:virAddr=%u,width=%u,height=%u,pixfmt=%x,size=%u,index_write=%d, result=%p\n", __func__, frame->virAddr, frame->width, frame->height, frame->pixfmt, frame->size, handler->algosem.index_write, output);

    handler->algosem.index_write = ((handler->algosem.index_write + 1) % MAX_EXALGO_RESULTNUM);

    IMP_IVS_ReleaseData((void *)frame->virAddr);

    return 0;
}

static int ExalgoGetResult(IMPIVSInterface *inf, void **result)
{
    IMP_IVS_ExalgoHandler *handler = NULL;

	if (!inf || !inf->priv) {
		IMP_LOG_ERR(TAG, "%s:exalgo is NULL!\n", __func__);
		return -1;
	}

    handler = inf->priv;

    *result = &handler->algosem.result[handler->algosem.index_read];

    //printf("handler->algosem.index_read=%d, result=%p\n", handler->algosem.index_read, handler->algosem.result + handler->algosem.index_read);

    handler->algosem.index_read = ((handler->algosem.index_read + 1) % MAX_EXALGO_RESULTNUM);

    return 0;
}

static int ExalgoReleaseResult(IMPIVSInterface *inf, void *result)
{
    return 0;
}

static int ExalgoGetParam(IMPIVSInterface *inf, void *param)
{
    IMP_IVS_ExalgoHandler *handler = NULL;

	if (!inf || !inf->priv) {
		IMP_LOG_ERR(TAG, "%s:exalgo is NULL!\n", __func__);
		return -1;
	}

    handler = inf->priv;

    memcpy(param, &handler->param, sizeof(IMP_IVS_ExalgoParam));

    return 0;
}

static int ExalgoSetParam(IMPIVSInterface *inf, void *param)
{
    IMP_IVS_ExalgoHandler *handler = NULL;

	if (!inf || !inf->priv) {
		IMP_LOG_ERR(TAG, "%s:exalgo is NULL!\n", __func__);
		return -1;
	}

    handler = inf->priv;

    memcpy(&handler->param, param, sizeof(IMP_IVS_ExalgoParam));

    return 0;
}

static int ExalgoFlushFrame(IMPIVSInterface *inf)
{
    return 0;
}

static IMPIVSInterface *sample_ivs_exalgo_create_interface(IMP_IVS_ExalgoParam *param)
{
	IMPIVSInterface *exalgoInterface = calloc(1, sizeof(IMPIVSInterface) + sizeof(IMP_IVS_ExalgoParam));
	if (NULL == exalgoInterface) {
		IMP_LOG_ERR(TAG, "calloc exalgoInterface is NULL!\n");
		return NULL;
	}

	exalgoInterface->param          = (char *)exalgoInterface + sizeof(IMPIVSInterface);
	memcpy(exalgoInterface->param, param, sizeof(IMP_IVS_ExalgoParam));
	exalgoInterface->paramSize      = sizeof(IMP_IVS_ExalgoParam);
	exalgoInterface->pixfmt         = PIX_FMT_NV12;
	exalgoInterface->init           = ExalgoInit;
	exalgoInterface->exit           = ExalgoExit;
	exalgoInterface->preProcessSync = ExalgoPreprocessSync;
	exalgoInterface->processAsync   = ExalgoProcessAsync;
	exalgoInterface->getResult      = ExalgoGetResult;
	exalgoInterface->releaseResult  = ExalgoReleaseResult;
	exalgoInterface->getParam       = ExalgoGetParam;
	exalgoInterface->setParam       = ExalgoSetParam;
	exalgoInterface->flushFrame     = ExalgoFlushFrame;

	return exalgoInterface;
}

void sample_ivs_exalgo_destroy_interface(IMPIVSInterface *exalgoInterface)
{
    if (exalgoInterface) {
        free(exalgoInterface);
    }
}

static int sample_ivs_exalgo_init(int grp_num)
{
	int ret = 0;

	ret = IMP_IVS_CreateGroup(grp_num);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_IVS_CreateGroup(%d) failed\n", grp_num);
		return -1;
	}
	return 0;
}

static int sample_ivs_exalgo_exit(int grp_num)
{
	int ret = 0;

	ret = IMP_IVS_DestroyGroup(grp_num);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_IVS_DestroyGroup(%d) failed\n", grp_num);
		return -1;
	}
	return 0;
}

static int sample_osd_exalgo_init(int grp_num)
{
	int ret = 0;

	ret = IMP_OSD_CreateGroup(grp_num);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_OSD_CreateGroup(%d) failed\n", grp_num);
		return -1;
	}
	return 0;
}

static int sample_osd_exalgo_exit(int grp_num)
{
	int ret = 0;

	ret = IMP_OSD_DestroyGroup(grp_num);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_OSD_DestroyGroup(%d) failed\n", grp_num);
		return -1;
	}
	return 0;
}

static int sample_osd_exalgo_start(int grp_num)
{
    int ret = 0, i = 0;

    for (i = 0; i < MAX_EXALGO_RECTNUM; i++) {
        osd_algo_handle[i] = IMP_OSD_CreateRgn(NULL);
        if (osd_algo_handle[i] == INVHANDLE) {
            IMP_LOG_ERR(TAG, "IMP_OSD_CreateRgn osd_algo_handle[%d] error !\n", i);
            return -1;
        }

        ret = IMP_OSD_RegisterRgn(osd_algo_handle[i], grp_num, NULL);
        if (ret < 0) {
            IMP_LOG_ERR(TAG, "IVS IMP_OSD_RegisterRgn osd_algo_handle[%d] failed\n", i);
            return -1;
        }

        IMPOSDRgnAttr rAttrRect;
        memset(&rAttrRect, 0, sizeof(IMPOSDRgnAttr));

        rAttrRect.type = OSD_REG_RECT;
        rAttrRect.rect.p0.x = 0;
        rAttrRect.rect.p0.y = 0;
        rAttrRect.rect.p1.x = 0;
        rAttrRect.rect.p1.y = 0;
        rAttrRect.fmt = PIX_FMT_MONOWHITE;
        rAttrRect.data.lineRectData.color = OSD_GREEN;
        rAttrRect.data.lineRectData.linewidth = 5;
        ret = IMP_OSD_SetRgnAttr(osd_algo_handle[i], &rAttrRect);
        if (ret < 0) {
            IMP_LOG_ERR(TAG, "IMP_OSD_SetRgnAttr osd_algo_handle[%d] error !\n", i);
            return -1;
        }
        IMPOSDGrpRgnAttr grAttrRect;

        if (IMP_OSD_GetGrpRgnAttr(osd_algo_handle[i], grp_num, &grAttrRect) < 0) {
            IMP_LOG_ERR(TAG, "IMP_OSD_GetGrpRgnAttr osd_algo_handle[%d] error !\n", i);
            return -1;

        }
        memset(&grAttrRect, 0, sizeof(IMPOSDGrpRgnAttr));
        grAttrRect.show = 0;
        grAttrRect.layer = i;
        grAttrRect.scalex = SENSOR_WIDTH / SENSOR_WIDTH_SECOND;
        grAttrRect.scaley = SENSOR_HEIGHT / SENSOR_HEIGHT_SECOND;
        if (IMP_OSD_SetGrpRgnAttr(osd_algo_handle[i], grp_num, &grAttrRect) < 0) {
            IMP_LOG_ERR(TAG, "IMP_OSD_SetGrpRgnAttr osd_algo_handle[%d] error !\n", i);
            return -1;
        }
    }

    /* if has start, no need to do again */
	ret = IMP_OSD_Start(grp_num);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_OSD_Start osd_algo error !\n");
		return -1;
	}

	return 0;
}

static int sample_osd_exalgo_stop(int grp_num)
{
    int ret = 0, i = 0;

    /* if has stoped, no need to do again */
	ret = IMP_OSD_Stop(grp_num);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_OSD_Stop osd_algo error !\n");
		return -1;
	}

    for (i = MAX_EXALGO_RECTNUM - 1; i >= 0; i--) {
        ret = IMP_OSD_UnRegisterRgn(osd_algo_handle[i], grp_num);
        if (ret < 0) {
            IMP_LOG_ERR(TAG, "IVS IMP_OSD_UnRegisterRgn osd_algo_handle[%d] failed\n", i);
            return -1;
        }

        IMP_OSD_DestroyRgn(osd_algo_handle[i]);
    }

	return 0;
}

static int sample_ivs_exalgo_start(int grp_num, int chn_num, IMPIVSInterface **interface)
{
	int ret = 0;
	IMP_IVS_ExalgoParam param;
	//int i = 0;

	memset(&param, 0, sizeof(IMP_IVS_ExalgoParam));
	param.width = SENSOR_WIDTH_SECOND;
	param.height = SENSOR_HEIGHT_SECOND;

	*interface = sample_ivs_exalgo_create_interface(&param);
	if (*interface == NULL) {
		IMP_LOG_ERR(TAG, "IMP_IVS_CreateGroup(%d) failed\n", grp_num);
		return -1;
	}

	ret = IMP_IVS_CreateChn(chn_num, *interface);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_IVS_CreateChn(%d) failed\n", chn_num);
		return -1;
	}

	ret = IMP_IVS_RegisterChn(grp_num, chn_num);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_IVS_RegisterChn(%d, %d) failed\n", grp_num, chn_num);
		return -1;
	}

	ret = IMP_IVS_StartRecvPic(chn_num);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_IVS_StartRecvPic(%d) failed\n", chn_num);
		return -1;
	}

	return 0;
}

static int sample_ivs_exalgo_stop(int chn_num, IMPIVSInterface *interface)
{
	int ret = 0;

	ret = IMP_IVS_StopRecvPic(chn_num);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_IVS_StopRecvPic(%d) failed\n", chn_num);
		return -1;
	}
	sleep(1);

	ret = IMP_IVS_UnRegisterChn(chn_num);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_IVS_UnRegisterChn(%d) failed\n", chn_num);
		return -1;
	}

	ret = IMP_IVS_DestroyChn(chn_num);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_IVS_DestroyChn(%d) failed\n", chn_num);
		return -1;
	}

	sample_ivs_exalgo_destroy_interface(interface);

	return 0;
}

static int drawLineRectShow(IMPRgnHandle handler, int x0, int y0, int x1, int y1, int color, IMPOsdRgnType type)
{
	IMPOSDRgnAttr rAttr;
	if (IMP_OSD_GetRgnAttr(handler, &rAttr) < 0) {
		IMP_LOG_ERR(TAG, "%s(%d):IMP_OSD_GetRgnAttr failed\n", __func__, __LINE__);
		return -1;
	}
	rAttr.type = type;
	rAttr.rect.p0.x = x0;
	rAttr.rect.p0.y = y0;
	rAttr.rect.p1.x = x1;
	rAttr.rect.p1.y = y1;
	rAttr.data.lineRectData.color = color;
	if (IMP_OSD_SetRgnAttr(handler, &rAttr) < 0) {
		IMP_LOG_ERR(TAG, "%s(%d):IMP_OSD_SetRgnAttr failed\n", __func__, __LINE__);
		return -1;
	}
	if (IMP_OSD_ShowRgn(handler, osdExalgoGrpNum, 1) < 0) {
		IMP_LOG_ERR(TAG, "%s(%d): IMP_OSD_ShowRgn failed\n", __func__, __LINE__);
		return -1;
	}

	return 0;
}

static void *sample_ivs_exalgo_get_result_process(void *arg)
{
	int i = 0, ret = 0;
	int chn_num = (int)arg;
	IMP_IVS_ExalgoOutput *result = NULL;
    int x0 = 0, x1 = 0, y0 = 0, y1 = 0;

	for (i = 0; i < NR_FRAMES_TO_SAVE; i++) {
		ret = IMP_IVS_PollingResult(chn_num, IMP_IVS_DEFAULT_TIMEOUTMS);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "IMP_IVS_PollingResult(%d, %d) failed\n", chn_num, IMP_IVS_DEFAULT_TIMEOUTMS);
			return (void *)-1;
		}
		ret = IMP_IVS_GetResult(chn_num, (void **)&result);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "IMP_IVS_GetResult(%d) failed\n", chn_num);
			return (void *)-1;
		}

        if (result->rectCnt > 0) {
            int j = 0;
            for (j = 0; j < result->rectCnt; j++) {
                x0 = result->rect[j].p0.x;
                y0 = result->rect[j].p0.y;
                x1 = result->rect[j].p1.x;
                y1 = result->rect[j].p1.y;
                drawLineRectShow(osd_algo_handle[j], x0, y0, x1, y1, OSD_RED, OSD_REG_RECT);
            }
            for (; j < MAX_EXALGO_RECTNUM; j++) {
                IMP_OSD_ShowRgn(osd_algo_handle[j], osdExalgoGrpNum, 0);
            }
        }

		ret = IMP_IVS_ReleaseResult(chn_num, (void *)result);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "IMP_IVS_ReleaseResult(%d) failed\n", chn_num);
			return (void *)-1;
		}
	}

	return (void *)0;
}

static int sample_ivs_exalgo_get_result_start(int chn_num, pthread_t *ptid)
{
	if (pthread_create(ptid, NULL, sample_ivs_exalgo_get_result_process, (void *)chn_num) < 0) {
		IMP_LOG_ERR(TAG, "create sample_ivs_exalgo_get_result_process failed\n");
		return -1;
	}

	return 0;
}

static int sample_ivs_exalgo_get_result_stop(pthread_t tid)
{
	pthread_join(tid, NULL);
	return 0;
}

int main(int argc, char *argv[])
{
	int i, ret;
	pthread_t ivs_tid;
	IMPIVSInterface *inteface = NULL;

	chn[0].enable = 1;
	chn[1].enable = 0;
	chn[2].enable = 0;
	chn[3].enable = 1;

	/* Step.1 System init */
	ret = sample_system_init();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_System_Init() failed\n");
		return -1;
	}

	/* Step.2 FrameSource init */
	ret = sample_framesource_init();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "FrameSource init failed\n");
		return -1;
	}

	/* Step.3 Encoder init */
	for (i = 0; i < FS_CHN_NUM; i++) {
		if (chn[i].enable) {
			ret = IMP_Encoder_CreateGroup(chn[i].index);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "IMP_Encoder_CreateGroup(%d) error !\n", i);
				return -1;
			}
		}
	}

	ret = sample_encoder_init();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "Encoder init failed\n");
		return -1;
	}

	/* Step.4 ivs init */
	ret = sample_ivs_exalgo_init(ivsExalgoGrpNum);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "sample_ivs_exalgo_init(0) failed\n");
		return -1;
	}

    ret = sample_osd_exalgo_init(osdExalgoGrpNum);
    if (ret < 0) {
		IMP_LOG_ERR(TAG, "sample_osd_exalgo_init(0) failed\n");
		return -1;
    }

	/* Step.5 Bind */
    /* Step 5.1: first stream */
	IMPCell osdcell = {DEV_ID_OSD, osdExalgoGrpNum, 0};
    ret = IMP_System_Bind(&chn[0].framesource_chn, &osdcell);
    if (ret < 0) {
        IMP_LOG_ERR(TAG, "Bind FrameSource channel0 and OSD failed\n");
        return -1;
    }
    ret = IMP_System_Bind(&osdcell, &chn[0].imp_encoder);
    if (ret < 0) {
        IMP_LOG_ERR(TAG, "Bind OSD and Encoder0 failed\n");
        return -1;
    }

    /* Step 5.2 second stream */
	IMPCell ivscell = {DEV_ID_IVS, ivsExalgoGrpNum, 0};
	ret = IMP_System_Bind(&chn[3].framesource_chn, &ivscell);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "Bind FrameSource channel1 and ivs failed\n");
		return -1;
	}
	ret = IMP_System_Bind(&ivscell, &chn[3].imp_encoder);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "Bind ivs and Encoder1 failed\n");
		return -1;
	}

	/* Step.6 framesource Stream On */
	ret = sample_framesource_streamon();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "ImpStreamOn failed\n");
		return -1;
	}

	/* Step.7 ivs exalgo and osd start */
	ret = sample_ivs_exalgo_start(ivsExalgoGrpNum, ivsExalgoChnNum, &inteface);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "sample_ivs_exalgo_start(%d, %d) failed\n", ivsExalgoGrpNum, ivsExalgoChnNum);
		return -1;
	}

	ret = sample_osd_exalgo_start(osdExalgoGrpNum);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "sample_osd_exalgo_start(%d) failed\n", osdExalgoGrpNum);
		return -1;
	}

	/* Step.8 start to get ivs exalgo result */
	ret = sample_ivs_exalgo_get_result_start(ivsExalgoChnNum, &ivs_tid);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "sample_ivs_exalgo_get_result_start failed\n");
		return -1;
	}

	/* Step.9 get video stream */
	ret = sample_get_video_stream();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "Get H264 stream failed\n");
		return -1;
	}

	/* Exit sequence as follow */

	/* Step.10 stop to get ivs exalgo result */
	ret = sample_ivs_exalgo_get_result_stop(ivs_tid);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "sample_ivs_exalgo_get_result_stop failed\n");
		return -1;
	}

	/* Step.11 osd and ivs exalgo stop */
	ret = sample_osd_exalgo_stop(osdExalgoGrpNum);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "sample_osd_exalgo_stop(0) failed\n");
		return -1;
	}

	ret = sample_ivs_exalgo_stop(ivsExalgoChnNum, inteface);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "sample_ivs_exalgo_stop(0) failed\n");
		return -1;
	}

	/* Step.12 Stream Off */
	ret = sample_framesource_streamoff();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "FrameSource StreamOff failed\n");
		return -1;
	}

	/* Step.13 UnBind */
	/* Step 13.1 UnBind second stream */
	ret = IMP_System_UnBind(&chn[3].framesource_chn, &ivscell);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "UnBind FrameSource channel1 and ivs failed\n");
		return -1;
	}
	ret = IMP_System_UnBind(&ivscell, &chn[3].imp_encoder);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "UnBind ivs and Encoder1 failed\n");
		return -1;
	}
	/* Step 13.2 UnBind fisrt stream */
	ret = IMP_System_UnBind(&chn[0].framesource_chn, &osdcell);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "UnBind FrameSource channel0 and OSD failed\n");
		return -1;
	}
	ret = IMP_System_UnBind(&osdcell, &chn[0].imp_encoder);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "UnBind OSD and Encoder0 failed\n");
		return -1;
	}

	/* Step.14 ivs exit */
	ret = sample_ivs_exalgo_exit(ivsExalgoGrpNum);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "ivs mode exit failed\n");
		return -1;
	}
	ret = sample_osd_exalgo_exit(osdExalgoGrpNum);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "osd mode exit failed\n");
	return -1;
	}

	/* Step.15 Encoder exit */
	ret = sample_encoder_exit();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "Encoder exit failed\n");
		return -1;
	}

	/* Step.16 FrameSource exit */
	ret = sample_framesource_exit();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "FrameSource exit failed\n");
		return -1;
	}

	/* Step.17 System exit */
	ret = sample_system_exit();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "sample_system_exit() failed\n");
		return -1;
	}

	return 0;
}

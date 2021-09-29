/*
 * sample-Encoder-video.c
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co.,Ltd
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <imp/imp_log.h>
#include <imp/imp_common.h>
#include <imp/imp_system.h>
#include <imp/imp_framesource.h>
#include <imp/imp_ivs_move.h>
#ifdef WITH_ADK
#include <iaac.h>
#endif

#include "sample-common.h"

#define TAG "Sample-FrameSource"

extern struct chn_conf chn[];

static void *sample_frame_ivs_get_frame_thread(void *args)
{
	int index = (int)args;
	int chnNum = chn[index].index;
	int i = 0, j = 0, ret = 0;
	IMPFrameInfo *frame = NULL;

  IMP_IVS_MoveParam move_param;
  IMP_IVS_MoveOutput *move_output = NULL;
  IMPIVSInterface *move_interface = NULL;

	memset(&move_param, 0, sizeof(IMP_IVS_MoveParam));
	move_param.skipFrameCnt = 5;
	move_param.frameInfo.width = SENSOR_WIDTH_SECOND;
	move_param.frameInfo.height = SENSOR_HEIGHT_SECOND;
	move_param.roiRectCnt = 4;

	for(i=0; i<move_param.roiRectCnt; i++) {
	  move_param.sense[i] = 4;
	}

  for (j = 0; j < 2; j++) {
    for (i = 0; i < 2; i++) {
      if((i==0)&&(j==0)){
        move_param.roiRect[j * 2 + i].p0.x = i * move_param.frameInfo.width /* / 2 */;
        move_param.roiRect[j * 2 + i].p0.y = j * move_param.frameInfo.height /* / 2 */;
        move_param.roiRect[j * 2 + i].p1.x = (i + 1) * move_param.frameInfo.width /* / 2 */ - 1;
        move_param.roiRect[j * 2 + i].p1.y = (j + 1) * move_param.frameInfo.height /* / 2 */ - 1;
        printf("(%d,%d) = ((%d,%d)-(%d,%d))\n", i, j, move_param.roiRect[j * 2 + i].p0.x, move_param.roiRect[j * 2 + i].p0.y,move_param.roiRect[j * 2 + i].p1.x, move_param.roiRect[j * 2 + i].p1.y);
      }
      else
      {
        move_param.roiRect[j * 2 + i].p0.x = move_param.roiRect[0].p0.x;
        move_param.roiRect[j * 2 + i].p0.y = move_param.roiRect[0].p0.y;
        move_param.roiRect[j * 2 + i].p1.x = move_param.roiRect[0].p1.x;;
        move_param.roiRect[j * 2 + i].p1.y = move_param.roiRect[0].p1.y;;
        printf("(%d,%d) = ((%d,%d)-(%d,%d))\n", i, j, move_param.roiRect[j * 2 + i].p0.x, move_param.roiRect[j * 2 + i].p0.y,move_param.roiRect[j * 2 + i].p1.x, move_param.roiRect[j * 2 + i].p1.y);
      }
    }
  }

	move_interface = IMP_IVS_CreateMoveInterface(&move_param);
	if (move_interface == NULL) {
		IMP_LOG_ERR(TAG, "IMP_IVS_CreateMoveInterface\n");
    goto err_IMP_IVS_CreateMoveInterface;
	}

  if (move_interface->init && ((ret = move_interface->init(move_interface)) < 0)) {
    IMP_LOG_ERR(TAG, "move_interface->init failed,ret=%d\n", ret);
    goto err_move_interface_init;
  }

	ret = IMP_FrameSource_SetFrameDepth(chnNum, chn[index].fs_chn_attr.nrVBs * 2);
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "IMP_FrameSource_SetFrameDepth(%d,%d) failed\n", chnNum, chn[index].fs_chn_attr.nrVBs * 2);
		goto err_IMP_FrameSource_SetFrameDepth;
	}

	for (i = 0; i < NR_FRAMES_TO_SAVE; i++) {
		ret = IMP_FrameSource_GetFrame(chnNum, &frame);
		if (ret < 0) {
			IMP_LOG_ERR(TAG, "IMP_FrameSource_GetFrame(%d) i=%d failed\n", chnNum, i);
			goto err_IMP_FrameSource_GetFrame_i;
		}
    if (move_interface->preProcessSync && ((ret = move_interface->preProcessSync(move_interface, frame)) < 0)) {
      IMP_LOG_ERR(TAG, "chnNum=%d,i=%d,move_interface->preProcessSync failed,ret=%d\n", chnNum, i, ret);
      goto err_move_interface_preProcessSync_i;
    }
    if (move_interface->processAsync && ((ret = move_interface->processAsync(move_interface, frame)) < 0)) {
      IMP_LOG_ERR(TAG, "chnNum=%d,i=%d,move_interface->processAsync failed,ret=%d\n", chnNum, i, ret);
      goto err_move_interface_processAsync_i;
    }
    if (move_interface->getResult && ((ret = move_interface->getResult(move_interface, (void **)&move_output)) >= 0)) {
      IMP_LOG_INFO(TAG, "frame[%d], move_output->retRoi(%d,%d,%d,%d)\n", i, move_output->retRoi[0], move_output->retRoi[1], move_output->retRoi[2], move_output->retRoi[3]);
      if (move_interface->releaseResult) {
        move_interface->releaseResult(move_interface, (void *)move_output);
      }
    }
    IMP_FrameSource_ReleaseFrame(chnNum, frame);
	}

	IMP_FrameSource_SetFrameDepth(chnNum, 0);

  if (move_interface->exit) {
    move_interface->exit(move_interface);
  }

  IMP_IVS_DestroyMoveInterface(move_interface);

	return (void *)0;

err_move_interface_processAsync_i:
err_move_interface_preProcessSync_i:
  IMP_FrameSource_ReleaseFrame(chnNum, frame);
err_IMP_FrameSource_GetFrame_i:
	IMP_FrameSource_SetFrameDepth(chnNum, 0);
err_IMP_FrameSource_SetFrameDepth:
  if (move_interface->exit) {
    move_interface->exit(move_interface);
  }
err_move_interface_init:
  IMP_IVS_DestroyMoveInterface(move_interface);
err_IMP_IVS_CreateMoveInterface:
	return (void *)-1;
}

int sample_frame_ivs_get_frame()
{
	unsigned int i;
	int ret;
	pthread_t tid[FS_CHN_NUM];

#ifdef WITH_ADK
	IAACInfo ainfo = {
		.license_path = "./license.txt",
		.cid = 1,
		.fid = 1,
		.sn = "ae7117082a18d846e4ea433bcf7e3d2b",
	};

	ret = IAAC_Init(&ainfo);
	if (ret) {
		IMP_LOG_ERR(TAG, "IAAC_Init error!\n");
		return -1;
	}
#endif

	for (i = 0; i < FS_CHN_NUM; i++) {
		if (chn[i].enable && i == 2) {
			ret = pthread_create(&tid[i], NULL, sample_frame_ivs_get_frame_thread, (void *)i);
			if (ret < 0) {
				IMP_LOG_ERR(TAG, "Create ChnNum%d get_frame failed\n", chn[i].index);
				return -1;
			}
		}
	}

	for (i = 0; i < FS_CHN_NUM; i++) {
		if (chn[i].enable && i == 2) {
			pthread_join(tid[i],NULL);
		}
	}

#ifdef WITH_ADK
	IAAC_DeInit();
#endif

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;

	printf("Usage:%s [fpsnum2]\n", argv[0]);

	if (argc >= 2) {
		chn[2].fs_chn_attr.outFrmRateNum = atoi(argv[1]);
	}

  chn[0].enable = 1;
  chn[1].enable = 0;
  chn[2].enable = 1;

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

	/* Step.3 Stream On */
	ret = sample_framesource_streamon();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "ImpStreamOn failed\n");
		return -1;
	}

	/* Step.4 Get frame */
  ret = sample_frame_ivs_get_frame();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "sample_frame_ivs_get_frame failed\n");
		return -1;
	}

	/* Exit sequence as follow */

	/* Step.5 Stream Off */
	ret = sample_framesource_streamoff();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "FrameSource StreamOff failed\n");
		return -1;
	}

	/* Step.6 FrameSource exit */
	ret = sample_framesource_exit();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "FrameSource exit failed\n");
		return -1;
	}

	/* Step.7 System exit */
	ret = sample_system_exit();
	if (ret < 0) {
		IMP_LOG_ERR(TAG, "sample_system_exit() failed\n");
		return -1;
	}

	return 0;
}

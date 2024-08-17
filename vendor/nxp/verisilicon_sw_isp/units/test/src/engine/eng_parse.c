/******************************************************************************\
|* Copyright (c) 2020 by VeriSilicon Holdings Co., Ltd. ("VeriSilicon")       *|
|* All Rights Reserved.                                                       *|
|*                                                                            *|
|* The material in this file is confidential and contains trade secrets of    *|
|* of VeriSilicon.  This is proprietary information owned or licensed by      *|
|* VeriSilicon.  No part of this work may be disclosed, reproduced, copied,   *|
|* transmitted, or used in any way for any purpose, without the express       *|
|* written permission of VeriSilicon.                                         *|
|*                                                                            *|
\******************************************************************************/

#include <ebase/trace.h>

#include "cJSON.h"
#include "parse/include/parse_engine.h"
#include "test_base.h"
#include <assert.h>

CREATE_TRACER(ENG_PARSE_INFO, "ENG-PARSE: ", INFO, 1);
CREATE_TRACER(ENG_PARSE_ERROR, "ENG-PARSE: ", ERROR, 1);

#define FLOAT2FIX_0404(x) (int)((x) * 16) & 0x1FF

static int PAS_ParseInput(cJSON *pJObject,EngineTestConfig_t *pEngineTestConfig);
static int PAS_ParseOutput(cJSON *pJObject,EngineTestConfig_t *pEngineTestConfig);

static int PAS_ParseInputGoldenFiles(cJSON *, EngineTestConfig_t *pEngineTestConfig);
static int PAS_ParseOutputImages(cJSON *, EngineTestConfig_t *pEngineTestConfig);
static int PAS_ParseInputImages(cJSON *, EngineTestConfig_t *pEngineTestConfig);
static void PAS_PrintTestConfig( uint8_t configIndex, TestCase_t *pTestCase );

int ENGPAS_Parse(cJSON *pJObject, TestCase_t *pTestCase) {
    assert(pJObject);
    assert(pTestCase);

    pTestCase->configCount = cJSON_GetArraySize(pJObject);

    for (int i = 0; i < pTestCase->configCount && i < CONFIG_MAX; i++) {
        cJSON *pChildItem = cJSON_GetArrayItem(pJObject, i);

        EngineTestConfig_t *pEngineTestConfig = &pTestCase->config[i].engine;

        cJSON *pJItem = NULL;

    	pJItem = cJSON_GetObjectItem(pChildItem, "engine");
    	if (pJItem) {
    		ENGPAS_ParseEngineConfig(pJItem, &pEngineTestConfig->engineConfig);
    	}

    	pJItem = cJSON_GetObjectItem(pChildItem, "rotateAngle");
        if (pJItem){
            pEngineTestConfig->orientation = cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "enableDigitalGain");
        if (pJItem) {
            pEngineTestConfig->digiGain.enable = cJSON_GetObjectValue(pJItem);
            pJItem = cJSON_GetObjectItem(pChildItem, "gainR");
            if (pJItem) {
                pEngineTestConfig->digiGain.gain_r = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "gainB");
            if (pJItem) {
                pEngineTestConfig->digiGain.gain_b = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "gainGr");
            if (pJItem) {
                pEngineTestConfig->digiGain.gain_gr = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "gainGb");
            if (pJItem) {
                pEngineTestConfig->digiGain.gain_gb = cJSON_GetObjectValue(pJItem);
            }
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "enableCAC");
        if (pJItem){
            pEngineTestConfig->enableCAC = cJSON_GetObjectValue(pJItem);
    		pEngineTestConfig->disableCAC = (0 == pEngineTestConfig->enableCAC)? 1 : 0;

    		pJItem = cJSON_GetObjectItem(pChildItem, "CacXNs");
    		if (pJItem){
    			pEngineTestConfig->CacConfig.Xns = cJSON_GetObjectValue(pJItem);
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "CacXNf");
    		if (pJItem){
    			pEngineTestConfig->CacConfig.Xnf = cJSON_GetObjectValue(pJItem);
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "CacYNs");
    		if (pJItem){
    			pEngineTestConfig->CacConfig.Yns = cJSON_GetObjectValue(pJItem);
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "CacYNf");
    		if (pJItem){
    			pEngineTestConfig->CacConfig.Ynf = cJSON_GetObjectValue(pJItem);
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "CacARed");
            if (pJItem){
                pEngineTestConfig->CacConfig.aRed = FLOAT2FIX_0404(cJSON_GetObjectDouble(pJItem));
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "CacBRed");
            if (pJItem){
                pEngineTestConfig->CacConfig.bRed = FLOAT2FIX_0404(cJSON_GetObjectDouble(pJItem));
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "CacCRed");
            if (pJItem){
                pEngineTestConfig->CacConfig.cRed = FLOAT2FIX_0404(cJSON_GetObjectDouble(pJItem));
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "CacABlue");
            if (pJItem){
                pEngineTestConfig->CacConfig.aBlue = FLOAT2FIX_0404(cJSON_GetObjectDouble(pJItem));
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "CacBBlue");
            if (pJItem){
                pEngineTestConfig->CacConfig.bBlue = FLOAT2FIX_0404(cJSON_GetObjectDouble(pJItem));
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "CacCBlue");
            if (pJItem){
                pEngineTestConfig->CacConfig.cBlue = FLOAT2FIX_0404(cJSON_GetObjectDouble(pJItem));
            }

    		pJItem = cJSON_GetObjectItem(pChildItem, "CacHClip");
    		if (pJItem){
    			pEngineTestConfig->CacConfig.hClipMode = cJSON_GetObjectValue(pJItem);
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "CacVClip");
    		if (pJItem){
    			pEngineTestConfig->CacConfig.vClipMode = cJSON_GetObjectValue(pJItem);
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "CacHCenterOffset");
    		if (pJItem){
    			pEngineTestConfig->CacConfig.hCenterOffset = cJSON_GetObjectValue(pJItem);
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "CacVCenterOffset");
    		if (pJItem){
    			pEngineTestConfig->CacConfig.vCenterOffset = cJSON_GetObjectValue(pJItem);
    		}

    		//pEngineTestConfig->CacConfig.width  = pEngineConfig->pathConfig[CAM_ENGINE_PATH_MAIN].width;
    		//pEngineTestConfig->CacConfig.height = pEngineConfig->pathConfig[CAM_ENGINE_PATH_MAIN].height;
    		pEngineTestConfig->CacConfig.width  = pEngineTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_MAIN].width;
    		pEngineTestConfig->CacConfig.height = pEngineTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_MAIN].height;
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "disableCAC");
        if (pJItem){
            pEngineTestConfig->disableCAC = cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "enableDpcc");
        if (pJItem) {
            pEngineTestConfig->enableDpcc = cJSON_GetObjectValue(pJItem);
    		pEngineTestConfig->disableDpcc = (0 == pEngineTestConfig->enableDpcc)? 1 : 0;
            pJItem = cJSON_GetObjectItem(pChildItem, "DpccMode");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_mode = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccOutputMode");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_output_mode = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccRoLimits");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_ro_limits = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccRndOffs");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_rnd_offs = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccSetUse");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_set_use = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccMethodsSet1");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_methods_set_1 = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccMethodsSet2");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_methods_set_2 = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccMethodsSet3");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_methods_set_3 = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccLineThs1");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_line_thresh_1 = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccLineMadFac1");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_line_mad_fac_1 = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccPgFac1");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_pg_fac_1 = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccRndThs1");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_rnd_thresh_1 = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccRgFac1");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_rg_fac_1 = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccLineThs2");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_line_thresh_2 = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccLineMadFac2");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_line_mad_fac_2 = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccPgFac2");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_pg_fac_2 = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccRndThs2");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_rnd_thresh_2 = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccRgFac2");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_rg_fac_2 = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccLineThs3");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_line_thresh_3 = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccLineMadFac3");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_line_mad_fac_3 = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccPgFac3");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_pg_fac_3 = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccRndThs3");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_rnd_thresh_3 = cJSON_GetObjectValue( pJItem );
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DpccRgFac3");
            if (pJItem) {
                pEngineTestConfig->dpccConfig.isp_dpcc_rg_fac_3 = cJSON_GetObjectValue( pJItem );
            }
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "enableDPF");
        if (pJItem){
            pEngineTestConfig->dpfConfig.enable = cJSON_GetObjectValue(pJItem);
            pJItem = cJSON_GetObjectItem(pChildItem, "DpfFilter");
            if (pJItem){
                pEngineTestConfig->dpfConfig.filter_type = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DpfGainUsage");
            if (pJItem){
                pEngineTestConfig->dpfConfig.gain_usage = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DpfStrength");
            if (pJItem){
                pEngineTestConfig->dpfConfig.strength_r = cJSON_GetObjectValue(cJSON_GetArrayItem(pJItem, 0));
                pEngineTestConfig->dpfConfig.strength_g = cJSON_GetObjectValue(cJSON_GetArrayItem(pJItem, 1));
                pEngineTestConfig->dpfConfig.strength_b = cJSON_GetObjectValue(cJSON_GetArrayItem(pJItem, 2));
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DpfWeightG");
            if (pJItem){
                int size = sizeof(pEngineTestConfig->dpfConfig.weight_g)/sizeof(pEngineTestConfig->dpfConfig.weight_g[0]);
                for (int i = 0; i < size; i++)
                    pEngineTestConfig->dpfConfig.weight_g[i] = cJSON_GetObjectValue(cJSON_GetArrayItem(pJItem, i));
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DpfWeightRB");
            if (pJItem){
                int size = sizeof(pEngineTestConfig->dpfConfig.weight_rb)/sizeof(pEngineTestConfig->dpfConfig.weight_rb[0]);
                for (int i = 0; i < size; i++)
                    pEngineTestConfig->dpfConfig.weight_rb[i] = cJSON_GetObjectValue(cJSON_GetArrayItem(pJItem, i));
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DpfWeightDenoiseTable");
            if (pJItem){
                int size = sizeof(pEngineTestConfig->dpfConfig.denoise_talbe)/sizeof(pEngineTestConfig->dpfConfig.denoise_talbe[0]);
                for (int i = 0; i < size; i++)
                    pEngineTestConfig->dpfConfig.denoise_talbe[i] = cJSON_GetObjectValue(cJSON_GetArrayItem(pJItem, i));
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DpfXScale");
            if (pJItem){
                pEngineTestConfig->dpfConfig.x_scale = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DpfNFGain");
            if (pJItem){
                pEngineTestConfig->dpfConfig.nf_gain_r  = cJSON_GetObjectValue(cJSON_GetArrayItem(pJItem, 0));
                pEngineTestConfig->dpfConfig.nf_gain_gr = cJSON_GetObjectValue(cJSON_GetArrayItem(pJItem, 1));
                pEngineTestConfig->dpfConfig.nf_gain_gb = cJSON_GetObjectValue(cJSON_GetArrayItem(pJItem, 2));
                pEngineTestConfig->dpfConfig.nf_gain_b  = cJSON_GetObjectValue(cJSON_GetArrayItem(pJItem, 3));
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DpfFilterOff");
            if (pJItem){
                pEngineTestConfig->dpfConfig.filter_r_off  = cJSON_GetObjectValue(cJSON_GetArrayItem(pJItem, 0));
                pEngineTestConfig->dpfConfig.filter_gr_off = cJSON_GetObjectValue(cJSON_GetArrayItem(pJItem, 1));
                pEngineTestConfig->dpfConfig.filter_gb_off = cJSON_GetObjectValue(cJSON_GetArrayItem(pJItem, 2));
                pEngineTestConfig->dpfConfig.filter_b_off  = cJSON_GetObjectValue(cJSON_GetArrayItem(pJItem, 3));
            }
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "disableDPF");
        if (pJItem){
            pEngineTestConfig->dpfConfig.enable = !cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "enableGamma");
        if (pJItem){
            pEngineTestConfig->enableGamma = cJSON_GetObjectValue(pJItem);
    		pEngineTestConfig->disableGamma = (0 == pEngineTestConfig->enableGamma)? 1 : 0;
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "disableGamma");
        if (pJItem){
            pEngineTestConfig->disableGamma = cJSON_GetObjectValue(pJItem);
        }
#ifdef ISP_DEMOSAIC2
        pJItem = cJSON_GetObjectItem(pChildItem, "enableDmsc2");
        if (pJItem){
            pEngineTestConfig->dmsc2.enable = cJSON_GetObjectValue(pJItem);
            pJItem = cJSON_GetObjectItem(pChildItem, "DemosaicDenoiseStength");
            if (pJItem){
                pEngineTestConfig->dmsc2.denoise_stren = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DemosaicThreshold");
            if (pJItem){
                pEngineTestConfig->dmsc2.demosaic_thr = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "enableDemoire");
            if (pJItem) {
                pEngineTestConfig->dmsc2.demoir_cxt.enable = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DemoireAreaThr");
            if (pJItem) {
                pEngineTestConfig->dmsc2.demoir_cxt.demoire_area_thr = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DemoireSatShrink");
            if (pJItem) {
                pEngineTestConfig->dmsc2.demoir_cxt.demoire_sat_shrink = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DemoireR2");
            if (pJItem) {
                pEngineTestConfig->dmsc2.demoir_cxt.demoire_r2 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DemoireR1");
            if (pJItem) {
                pEngineTestConfig->dmsc2.demoir_cxt.demoire_r1 = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DemoireT2Shift");
            if (pJItem) {
                pEngineTestConfig->dmsc2.demoir_cxt.demoire_t2_shift = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DemoireT1");
            if (pJItem) {
                pEngineTestConfig->dmsc2.demoir_cxt.demoire_t1 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DemoireEdgeR2");
            if (pJItem) {
                pEngineTestConfig->dmsc2.demoir_cxt.demoire_edge_r2 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DemoireEdgeR1");
            if (pJItem) {
                pEngineTestConfig->dmsc2.demoir_cxt.demoire_edge_r1 = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DemoireEdgeT2Shift");
            if (pJItem) {
                pEngineTestConfig->dmsc2.demoir_cxt.demoire_edge_t2_shift = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DemoireEdgeT1");
            if (pJItem) {
                pEngineTestConfig->dmsc2.demoir_cxt.demoire_edge_t1 = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "enableDepurple");
            if (pJItem) {
                pEngineTestConfig->dmsc2.skin_cxt.enabled = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DepurpleCbcrMode");
            if (pJItem) {
                pEngineTestConfig->dmsc2.skin_cxt.cbcr_mode = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "DepurpleSatShrink");
            if (pJItem) {
                pEngineTestConfig->dmsc2.skin_cxt.depurple_sat_shrink = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "DepurpleThreshold");
            if (pJItem) {
                pEngineTestConfig->dmsc2.skin_cxt.depurple_thr = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "SkinCbThrMax2047");
            if (pJItem) {
                pEngineTestConfig->dmsc2.skin_cxt.cb_thr_max_2047 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "SkinCbThrMin2047");
            if (pJItem) {
                pEngineTestConfig->dmsc2.skin_cxt.cb_thr_min_2047 = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "SkinCrThrMax2047");
            if (pJItem) {
                pEngineTestConfig->dmsc2.skin_cxt.cr_thr_max_2047 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "SkinCrThrMin2047");
            if (pJItem) {
                pEngineTestConfig->dmsc2.skin_cxt.cr_thr_min_2047 = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "SkinYThrMax2047");
            if (pJItem) {
                pEngineTestConfig->dmsc2.skin_cxt.y_thr_max_2047 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "SkinYThrMin2047");
            if (pJItem) {
                pEngineTestConfig->dmsc2.skin_cxt.y_thr_min_2047 = cJSON_GetObjectValue(pJItem);
            }

        } else {
            pEngineTestConfig->dmsc2.enable = BOOL_FALSE;
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "enableFlt");
        if (pJItem) {
			pEngineTestConfig->flt.enable = cJSON_GetObjectValue(pJItem);
            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenSize");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_size = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenFactorBlack");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_factor_black = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenFactorWhite");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_factor_white = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenClipBlack");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_clip_black = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenClipWhite");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_clip_white = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenT4Shift");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_t4_shift = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenT3");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_t3 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenT2Shift");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_t2_shift = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenT1");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_t1 = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenR3");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_r3 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenR2");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_r2 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenR1");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_r1 = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenLineShift1");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_line_shift1 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenLineShift2");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_line_shift2 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenLineT1");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_line_t1 = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenLineStrength");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_line_strength = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenLineR2");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_line_r2 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "sharpenLineR1");
            if (pJItem) {
                pEngineTestConfig->flt.sharpen_line_r1 = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "hfFilt00");
            if (pJItem) {
                pEngineTestConfig->flt.hf_filt_00 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "hfFilt01");
            if (pJItem) {
                pEngineTestConfig->flt.hf_filt_01 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "hfFilt02");
            if (pJItem) {
                pEngineTestConfig->flt.hf_filt_02 = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "hfFilt10");
            if (pJItem) {
                pEngineTestConfig->flt.hf_filt_10 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "hfFilt11");
            if (pJItem) {
                pEngineTestConfig->flt.hf_filt_11 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "hfFilt12");
            if (pJItem) {
                pEngineTestConfig->flt.hf_filt_12 = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "hfFilt20");
            if (pJItem) {
                pEngineTestConfig->flt.hf_filt_20 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "hfFilt21");
            if (pJItem) {
                pEngineTestConfig->flt.hf_filt_21 = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "hfFilt22");
            if (pJItem) {
                pEngineTestConfig->flt.hf_filt_22 = cJSON_GetObjectValue(pJItem);
            }
        } else {
            pEngineTestConfig->flt.enable = BOOL_FALSE;
        }
#endif

#ifdef  ISP_CA
        pJItem = cJSON_GetObjectItem(pChildItem, "enableCa");
        if (pJItem) {
            pEngineTestConfig->enableCa = cJSON_GetObjectValue(pJItem);
            pJItem = cJSON_GetObjectItem(pChildItem, "ColorAdjustMode");
            if (pJItem) {
                pEngineTestConfig->caMode = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "curve_lut_x_tbl");
            if (pJItem){
                int num, i ;
                cJSON * array;

                num = cJSON_GetArraySize(pJItem);

                for ( i = 0; i < num; i++ ){
                    array = cJSON_GetArrayItem(pJItem, i);
                    pEngineTestConfig->CaLutXTbl[ i ] = cJSON_GetObjectValue( array );

                    TRACE(ENG_PARSE_INFO, "---> CaLutXTbl[ %d ] = %d\r\n", i, pEngineTestConfig->CaLutXTbl[ i ]);
                }
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "curve_lut_luma_tbl");
            if (pJItem){
                int num, i ;
                cJSON * array;

                num = cJSON_GetArraySize(pJItem);

                for ( i = 0; i < num; i++ ){
                    array = cJSON_GetArrayItem(pJItem, i);
                    pEngineTestConfig->CaLutLumaTbl[ i ] = cJSON_GetObjectValue( array );

                    TRACE(ENG_PARSE_INFO, "---> CaLutLumaTbl[ %d ] = %d\r\n", i, pEngineTestConfig->CaLutLumaTbl[ i ]);
                }
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "curve_lut_chroma_tbl");
            if (pJItem){
                int num, i ;
                cJSON * array;

                num = cJSON_GetArraySize(pJItem);

                for ( i = 0; i < num; i++ ){
                    array = cJSON_GetArrayItem(pJItem, i);
                    pEngineTestConfig->CaLutChromaTbl[ i ] = cJSON_GetObjectValue( array );

                    TRACE(ENG_PARSE_INFO, "---> CaLutChromaTbl[ %d ] = %d\r\n", i, pEngineTestConfig->CaLutChromaTbl[ i ]);
                }
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "curve_lut_shift_tbl");
            if (pJItem){
                int num, i ;
                cJSON * array;

                num = cJSON_GetArraySize(pJItem);

                for ( i = 0; i < num; i++ ){
                    array = cJSON_GetArrayItem(pJItem, i);
                    pEngineTestConfig->CaLutShiftTbl[ i ] = cJSON_GetObjectValue( array );

                    TRACE(ENG_PARSE_INFO, "---> CaLutShiftTbl[ %d ] = %d\r\n", i, pEngineTestConfig->CaLutShiftTbl[ i ]);
                }
            }
       }
#endif
        pJItem = cJSON_GetObjectItem(pChildItem, "enableLSC");
        if (pJItem){
            pEngineTestConfig->enableLSC = cJSON_GetObjectValue(pJItem);
    		pEngineTestConfig->disableLSC = (0 == pEngineTestConfig->enableLSC)? 1 : 0;

    		pJItem = cJSON_GetObjectItem(pChildItem, "LscMatrixR");
    		if (pJItem){
    			int num, i ;
    			cJSON * array;

    			num = cJSON_GetArraySize(pJItem);

    			for ( i = 0; i < num; i++ ){
    				array = cJSON_GetArrayItem(pJItem, i);
    				pEngineTestConfig->LscRDataTbl[ i ] = cJSON_GetObjectValue( array );

    				TRACE(ENG_PARSE_INFO, "---> LscRDataTbl[ %d ] = %d\r\n", i, pEngineTestConfig->LscRDataTbl[ i ]);
    			}
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "LscMatrixGR");
    		if (pJItem){
    			int num, i ;
    			cJSON * array;

    			num = cJSON_GetArraySize(pJItem);

    			for ( i = 0; i < num; i++ ){
    				array = cJSON_GetArrayItem(pJItem, i);
    				pEngineTestConfig->LscGRDataTbl[ i ] = cJSON_GetObjectValue( array );

    				TRACE(ENG_PARSE_INFO, "---> LscGRDataTbl[ %d ] = %d\r\n", i, pEngineTestConfig->LscGRDataTbl[ i ]);
    			}
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "LscMatrixGB");
    		if (pJItem){
    			int num, i ;
    			cJSON * array;

    			num = cJSON_GetArraySize(pJItem);

    			for ( i = 0; i < num; i++ ){
    				array = cJSON_GetArrayItem(pJItem, i);
    				pEngineTestConfig->LscGBDataTbl[ i ] = cJSON_GetObjectValue( array );

    				TRACE(ENG_PARSE_INFO, "---> LscGBDataTbl[ %d ] = %d\r\n", i, pEngineTestConfig->LscGBDataTbl[ i ]);
    			}
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "LscMatrixB");
    		if (pJItem){
    			int num, i ;
    			cJSON * array;

    			num = cJSON_GetArraySize(pJItem);

    			for ( i = 0; i < num; i++ ){
    				array = cJSON_GetArrayItem(pJItem, i);
    				pEngineTestConfig->LscBDataTbl[ i ] = cJSON_GetObjectValue( array );

    				TRACE(ENG_PARSE_INFO, "---> LscBDataTbl[ %d ] = %d\r\n", i, pEngineTestConfig->LscBDataTbl[ i ]);
    			}
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "LscXgrad");
    		if (pJItem){
    			int num, i ;
    			cJSON * array;

    			num = cJSON_GetArraySize(pJItem);

    			for ( i = 0; i < num; i++ ){
    				array = cJSON_GetArrayItem(pJItem, i);
    				pEngineTestConfig->LscSectorConfig.LscXGradTbl[ i ] = cJSON_GetObjectValue( array );

    				TRACE(ENG_PARSE_INFO, "---> LscXGradTbl[ %d ] = %d\r\n", i, pEngineTestConfig->LscSectorConfig.LscXGradTbl[ i ]);
    			}
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "LscYgrad");
    		if (pJItem){
    			int num, i ;
    			cJSON * array;

    			num = cJSON_GetArraySize(pJItem);

    			for ( i = 0; i < num; i++ ){
    				array = cJSON_GetArrayItem(pJItem, i);
    				pEngineTestConfig->LscSectorConfig.LscYGradTbl[ i ] = cJSON_GetObjectValue( array );

    				TRACE(ENG_PARSE_INFO, "---> LscYGradTbl[ %d ] = %d\r\n", i, pEngineTestConfig->LscSectorConfig.LscYGradTbl[ i ]);
    			}
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "LscXsize");
    		if (pJItem){
    			int num, i ;
    			cJSON * array;

    			num = cJSON_GetArraySize(pJItem);

    			for ( i = 0; i < num; i++ ){
    				array = cJSON_GetArrayItem(pJItem, i);
    				pEngineTestConfig->LscSectorConfig.LscXSizeTbl[ i ] = cJSON_GetObjectValue( array );

    				TRACE(ENG_PARSE_INFO, "---> LscXSizeTbl[ %d ] = %d\r\n", i, pEngineTestConfig->LscSectorConfig.LscXSizeTbl[ i ]);
    			}
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "LscYsize");
    		if (pJItem){
    			int num, i ;
    			cJSON * array;

    			num = cJSON_GetArraySize(pJItem);

    			for ( i = 0; i < num; i++ ){
    				array = cJSON_GetArrayItem(pJItem, i);
    				pEngineTestConfig->LscSectorConfig.LscYSizeTbl[ i ] = cJSON_GetObjectValue( array );

    				TRACE(ENG_PARSE_INFO, "---> LscYSizeTbl[ %d ] = %d\r\n", i, pEngineTestConfig->LscSectorConfig.LscYSizeTbl[ i ]);
    			}
    		}
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "disableLSC");
        if (pJItem){
            pEngineTestConfig->disableLSC = cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "enableMGC");
        if (pJItem){
            pEngineTestConfig->enableMGC = cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "disableMGC");
        if (pJItem){
            pEngineTestConfig->disableMGC = cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "enableWDR");
        if (pJItem){
            pEngineTestConfig->enableWDR = cJSON_GetObjectValue(pJItem);
            pEngineTestConfig->disableWDR = (0 == pEngineTestConfig->enableWDR) ? 1 : 0;
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "disableWDR");
        if (pJItem){
            pEngineTestConfig->disableWDR = cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "enableJpeg");
        if (pJItem)
        {
            pEngineTestConfig->enableJpeg = cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "frameNum");
        if (pJItem){
            pEngineTestConfig->frameNum = cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "enableDual");
        if (pJItem){
            pEngineTestConfig->enableDual= cJSON_GetObjectValue(pJItem);
        }

        pEngineTestConfig->verifyCurFrame = 1;                          // Verify all frames by default.
        pJItem = cJSON_GetObjectItem(pChildItem, "verifyCurFrame");
        if (pJItem){
            pEngineTestConfig->verifyCurFrame = cJSON_GetObjectValue(pJItem);
        }

    	pJItem = cJSON_GetObjectItem(pChildItem, "WDRCurve_Ym");
    	if (pJItem){
    		int num, i ;
    		cJSON * array;

    		num = cJSON_GetArraySize(pJItem);

    		for ( i = 0; i < num; i++ ){
    			array = cJSON_GetArrayItem(pJItem, i);
    			pEngineTestConfig->WdrCurve.Ym[ i ] = cJSON_GetObjectValue( array );

    			TRACE(ENG_PARSE_INFO, "---> WdrCurve.Ym[ %d ] = %d\r\n", i, pEngineTestConfig->WdrCurve.Ym[ i ]);
    		}
    	}

    	pJItem = cJSON_GetObjectItem(pChildItem, "WDRCurve_dY");
    	if (pJItem){
    		int num, i ;
    		cJSON * array;

    		num = cJSON_GetArraySize(pJItem);

    		for ( i = 0; i < num; i++ ){
    			array = cJSON_GetArrayItem(pJItem, i);
    			pEngineTestConfig->WdrCurve.dY[ i ] = cJSON_GetObjectValue( array );

    			TRACE(ENG_PARSE_INFO, "---> WdrCurve.dY[ %d ] = %d\r\n", i, pEngineTestConfig->WdrCurve.dY[ i ]);
    		}
    	}

        pJItem = cJSON_GetObjectItem(pChildItem, "enableGammaOut");
        if (pJItem){
            pEngineTestConfig->enableGammaOut = cJSON_GetObjectValue(pJItem);
            pEngineTestConfig->disableGammaOut = (0 == pEngineTestConfig->enableGammaOut) ? 1 : 0;
        }

    	pJItem = cJSON_GetObjectItem(pChildItem, "GammaY");
    	if (pJItem){
    		int num, i ;
    		cJSON * array;

    		num = cJSON_GetArraySize(pJItem);

    		for ( i = 0; i < num; i++ ){
    			array = cJSON_GetArrayItem(pJItem, i);
    			pEngineTestConfig->gammaOutCurve.GammaY[ i ] = cJSON_GetObjectValue( array );

    			TRACE(ENG_PARSE_INFO, "---> GammaY[ %d ] = %d\r\n", i, pEngineTestConfig->gammaOutCurve.GammaY[ i ]);
    		}
    	}

        pJItem = cJSON_GetObjectItem(pChildItem, "enableBLS");
        if (pJItem){
            pEngineTestConfig->enableBLS = cJSON_GetObjectValue(pJItem);
            pEngineTestConfig->disableBLS = (0 ==pEngineTestConfig->enableBLS) ? 1 : 0;

    		pJItem = cJSON_GetObjectItem(pChildItem, "BLSSubMode");
    		if (pJItem){
    			pEngineTestConfig->BLSSubMode = cJSON_GetObjectValue(pJItem);
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "BLSLevel");
    		if (pJItem){
    			int num, i ;
    			cJSON * array;

    			num = cJSON_GetArraySize(pJItem);

    			for ( i = 0; i < num; i++ ){
    				array = cJSON_GetArrayItem(pJItem, i);
    				pEngineTestConfig->BLSLevel[ i ] = cJSON_GetObjectValue( array );

    				TRACE(ENG_PARSE_INFO, "---> BLSLevel[ %d ] = %d\r\n", i, pEngineTestConfig->BLSLevel[ i ]);
    			}
    		}
    	}

        pJItem = cJSON_GetObjectItem(pChildItem, "enableAWB");
        if (pJItem){
            pEngineTestConfig->enableAWB = cJSON_GetObjectValue(pJItem);
            pEngineTestConfig->disableAWB = (0 ==pEngineTestConfig->enableAWB) ? 1 : 0;

            pJItem = cJSON_GetObjectItem(pChildItem, "AWBMeasureMode");
            if (pJItem){
        		pEngineTestConfig->AWBMeasureMode = cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "AWBGain");
            if (pJItem){
                pEngineTestConfig->AWBGain.Red    = cJSON_GetObjectValue( cJSON_GetArrayItem(pJItem, 0) ) * 0x100;
                pEngineTestConfig->AWBGain.GreenR = cJSON_GetObjectValue( cJSON_GetArrayItem(pJItem, 1) ) * 0x100;
                pEngineTestConfig->AWBGain.GreenB = cJSON_GetObjectValue( cJSON_GetArrayItem(pJItem, 2) ) * 0x100;
                pEngineTestConfig->AWBGain.Blue   = cJSON_GetObjectValue( cJSON_GetArrayItem(pJItem, 3) ) * 0x100;
            } else {
                pEngineTestConfig->AWBGain.Red    = 0x100;
                pEngineTestConfig->AWBGain.GreenR = 0x100;
                pEngineTestConfig->AWBGain.GreenB = 0x100;
                pEngineTestConfig->AWBGain.Blue   = 0x100;
            }
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "enableAEC");
        if (pJItem){
            pEngineTestConfig->enableEXP = cJSON_GetObjectValue(pJItem);
            pEngineTestConfig->disableEXP = (0 ==pEngineTestConfig->enableEXP) ? 1 : 0;

        	pJItem = cJSON_GetObjectItem(pChildItem, "AECMeasureMode");
        	if (pJItem){
        		pEngineTestConfig->EXPMeasureMode = cJSON_GetObjectValue(pJItem);
        	}
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "enableFLT");
        if (pJItem){
            pEngineTestConfig->enableFLT = cJSON_GetObjectValue(pJItem);
            pEngineTestConfig->disableFLT = (0 ==pEngineTestConfig->enableFLT) ? 1 : 0;
        }

    	pJItem = cJSON_GetObjectItem(pChildItem, "FLTDenoiseLevel");
    	if (pJItem){
    		pEngineTestConfig->FLTDenoiseLevel = cJSON_GetObjectValue(pJItem);
    	}

    	pJItem = cJSON_GetObjectItem(pChildItem, "FLTSharpenLevel");
    	if (pJItem){
    		pEngineTestConfig->FLTShrpenLevel = cJSON_GetObjectValue(pJItem);
    	}

		pJItem = cJSON_GetObjectItem(pChildItem, "enableHIST");
		if (pJItem){
			pEngineTestConfig->enableHIST = cJSON_GetObjectValue(pJItem);
			pEngineTestConfig->disableHIST = (0 ==pEngineTestConfig->enableHIST) ? 1 : 0;

			pJItem = cJSON_GetObjectItem(pChildItem, "HISTMode");
			if (pJItem){
				pEngineTestConfig->HISTMode = cJSON_GetObjectValue(pJItem);
			}
			pJItem = cJSON_GetObjectItem(pChildItem, "HISTWindowCfg");
			if (pJItem){
				cJSON * array;
				array = cJSON_GetArrayItem(pJItem, 0);
				pEngineTestConfig->histWindow.offsetX = cJSON_GetObjectValue( array );

				array = cJSON_GetArrayItem(pJItem, 1);
				pEngineTestConfig->histWindow.offsetY = cJSON_GetObjectValue( array );

				array = cJSON_GetArrayItem(pJItem, 2);
				pEngineTestConfig->histWindow.width= cJSON_GetObjectValue( array );

				array = cJSON_GetArrayItem(pJItem, 3);
				pEngineTestConfig->histWindow.height= cJSON_GetObjectValue( array );

				TRACE(ENG_PARSE_INFO, "---> hist offsetX %d, offsetY %d, width %d, height %d", pEngineTestConfig->histWindow.offsetX,  \
					  pEngineTestConfig->histWindow.offsetY, pEngineTestConfig->histWindow.width, pEngineTestConfig->histWindow.height);
			}

			pJItem = cJSON_GetObjectItem(pChildItem, "histWeightTbl");
			if (pJItem){
				int num, i ;
				cJSON * array;

				num = cJSON_GetArraySize(pJItem);
				for ( i = 0; i < num; i++ ){
					array = cJSON_GetArrayItem(pJItem, i);
					pEngineTestConfig->histWeightTbl[i] = cJSON_GetObjectValue( array );

					TRACE(ENG_PARSE_INFO, "---> histWeight table[ %d ] = %d\r\n", i, pEngineTestConfig->histWeightTbl[ i ]);
				}
			}

		}

        pJItem = cJSON_GetObjectItem(pChildItem, "enableIS");
        if (pJItem){
            pEngineTestConfig->enableIS = cJSON_GetObjectValue(pJItem);
            pEngineTestConfig->disableIS = (0 ==pEngineTestConfig->enableIS) ? 1 : 0;
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "IsConfig");
        if (pJItem){
            int num, i ;
            cJSON * array;

            num = cJSON_GetArraySize(pJItem);

             for ( i = 0; i < num; i++ ){
                 array = cJSON_GetArrayItem(pJItem, i);
                 pEngineTestConfig->IsConfigTbl[i] = cJSON_GetObjectValue( array );

                 TRACE(ENG_PARSE_INFO, "---> IsConfigTbl[ %d ] = %d  cJSON_GetObjectValue( array ) %d\r\n", i, pEngineTestConfig->IsConfigTbl[ i ],cJSON_GetObjectValue( array ));
             }
        }
#ifdef ISP_RAWIS
        pJItem = cJSON_GetObjectItem(pChildItem, "enableRawIS");
        if (pJItem){
            pEngineTestConfig->enableRawIS = cJSON_GetObjectValue(pJItem);
            pEngineTestConfig->disableRawIS = (0 ==pEngineTestConfig->enableRawIS) ? 1 : 0;
			pJItem = cJSON_GetObjectItem(pChildItem, "RawIsConfig");
			if (pJItem){
				int num, i ;
				cJSON * array;

				num = cJSON_GetArraySize(pJItem);

				 for ( i = 0; i < num; i++ ){
					 array = cJSON_GetArrayItem(pJItem, i);
					 pEngineTestConfig->rawIsConfigTbl[i] = cJSON_GetObjectValue( array );

					 TRACE(ENG_PARSE_INFO, "---> RawIsConfigTbl[ %d ] = %d  cJSON_GetObjectValue( array ) %d\r\n", i, pEngineTestConfig->rawIsConfigTbl[ i ],cJSON_GetObjectValue( array ));
				 }
			}
        } else {
            pEngineTestConfig->disableRawIS = 1;
        }
#endif

        pJItem = cJSON_GetObjectItem(pChildItem, "enableSIMP");
        if (pJItem){
            pEngineTestConfig->enableSIMP = cJSON_GetObjectValue(pJItem);
            pEngineTestConfig->disableSIMP = (0 ==pEngineTestConfig->enableSIMP) ? 1 : 0;
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "IeWorkingMode");
        if (pJItem){
            int ieMode = cJSON_GetObjectValue(pJItem);
            pEngineTestConfig->ieConfig.mode = ieMode;
            switch (pEngineTestConfig->ieConfig.mode) {
                case CAMERIC_IE_MODE_INVALID:
                    break;
                case CAMERIC_IE_MODE_GRAYSCALE:
                    break;
                case CAMERIC_IE_MODE_NEGATIVE:
                    break;
                case CAMERIC_IE_MODE_SEPIA:
                    pJItem = cJSON_GetObjectItem(pChildItem, "IeSepiaTint");
                    if (pJItem){
                        uint8_t sepiaTint = cJSON_GetObjectValue(pJItem);
                        pEngineTestConfig->ieConfig.ModeConfig.Sepia.TintCb = sepiaTint;
                        pEngineTestConfig->ieConfig.ModeConfig.Sepia.TintCr = sepiaTint;
                    }
                    break;
                case CAMERIC_IE_MODE_COLOR:
                    pJItem = cJSON_GetObjectItem(pChildItem, "IeColorSel");
                    if (pJItem){
                        pEngineTestConfig->ieConfig.ModeConfig.ColorSelection.col_selection = cJSON_GetObjectValue(pJItem);;
                    }
                    pJItem = cJSON_GetObjectItem(pChildItem, "IeColorThreshold");
                    if (pJItem){
                        pEngineTestConfig->ieConfig.ModeConfig.ColorSelection.col_threshold = cJSON_GetObjectValue(pJItem);;
                    }
                    break;
                default:
                    TRACE(ENG_PARSE_ERROR, "unsupported IE mode!\n");
                    return -1;
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "IeRange");
            if (pJItem){
                pEngineTestConfig->ieConfig.range = cJSON_GetObjectValue(pJItem);
            }
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "DegammaCurve_Seg");
        if (pJItem){
        	int num, i ;
    		cJSON * array;

    		num = cJSON_GetArraySize(pJItem);

    		for ( i = 0; i < num; i++ ){
    			array = cJSON_GetArrayItem(pJItem, i);
    			pEngineTestConfig->DegammaCurve_Seg[ i ] = cJSON_GetObjectValue( array );

    			TRACE(ENG_PARSE_INFO, "---> DegammaCurve_Seg[ %d ] = %d\r\n", i, pEngineTestConfig->DegammaCurve_Seg[ i ]);
    		}
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "DegammaCurve_R");
        if (pJItem){
        	int num, i ;
    		cJSON * array;

    		num = cJSON_GetArraySize(pJItem);

    		for ( i = 0; i < num; i++ ){
    			array = cJSON_GetArrayItem(pJItem, i);
    			pEngineTestConfig->DegammaCurve_R[ i ] = cJSON_GetObjectValue( array );

    			TRACE(ENG_PARSE_INFO, "---> DegammaCurve_R[ %d ] = %d\r\n", i, pEngineTestConfig->DegammaCurve_R[ i ]);
    		}
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "DegammaCurve_G");
        if (pJItem){
        	int num, i ;
    		cJSON * array;

    		num = cJSON_GetArraySize(pJItem);

    		for ( i = 0; i < num; i++ ){
    			array = cJSON_GetArrayItem(pJItem, i);
    			pEngineTestConfig->DegammaCurve_G[ i ] = cJSON_GetObjectValue( array );

    			TRACE(ENG_PARSE_INFO, "---> DegammaCurve_G[ %d ] = %d\r\n", i, pEngineTestConfig->DegammaCurve_G[ i ]);
    		}
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "DegammaCurve_B");
        if (pJItem){
        	int num, i ;
    		cJSON * array;

    		num = cJSON_GetArraySize(pJItem);

    		for ( i = 0; i < num; i++ ){
    			array = cJSON_GetArrayItem(pJItem, i);
    			pEngineTestConfig->DegammaCurve_B[ i ] = cJSON_GetObjectValue( array );

    			TRACE(ENG_PARSE_INFO, "---> DegammaCurve_B[ %d ] = %d\r\n", i, pEngineTestConfig->DegammaCurve_B[ i ]);
    		}
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "EnableCproc");
        if (pJItem){
            pEngineTestConfig->enableCPROC = cJSON_GetObjectValue(pJItem);
            pEngineTestConfig->disableCPROC = (0 == pEngineTestConfig->enableCPROC) ? 1 : 0;

    		pJItem = cJSON_GetObjectItem(pChildItem, "CprocChromaRangeOut");
    		if (pJItem){
    			pEngineTestConfig->CprocConfig.ChromaOut = cJSON_GetObjectValue(pJItem);
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "CprocLumaRangeIn");
    		if (pJItem){
    			pEngineTestConfig->CprocConfig.LumaIn = cJSON_GetObjectValue(pJItem);
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "CprocLumaRangeOut");
    		if (pJItem){
    			pEngineTestConfig->CprocConfig.LumaOut = cJSON_GetObjectValue(pJItem);
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "CprocContrast");
    		if (pJItem){
    			pEngineTestConfig->CprocConfig.contrast = cJSON_GetObjectDouble(pJItem);
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "CprocBrightness");
    		if (pJItem){
    			pEngineTestConfig->CprocConfig.brightness = cJSON_GetObjectValue(pJItem);
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "CprocSaturation");
    		if (pJItem){
    			pEngineTestConfig->CprocConfig.saturation = cJSON_GetObjectDouble(pJItem);
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "CprocHue");
    		if (pJItem){
    			pEngineTestConfig->CprocConfig.hue = cJSON_GetObjectDouble(pJItem);
    		}

        }

#ifdef ISP_DVP_PINMAPPING

    	pJItem = cJSON_GetObjectItem(pChildItem, "DVPInputPinMappingEn");
    	if (pJItem){
    		pEngineTestConfig->dvp_input_pin_mapping_en = cJSON_GetObjectValue(pJItem);
    	}

    	pJItem = cJSON_GetObjectItem(pChildItem, "DVPInputPinMapping");
    	if (pJItem){
    		pEngineTestConfig->dvp_input_pin_mapping = cJSON_GetObjectValue(pJItem);
    	}

#endif
#ifdef ISP_MI_HANDSHAKE_NANO
    	pJItem = cJSON_GetObjectItem(pChildItem, "HSMpHandshkEn");
    	if (pJItem){
    		pEngineTestConfig->mi_mp_handshake.handshk_switch = cJSON_GetObjectValue(pJItem);
    	}

    	pJItem = cJSON_GetObjectItem(pChildItem, "HSStorageFormat");
    	if (pJItem){
    		pEngineTestConfig->mi_mp_handshake.storage_fmt = cJSON_GetObjectValue(pJItem);
    	}

    	pJItem = cJSON_GetObjectItem(pChildItem, "HSDataFormat");
    	if (pJItem){
    		pEngineTestConfig->mi_mp_handshake.data_fmt = cJSON_GetObjectValue(pJItem);
    	}

    	pJItem = cJSON_GetObjectItem(pChildItem, "HSLinesPerSliceMinus1");
    	if (pJItem){
    		pEngineTestConfig->mi_mp_handshake.line_per_slice_minus1 = cJSON_GetObjectValue(pJItem);
    	}

    	pJItem = cJSON_GetObjectItem(pChildItem, "HSSlicesPerBufferMinus1");
    	if (pJItem){
    		pEngineTestConfig->mi_mp_handshake.slice_per_buffer_minus1 = cJSON_GetObjectValue(pJItem);
    	}

    	pJItem = cJSON_GetObjectItem(pChildItem, "HSAckCount");
    	if (pJItem)
    	{
    		pEngineTestConfig->mi_mp_handshake.ack_count = cJSON_GetObjectValue(pJItem);
    	}

    	pJItem = cJSON_GetObjectItem(pChildItem, "HSMpYLlengthPixel");
    	if (pJItem)
    	{
    		pEngineTestConfig->mi_mp_handshake.mp_y_llength_pixel = cJSON_GetObjectValue(pJItem);
    	}

    	pJItem = cJSON_GetObjectItem(pChildItem, "HSMpYSliceOffsetBytes");
    	if (pJItem)
    	{
    		pEngineTestConfig->mi_mp_handshake.mp_y_slice_offset_bytes = cJSON_GetObjectValue(pJItem);
    	}

    	pJItem = cJSON_GetObjectItem(pChildItem, "HSMpCSliceOffsetBytes");
    	if (pJItem)
    	{
    		pEngineTestConfig->mi_mp_handshake.mp_c_slice_offset_bytes = cJSON_GetObjectValue(pJItem);
    	}

    	pJItem = cJSON_GetObjectItem(pChildItem, "HSMpYPicWidthPixel");
    	if (pJItem)
    	{
    		pEngineTestConfig->mi_mp_handshake.mp_y_pic_width_pixel = cJSON_GetObjectValue(pJItem);
    	}

    	pJItem = cJSON_GetObjectItem(pChildItem, "HSMpYPicHeightPixel");
    	if (pJItem)
    	{
    		pEngineTestConfig->mi_mp_handshake.mp_y_pic_height_pixel = cJSON_GetObjectValue(pJItem);
    	}
#endif
#ifdef ISP_DPF_RAW
    	pJItem = cJSON_GetObjectItem(pChildItem, "enableDpfRawOutput");
    	if (pJItem){
    		pEngineTestConfig->dpf_raw_mode = cJSON_GetObjectValue(pJItem);
    	}
#endif
#ifdef ISP_MI_FIFO_DEPTH_NANO
    	pJItem = cJSON_GetObjectItem(pChildItem, "MiMpOutputFifoDepthEn");
    	if (pJItem){
    		pEngineTestConfig->mi_mp_fifo_depth_en = cJSON_GetObjectValue(pJItem);
    	}

    	pJItem = cJSON_GetObjectItem(pChildItem, "MiMpOutputFifoDepth");
    	if (pJItem){
    		pEngineTestConfig->mi_mp_fifo_depth = cJSON_GetObjectValue(pJItem);
    	}
#endif
#ifdef ISP_MI_ALIGN_NANO
    	pJItem = cJSON_GetObjectItem(pChildItem, "MpLsbAlignment");
    	if (pJItem)
    	{
    		pEngineTestConfig->mi_mp_lsb_align = cJSON_GetObjectValue(pJItem);
    	}
#endif
#ifdef ISP_MI_BYTESWAP
    	pJItem = cJSON_GetObjectItem(pChildItem, "MpByteSwap");
    	if (pJItem){
    		pEngineTestConfig->mi_mp_byte_swap = cJSON_GetObjectValue(pJItem);
    	}
#endif
#ifdef ISP_RGBGC
    	pJItem = cJSON_GetObjectItem(pChildItem, "enableRgbGammaOut");
    	if (pJItem){
    		pEngineTestConfig->rgbgamma_cxt.enabled = cJSON_GetObjectValue(pJItem);
			pJItem = cJSON_GetObjectItem(pChildItem, "Gamma_r_px");
			if (pJItem){
				int num, i ;
				cJSON * array;

				num = cJSON_GetArraySize(pJItem);

				for ( i = 0; i < num; i++ ){
					array = cJSON_GetArrayItem(pJItem, i);
					pEngineTestConfig->rgbgamma_cxt.rgbgammaRPx[ i ] = cJSON_GetObjectValue( array );

					TRACE(ENG_PARSE_INFO, "---> Gamma_r_px[ %d ] = %d\r\n", i, pEngineTestConfig->rgbgamma_cxt.rgbgammaRPx[ i ]);
				}
			}
			pJItem = cJSON_GetObjectItem(pChildItem, "Gamma_r_x_data");
			if (pJItem){
				int num, i ;
				cJSON * array;

				num = cJSON_GetArraySize(pJItem);

				for ( i = 0; i < num; i++ ){
					array = cJSON_GetArrayItem(pJItem, i);
					pEngineTestConfig->rgbgamma_cxt.rgbgammaRDataX[ i ] = cJSON_GetObjectValue( array );

					TRACE(ENG_PARSE_INFO, "---> rgbgcRDataX[ %d ] = %d\r\n", i, pEngineTestConfig->rgbgamma_cxt.rgbgammaRDataX[ i ]);
				}
			}
			pJItem = cJSON_GetObjectItem(pChildItem, "Gamma_r_y_data");
			if (pJItem){
				int num, i ;
				cJSON * array;

				num = cJSON_GetArraySize(pJItem);

				for ( i = 0; i < num; i++ ){
					array = cJSON_GetArrayItem(pJItem, i);
					pEngineTestConfig->rgbgamma_cxt.rgbgammaRDataY[ i ] = cJSON_GetObjectValue( array );

					TRACE(ENG_PARSE_INFO, "---> rgbgcRDataY[ %d ] = %d\r\n", i, pEngineTestConfig->rgbgamma_cxt.rgbgammaRDataY[ i ]);
				}
			}
			pJItem = cJSON_GetObjectItem(pChildItem, "Gamma_g_px");
			if (pJItem){
				int num, i ;
				cJSON * array;

				num = cJSON_GetArraySize(pJItem);

				for ( i = 0; i < num; i++ ){
					array = cJSON_GetArrayItem(pJItem, i);
					pEngineTestConfig->rgbgamma_cxt.rgbgammaGPx[ i ] = cJSON_GetObjectValue( array );

					TRACE(ENG_PARSE_INFO, "---> Gamma_g_px[ %d ] = %d\r\n", i, pEngineTestConfig->rgbgamma_cxt.rgbgammaGPx[ i ]);
				}
			}
			pJItem = cJSON_GetObjectItem(pChildItem, "Gamma_g_x_data");
			if (pJItem){
				int num, i ;
				cJSON * array;

				num = cJSON_GetArraySize(pJItem);

				for ( i = 0; i < num; i++ ){
					array = cJSON_GetArrayItem(pJItem, i);
					pEngineTestConfig->rgbgamma_cxt.rgbgammaGDataX[ i ] = cJSON_GetObjectValue( array );

					TRACE(ENG_PARSE_INFO, "---> rgbgcGDataX[ %d ] = %d\r\n", i, pEngineTestConfig->rgbgamma_cxt.rgbgammaGDataX[ i ]);
				}
			}
			pJItem = cJSON_GetObjectItem(pChildItem, "Gamma_g_y_data");
			if (pJItem){
				int num, i ;
				cJSON * array;

				num = cJSON_GetArraySize(pJItem);

				for ( i = 0; i < num; i++ ){
					array = cJSON_GetArrayItem(pJItem, i);
					pEngineTestConfig->rgbgamma_cxt.rgbgammaGDataY[ i ] = cJSON_GetObjectValue( array );

					TRACE(ENG_PARSE_INFO, "---> rgbgcGDataY[ %d ] = %d\r\n", i, pEngineTestConfig->rgbgamma_cxt.rgbgammaGDataY[ i ]);
				}
			}
			pJItem = cJSON_GetObjectItem(pChildItem, "Gamma_b_px");
			if (pJItem){
				int num, i ;
				cJSON * array;

				num = cJSON_GetArraySize(pJItem);

				for ( i = 0; i < num; i++ ){
					array = cJSON_GetArrayItem(pJItem, i);
					pEngineTestConfig->rgbgamma_cxt.rgbgammaBPx[ i ] = cJSON_GetObjectValue( array );

					TRACE(ENG_PARSE_INFO, "---> Gamma_b_px[ %d ] = %d\r\n", i, pEngineTestConfig->rgbgamma_cxt.rgbgammaBPx[ i ]);
				}
			}
			pJItem = cJSON_GetObjectItem(pChildItem, "Gamma_b_x_data");
			if (pJItem){
				int num, i ;
				cJSON * array;

				num = cJSON_GetArraySize(pJItem);

				for ( i = 0; i < num; i++ ){
					array = cJSON_GetArrayItem(pJItem, i);
					pEngineTestConfig->rgbgamma_cxt.rgbgammaBDataX[ i ] = cJSON_GetObjectValue( array );

					TRACE(ENG_PARSE_INFO, "---> rgbgcBDataX[ %d ] = %d\r\n", i, pEngineTestConfig->rgbgamma_cxt.rgbgammaBDataX[ i ]);
				}
			}
			pJItem = cJSON_GetObjectItem(pChildItem, "Gamma_b_y_data");
			if (pJItem){
				int num, i ;
				cJSON * array;

				num = cJSON_GetArraySize(pJItem);

				for ( i = 0; i < num; i++ ){
					array = cJSON_GetArrayItem(pJItem, i);
					pEngineTestConfig->rgbgamma_cxt.rgbgammaBDataY[ i ] = cJSON_GetObjectValue( array );

					TRACE(ENG_PARSE_INFO, "---> rgbgcBDataY[ %d ] = %d\r\n", i, pEngineTestConfig->rgbgamma_cxt.rgbgammaBDataY[ i ]);
				}
			}

		}
#endif
#ifdef ISP_GCMONO
    	pJItem = cJSON_GetObjectItem(pChildItem, "enableMonoGC");
    	if (pJItem){
    		pEngineTestConfig->gcmono_cxt.enabled = cJSON_GetObjectValue(pJItem);
    		pJItem = cJSON_GetObjectItem(pChildItem, "Mono_gc_mode");
    		if (pJItem){
				pEngineTestConfig->gcmono_cxt.mode= cJSON_GetObjectValue(pJItem);
    		}

    		pJItem = cJSON_GetObjectItem(pChildItem, "Mono_gc_LUT");
    		if (pJItem){
    	    	int num, i ;
    			cJSON * array;

    			num = cJSON_GetArraySize(pJItem);

    			for ( i = 0; i < num; i++ ){
    				array = cJSON_GetArrayItem(pJItem, i);
    				pEngineTestConfig->gcmono_cxt.gcmonoLut[ i ] = (unsigned char)cJSON_GetObjectValue( array );

    				TRACE(ENG_PARSE_INFO, "---> gcmonoLut[ %d ] = %d\r\n", i, pEngineTestConfig->gcmono_cxt.gcmonoLut[ i ]);
    			}
    		}

			pJItem = cJSON_GetObjectItem(pChildItem, "Mono_gc_px");
			if (pJItem){
				int num, i ;
				cJSON * array;

				num = cJSON_GetArraySize(pJItem);

				for ( i = 0; i < num; i++ ){
					array = cJSON_GetArrayItem(pJItem, i);
					pEngineTestConfig->gcmono_cxt.gcmonoPx[ i ] = cJSON_GetObjectValue( array );

					TRACE(ENG_PARSE_INFO, "---> gcmonoPx[ %d ] = %d\r\n", i, pEngineTestConfig->gcmono_cxt.gcmonoPx[ i ]);
				}
			}
			pJItem = cJSON_GetObjectItem(pChildItem, "Mono_gc_x_data");
			if (pJItem){
				int num, i ;
				cJSON * array;

				num = cJSON_GetArraySize(pJItem);

				for ( i = 0; i < num; i++ ){
					array = cJSON_GetArrayItem(pJItem, i);
					pEngineTestConfig->gcmono_cxt.gcmonoDataX[ i ] = cJSON_GetObjectValue( array );

					TRACE(ENG_PARSE_INFO, "---> gcmonoDataX[ %d ] = %d\r\n", i, pEngineTestConfig->gcmono_cxt.gcmonoDataX[ i ]);
				}
			}
			pJItem = cJSON_GetObjectItem(pChildItem, "Mono_gc_y_data");
			if (pJItem){
				int num, i ;
				cJSON * array;

				num = cJSON_GetArraySize(pJItem);

				for ( i = 0; i < num; i++ ){
					array = cJSON_GetArrayItem(pJItem, i);
					pEngineTestConfig->gcmono_cxt.gcmonoDataY[ i ] = cJSON_GetObjectValue( array );

					TRACE(ENG_PARSE_INFO, "---> gcmonoDataY[ %d ] = %d\r\n", i, pEngineTestConfig->gcmono_cxt.gcmonoDataY[ i ]);
				}
			}

    	}
#endif
#ifdef ISP8000NANO_BASE
    	pJItem = cJSON_GetObjectItem(pChildItem, "FieldSelection");
    	if (pJItem){
    		pEngineTestConfig->field_selection = cJSON_GetObjectValue(pJItem) + 1;  // reg + 1 == ENUM
    		pEngineTestConfig->enableFieldSelection = 1;
    	}
    	else{
    		pEngineTestConfig->enableFieldSelection = 0;
    		pEngineTestConfig->field_selection = CAMERIC_ISP_FIELD_SELECTION_BOTH;
    	}
#endif  //#ifdef ISP8000NANO_BASE

    	pJItem = cJSON_GetObjectItem(pChildItem, "enableCNR");
    	if (pJItem){
    		pEngineTestConfig->enableCNR = cJSON_GetObjectValue(pJItem);
    		pEngineTestConfig->disableCNR = (1 == pEngineTestConfig->enableCNR) ? 0 : 1;

            pJItem = cJSON_GetObjectItem(pChildItem, "cnrLineWidth");
            if (pJItem){
                pEngineTestConfig->cnrLineWidth = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "cnrThs1");
            if (pJItem){
                pEngineTestConfig->cnrThreshold1 = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "cnrThs2");
            if (pJItem){
                pEngineTestConfig->cnrThreshold2 = cJSON_GetObjectValue(pJItem);
            }
    	}

        /* 3DNR */
        pJItem = cJSON_GetObjectItem(pChildItem, "enable2DNR");
        if (pJItem){
            pEngineTestConfig->enable2DNR = cJSON_GetObjectValue(pJItem);
            pEngineTestConfig->disable2DNR = (1 == pEngineTestConfig->enable2DNR) ? 0 : 1;

            pJItem = cJSON_GetObjectItem(pChildItem, "denoise_2d_strength");
            if (pJItem){
                pEngineTestConfig->denoise2DStren = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "denoise_2d_pregmma_strength");
            if (pJItem){
                pEngineTestConfig->denoise2DPregmStren = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "denoise_2d_sigmay_tbl");
            if (pJItem){
                int num, i ;
                cJSON * array;

                num = cJSON_GetArraySize(pJItem);

                for ( i = 0; i < num; i++ ){
                    array = cJSON_GetArrayItem(pJItem, i);
                    pEngineTestConfig->denoiseSigmaYTbl[ i ] = cJSON_GetObjectValue( array );

                    TRACE(ENG_PARSE_INFO, "--->denoiseSigmaYTbl[ %d ] = %d\r\n", i, pEngineTestConfig->denoiseSigmaYTbl[ i ]);
                }
            }

        }

        /* WDR2 */
    	pJItem = cJSON_GetObjectItem(pChildItem, "enableWDR2");
    	if (pJItem)
    	{
    		pEngineTestConfig->enableWDR2 = cJSON_GetObjectValue(pJItem);
    		pEngineTestConfig->disableWDR2 = ( 1 == pEngineTestConfig->enableWDR2 ) ? 0 : 1;

            pJItem = cJSON_GetObjectItem(pChildItem, "WDR2Strength");
            if (pJItem)
            {
                pEngineTestConfig->WDR2Strength = cJSON_GetObjectDouble(pJItem);
            }
    	}

        /* WDR3 */
    	pJItem = cJSON_GetObjectItem(pChildItem, "enableWDR3");
    	if (pJItem)
    	{
    		pEngineTestConfig->enableWDR3 = cJSON_GetObjectValue(pJItem);
    		pEngineTestConfig->disableWDR3 = ( 1 == pEngineTestConfig->enableWDR3 ) ? 0 : 1;

            pJItem = cJSON_GetObjectItem(pChildItem, "WDR3Strength");
            if (pJItem)
            {
                pEngineTestConfig->Wdr3Strength = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "WDR3MaxGain");
            if (pJItem)
            {
                pEngineTestConfig->Wdr3MaxGain = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "WDR3GlobalStrength");
            if (pJItem)
            {
                pEngineTestConfig->Wdr3GlobalStrength = cJSON_GetObjectValue(pJItem);
            }
    	}

        /* compand */
        pJItem = cJSON_GetObjectItem(pChildItem, "enableCmpdBLS");
    	if (pJItem)
    	{
            pEngineTestConfig->cmpdBlsEnable = cJSON_GetObjectValue(pJItem);

            pJItem = cJSON_GetObjectItem(pChildItem, "BLSPara");
            if (pJItem){
    	    	int num, i ;
    			cJSON * array;

    			num = cJSON_GetArraySize(pJItem);

    			for ( i = 0; i < num; i++ ){
    				array = cJSON_GetArrayItem(pJItem, i);
    				pEngineTestConfig->BlsPara[ i ] = cJSON_GetObjectValue( array );

    				TRACE(ENG_PARSE_INFO, "---> Compand blsPara[ %d ] = %d\r\n", i, pEngineTestConfig->BlsPara[ i ]);
    			}
    		}

    	}

        pJItem = cJSON_GetObjectItem(pChildItem, "CmpdExpandTbl");
        if (pJItem){
	    	int num, i ;
			cJSON * array;

			num = cJSON_GetArraySize(pJItem);
            memset(pEngineTestConfig->ExpandTbl, 0, sizeof(pEngineTestConfig->ExpandTbl));
			for ( i = 0; i < num; i++ ){
				array = cJSON_GetArrayItem(pJItem, i);
				pEngineTestConfig->ExpandTbl[ i ] = cJSON_GetObjectValue( array );

				TRACE(ENG_PARSE_INFO, "---> Compand expandtable[ %d ] = %d\r\n", i, pEngineTestConfig->ExpandTbl[ i ]);
			}
		}

        pJItem = cJSON_GetObjectItem(pChildItem, "CmpdCompressTbl");
        if (pJItem){
	    	int num, i ;
			cJSON * array;

			num = cJSON_GetArraySize(pJItem);
            memset(pEngineTestConfig->CompressTbl, 0, sizeof(pEngineTestConfig->CompressTbl));
			for ( i = 0; i < num; i++ ){
				array = cJSON_GetArrayItem(pJItem, i);
				pEngineTestConfig->CompressTbl[ i ] = cJSON_GetObjectValue( array );

				TRACE(ENG_PARSE_INFO, "---> Compand CompressTbl[ %d ] = %d\r\n", i, pEngineTestConfig->CompressTbl[ i ]);
			}
		}

        pJItem = cJSON_GetObjectItem(pChildItem, "enableCompress");
        if (pJItem)
        {
            pEngineTestConfig->compressEnable = cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "enableExpand");
        if (pJItem)
        {
            pEngineTestConfig->expandEnable = cJSON_GetObjectValue(pJItem);
        }

        pEngineTestConfig->enableCmpd = (pEngineTestConfig->cmpdBlsEnable) | (pEngineTestConfig->compressEnable) | (pEngineTestConfig->expandEnable);
        pEngineTestConfig->disableCmpd= ( 1== pEngineTestConfig->enableCmpd ) ? 0 : 1;

        /* TPG */
        pJItem = cJSON_GetObjectItem(pChildItem, "enableTPG");
        if (pJItem)
        {
            pEngineTestConfig->enableTpg = cJSON_GetObjectValue(pJItem);
    		pEngineTestConfig->disableTpg = ( 1 == pEngineTestConfig->enableTpg ) ? 0 : 1;

            pJItem = cJSON_GetObjectItem(pChildItem, "imageType");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.imageType = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "bayerPattern");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.bayerPattern = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "colorDepth");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.colorDepth = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "resolution");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.resolution = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "pixleGap");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.pixleGap = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "lineGap");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.lineGap = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "gapStandard");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.gapStandard = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "randomSeed");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.randomSeed = cJSON_GetObjectDouble(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "h_total");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.userMode.H.total = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "h_fp");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.userMode.H.fp = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "h_bp");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.userMode.H.bp = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "h_w");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.userMode.H.syncHeaderWidth = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "h_act");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.userMode.H.act = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "v_total");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.userMode.V.total = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "v_fp");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.userMode.V.fp = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "v_bp");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.userMode.V.bp = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "v_w");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.userMode.V.syncHeaderWidth = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "v_act");
            if (pJItem)
            {
                pEngineTestConfig->tpgConfig.userMode.V.act = cJSON_GetObjectValue(pJItem);
            }
        }

        // STITCHING
      	pJItem = cJSON_GetObjectItem(pChildItem, "enableStitching");
    	  if (pJItem)
      	{
      		pEngineTestConfig->enableStitching = cJSON_GetObjectValue(pJItem);
    	  	pEngineTestConfig->disableStitching = ( 1 == pEngineTestConfig->enableStitching ) ? 0 : 1;

          pJItem = cJSON_GetObjectItem(pChildItem, "StitchingPara");

            if (pJItem)
            {
                pEngineTestConfig->StitchingPara = cJSON_GetObjectDouble(pJItem);
            }
      	}
		// EE
		pJItem = cJSON_GetObjectItem(pChildItem, "enableEe");
	    if (pJItem)
		{
	      pEngineTestConfig->EeConfig.enabled= cJSON_GetObjectValue(pJItem);

		  pJItem = cJSON_GetObjectItem(pChildItem, "ee_src_stren");

			if (pJItem)
			{
				pEngineTestConfig->EeConfig.srcStrength = cJSON_GetObjectValue(pJItem);
			}
	      pJItem = cJSON_GetObjectItem(pChildItem, "ee_stren");

		  if (pJItem)
		  {
			  pEngineTestConfig->EeConfig.strength = cJSON_GetObjectValue(pJItem);
		  }

		   pJItem = cJSON_GetObjectItem(pChildItem, "ee_input_sel");

			if (pJItem)
			{
				pEngineTestConfig->EeConfig.inputDataFormatSelect = cJSON_GetObjectValue(pJItem);
			}

			pJItem = cJSON_GetObjectItem(pChildItem, "ee_y_gain");

			if (pJItem)
			{	u32 yGain = cJSON_GetObjectValue(pJItem);
				pEngineTestConfig->EeConfig.yUpGain = (uint16_t)(yGain & 0x0000FFFF);
				pEngineTestConfig->EeConfig.yDownGain = ((u32)yGain & 0xFFFF0000) >> 16;
			}
			pJItem = cJSON_GetObjectItem(pChildItem, "ee_uv_gain");
			if (pJItem)
			{
				pEngineTestConfig->EeConfig.uvGain = cJSON_GetObjectValue(pJItem);
			}

			pJItem = cJSON_GetObjectItem(pChildItem, "ee_edge_gain");
			if (pJItem)
			{
				pEngineTestConfig->EeConfig.edgeGain = cJSON_GetObjectValue(pJItem);
			}
		}
        // CSM
      	pJItem = cJSON_GetObjectItem(pChildItem, "enableCSM");
        if (pJItem)
        {
            pEngineTestConfig->enableCSM = cJSON_GetObjectValue(pJItem);
            pJItem = cJSON_GetObjectItem(pChildItem, "csmColorRangeY");
            if (pJItem)
            {
                pEngineTestConfig->csm_color_range_y = cJSON_GetObjectValue(pJItem);
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "csmColorRangeC");
            if (pJItem)
            {
                pEngineTestConfig->csm_color_range_c = cJSON_GetObjectValue(pJItem);
            }
      	}

        // XTalk
        pJItem = cJSON_GetObjectItem(pChildItem, "enableXtalk");
        if (pJItem) {
            pEngineTestConfig->enableXtalk = cJSON_GetObjectValue(pJItem);
            pJItem = cJSON_GetObjectItem(pChildItem, "XtalkCCMatrix");
            if (pJItem) {
                int num, i ;
                cJSON * array;
                num = cJSON_GetArraySize(pJItem);
                for ( i = 0; i < num; i++ ){
                    array = cJSON_GetArrayItem(pJItem, i);
                    pEngineTestConfig->ccMatrix.Coeff[ i ] = cJSON_GetObjectDouble( array );
                }
            }

            pJItem = cJSON_GetObjectItem(pChildItem, "XtalkCCOffset");
            if (pJItem) {
                pEngineTestConfig->ccOffset.Red   = cJSON_GetObjectDouble( cJSON_GetArrayItem(pJItem, 0) );
                pEngineTestConfig->ccOffset.Green = cJSON_GetObjectDouble( cJSON_GetArrayItem(pJItem, 1) );
                pEngineTestConfig->ccOffset.Blue  = cJSON_GetObjectDouble( cJSON_GetArrayItem(pJItem, 2) );
            }
        }
        pJItem = cJSON_GetObjectItem(pChildItem, "enableAfm");
        if (pJItem)
        {
            pEngineTestConfig->enableAfm = cJSON_GetObjectValue(pJItem);
            pEngineTestConfig->disableAfm = ( 1 == pEngineTestConfig->enableAfm ) ? 0 : 1;
            pJItem = cJSON_GetObjectItem(pChildItem, "AfmWindowId");
            if (pJItem)
            {
                pEngineTestConfig->AfmWindowId= cJSON_GetObjectValue(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "AfmThreshold");
            if (pJItem)
            {
                pEngineTestConfig->AfmThreshold = cJSON_GetObjectValue(pJItem);
            }
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "enableVsm");
        if (pJItem)
        {
            pEngineTestConfig->enableVsm = cJSON_GetObjectValue(pJItem);

        }
        pJItem = cJSON_GetObjectItem(pChildItem, "VsmWinHoffset");
        if (pJItem)
        {
            pEngineTestConfig->vsmWinHoffset = cJSON_GetObjectValue(pJItem);

        }
        pJItem = cJSON_GetObjectItem(pChildItem, "VsmWinVoffset");
        if (pJItem)
        {
            pEngineTestConfig->vsmWinVoffset = cJSON_GetObjectValue(pJItem);

        }
        pJItem = cJSON_GetObjectItem(pChildItem, "VsmWinHeight");
        if (pJItem)
        {
            pEngineTestConfig->vsmWinHeight = cJSON_GetObjectValue(pJItem);

        }
        pJItem = cJSON_GetObjectItem(pChildItem, "VsmWinWidth");
        if (pJItem)
        {
            pEngineTestConfig->vsmWinWidth = cJSON_GetObjectValue(pJItem);

        }


#ifdef ISP_DEC

        pJItem = cJSON_GetObjectItem(pChildItem, "enableDec");

        if (pJItem)
        {
            pEngineTestConfig->enableDec = cJSON_GetObjectValue(pJItem);

        }
#endif
        pJItem = cJSON_GetObjectItem(pChildItem, "DemosaicMode");
        if (pJItem)
        {
            pEngineTestConfig->DemosaicMode = cJSON_GetObjectValue(pJItem);
            pJItem = cJSON_GetObjectItem(pChildItem, "DemosaicThreshold");
            if (pJItem)
            {
                pEngineTestConfig->DemosaicThreshold = cJSON_GetObjectDouble(pJItem);
            }
        }
#ifdef ISP_GREENEQUILIBRATE
           pJItem = cJSON_GetObjectItem(pChildItem, "enableGreenEquilibration");
        if (pJItem)
        {
            pEngineTestConfig->enableGrnEqlr = cJSON_GetObjectValue(pJItem);
            pJItem = cJSON_GetObjectItem(pChildItem, "greenEquilibrationTh");
            if (pJItem)
            {
                pEngineTestConfig->grnEqlrTh= cJSON_GetObjectDouble(pJItem);
            }
            pJItem = cJSON_GetObjectItem(pChildItem, "greenEquilibrationHcntDmy");
            if (pJItem)
            {
                pEngineTestConfig->grnEqlrHcntDmy = cJSON_GetObjectDouble(pJItem);
            }
        }
#endif
        /* 3DNR */
		pJItem = cJSON_GetObjectItem(pChildItem, "denoise3dEnable");
		if (pJItem)
		{
			pEngineTestConfig->denoise3dEnable = cJSON_GetObjectValue(pJItem);

			pJItem = cJSON_GetObjectItem(pChildItem, "denoise3dStrength");
			if (pJItem)
			{
				pEngineTestConfig->denoise3dStrength = cJSON_GetObjectDouble(pJItem);
			}
			else
			{
				pEngineTestConfig->denoise3dStrength = 0;
                        }

                        pJItem = cJSON_GetObjectItem(pChildItem, "denoise3dWeightUpY");
                        if (pJItem) {
                            int num, i;
                            cJSON *array;

                            num = cJSON_GetArraySize(pJItem);
                            for (i = 0; i < num; i++) {
                                array = cJSON_GetArrayItem(pJItem, i);
                                pEngineTestConfig->compress.weightUpY[i] =
                                    cJSON_GetObjectValue(array);

                                TRACE(ENG_PARSE_INFO,
                                      "---> 3dnr compress  weight_up_y[ %d ] = %d\r\n", i,
                                      pEngineTestConfig->compress.weightUpY[i]);
                            }
                        }
                        pJItem = cJSON_GetObjectItem(pChildItem, "denoise3dWeightDown");
                        if (pJItem) {
                            int num, i;
                            cJSON *array;

                            num = cJSON_GetArraySize(pJItem);
                            for (i = 0; i < num; i++) {
                                array = cJSON_GetArrayItem(pJItem, i);
                                pEngineTestConfig->compress.weightDown[i] =
                                    cJSON_GetObjectValue(array);

                                TRACE(ENG_PARSE_INFO,
                                      "---> 3dnr compress  weight_down[ %d ] = %d\r\n", i,
                                      pEngineTestConfig->compress.weightDown[i]);
                            }
                        }
                        pJItem = cJSON_GetObjectItem(pChildItem, "denoise3dWeightUpX");
                        if (pJItem) {
                            int num, i;
                            cJSON *array;

                            num = cJSON_GetArraySize(pJItem);
                            for (i = 0; i < num; i++) {
                                array = cJSON_GetArrayItem(pJItem, i);
                                pEngineTestConfig->compress.weightUpX[i] =
                                    cJSON_GetObjectValue(array);

                                TRACE(ENG_PARSE_INFO,
                                      "---> 3dnr compress  weight_up_x[ %d ] = %d\r\n", i,
                                      pEngineTestConfig->compress.weightUpX[i]);
                            }
                        }
                }

        // AEV2
        pJItem = cJSON_GetObjectItem(pChildItem, "enableAev2");
        if (pJItem) {
            pEngineTestConfig->enableAev2 = cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "Aev2VOffset");
        if (pJItem) {
            pEngineTestConfig->AEV2Config.vOffset = cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "Aev2HOffset");
        if (pJItem) {
            pEngineTestConfig->AEV2Config.hOffset = cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "Aev2Width");
        if (pJItem) {
            pEngineTestConfig->AEV2Config.width = cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "Aev2Height");
        if (pJItem) {
            pEngineTestConfig->AEV2Config.height = cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "Aev2RWeight");
        if (pJItem) {
            pEngineTestConfig->AEV2Config.rWeight = cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "Aev2GrWeight");
        if (pJItem) {
            pEngineTestConfig->AEV2Config.grWeight = cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "Aev2GbWeight");
        if (pJItem) {
            pEngineTestConfig->AEV2Config.gbWeight = cJSON_GetObjectValue(pJItem);
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "Aev2BWeight");
        if (pJItem) {
            pEngineTestConfig->AEV2Config.bWeight = cJSON_GetObjectValue(pJItem);
        }

        //---------------------------------------------------------------------
        // Input and output

        pJItem = cJSON_GetObjectItem(pChildItem, "input");
        if (pJItem){
            PAS_ParseInput(pJItem, pEngineTestConfig);
        }else{
            TRACE(ENG_PARSE_ERROR, "input segment not found in json!\n");
        }

        pJItem = cJSON_GetObjectItem(pChildItem, "output");
        if (pJItem){
            PAS_ParseOutput(pJItem, pEngineTestConfig);
        }else{
            TRACE(ENG_PARSE_ERROR, "output segment not found in json!\n");
        }

        //pTestCase->configCount++;

		PAS_PrintTestConfig( i, pTestCase ) ;
    }

    return 0;
}


static int PAS_ParseInput(cJSON *pJObject,
                           EngineTestConfig_t *pEngineTestConfig) {
    cJSON *pJItem = NULL;

    pJItem = cJSON_GetObjectItem(pJObject, "goldenFiles");
    if (pJItem) {
        PAS_ParseInputGoldenFiles(pJItem, pEngineTestConfig);
    }

    if ( (pEngineTestConfig->engineConfig.type == CAM_ENGINE_CONFIG_IMAGE)
      || (pEngineTestConfig->engineConfig.type == CAM_ENGINE_CONFIG_TPG  ) ) {
        pJItem = cJSON_GetObjectItem(pJObject, "images");
        if (pJItem) {
            PAS_ParseInputImages(pJItem, pEngineTestConfig);
        }
    }

    return 0;
}

static int PAS_ParseOutput(cJSON *pJObject,
                          EngineTestConfig_t *pEngineTestConfig) {
    cJSON *pJItem = NULL;

    pJItem = cJSON_GetObjectItem(pJObject, "images");
    if (pJItem) {
        PAS_ParseOutputImages(pJItem, pEngineTestConfig);
    }

    return 0;
}

static int PAS_ParseInputGoldenFiles(cJSON *pJObject,
                           EngineTestConfig_t *pEngineTestConfig) {
    int num = cJSON_GetArraySize(pJObject);
    pEngineTestConfig->goldenFileNameWithSuffix = calloc(num, MAX_LEN);

    for (int i = 0; i < num; i++) {
        cJSON *pJItem = cJSON_GetArrayItem(pJObject, i);
        if (pJItem) {
            pJItem = cJSON_GetObjectItem(pJItem, "fileName");
            if (pJItem) {
                strncpy(&pEngineTestConfig->goldenFileNameWithSuffix[i * MAX_LEN],
                        cJSON_GetObjectString(pJItem), MAX_LEN);
            }
        }
    }

    return 0;
}

static int PAS_ParseInputImages(cJSON *pJObject,
                          EngineTestConfig_t *pEngineTestConfig) {
    int num = cJSON_GetArraySize(pJObject);

    if ( 0 < num ){
        pEngineTestConfig->imagesNumber = num;

        pEngineTestConfig->images = calloc(num, MAX_LEN);

        for (int i = 0; i < num; i++) {
            cJSON *pJItem = cJSON_GetArrayItem(pJObject, i);
            if (pJItem) {
                pJItem = cJSON_GetObjectItem(pJItem, "fileName");
                if (pJItem) {
                   strncpy(&pEngineTestConfig->images[i * MAX_LEN],
                           cJSON_GetObjectString(pJItem), MAX_LEN);
                }
            }
        }
    }

   return 0;
}

static int PAS_ParseOutputImages(cJSON *pJObject,
                        EngineTestConfig_t *pEngineTestConfig) {
    int num = cJSON_GetArraySize(pJObject);
    pEngineTestConfig->outputFileNameNoSuffix = calloc(num, MAX_LEN);

    for (int i = 0; i < num; i++) {
        cJSON *pJItem = cJSON_GetArrayItem(pJObject, i);
        if (pJItem) {
            pJItem = cJSON_GetObjectItem(pJItem, "fileName");
            if (pJItem) {
                strncpy(&pEngineTestConfig->outputFileNameNoSuffix[i * MAX_LEN],
                    cJSON_GetObjectString(pJItem), MAX_LEN);
            }
        }
    }

    return 0;
}

static void
PAS_PrintTestConfig(
    uint8_t     configIndex,
    TestCase_t *pTestCase )
{
    EngineTestConfig_t *pTestConfig = &pTestCase->config[configIndex].engine;

    TRACE(ENG_PARSE_INFO, "------------------------->>\r\n	Test configurations\r\n" );

    TRACE(ENG_PARSE_INFO, "config index: %d of %d\n", configIndex + 1, pTestCase->configCount);
    TRACE(ENG_PARSE_INFO, "json version: %s\n", pTestCase->version);
    TRACE(ENG_PARSE_INFO, "release date: %s\n\n", pTestCase->date);

    TRACE(ENG_PARSE_INFO, "engine type : %d\n\n", pTestConfig->engineConfig.type);

    TRACE(ENG_PARSE_INFO, "MP: width  = %d\r\n", pTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_MAIN].width);
    TRACE(ENG_PARSE_INFO, "MP: height = %d\r\n", pTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_MAIN].height);
    TRACE(ENG_PARSE_INFO, "MP: mode   = %d\r\n", pTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_MAIN].mode);
    TRACE(ENG_PARSE_INFO, "MP: layout = %d\r\n", pTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_MAIN].layout);

    TRACE(ENG_PARSE_INFO, "SP: width  = %d\r\n", pTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_SELF].width);
    TRACE(ENG_PARSE_INFO, "SP: height = %d\r\n", pTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_SELF].height);
    TRACE(ENG_PARSE_INFO, "SP: mode   = %d\r\n", pTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_SELF].mode);
    TRACE(ENG_PARSE_INFO, "SP: layout = %d\n\n", pTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_SELF].layout);

	TRACE(ENG_PARSE_INFO, "SP2: width  = %d\r\n", pTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_SELF2_BP].width);
    TRACE(ENG_PARSE_INFO, "SP2: height = %d\r\n", pTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_SELF2_BP].height);
    TRACE(ENG_PARSE_INFO, "SP2: mode   = %d\r\n", pTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_SELF2_BP].mode);
    TRACE(ENG_PARSE_INFO, "SP2: layout = %d\n\n", pTestConfig->engineConfig.pathConfig[CAM_ENGINE_PATH_SELF2_BP].layout);

    TRACE(ENG_PARSE_INFO, "verifyCurFrame   = %d\r\n\n", pTestConfig->verifyCurFrame);

    TRACE(ENG_PARSE_INFO, "orientation   = %d\r\n\n", pTestConfig->orientation);
    TRACE(ENG_PARSE_INFO, "enableCAC     = %d\r\n", pTestConfig->enableCAC);
    TRACE(ENG_PARSE_INFO, "disableCAC    = %d\r\n\n", pTestConfig->disableCAC);
    TRACE(ENG_PARSE_INFO, "enableDpcc    = %d\r\n", pTestConfig->enableDpcc);
    TRACE(ENG_PARSE_INFO, "disableDpcc   = %d\r\n\n", pTestConfig->disableDpcc);
    TRACE(ENG_PARSE_INFO, "enableDPF     = %d\r\n", pTestConfig->dpfConfig.enable);
    TRACE(ENG_PARSE_INFO, "disableDPF    = %d\r\n\n", !pTestConfig->dpfConfig.enable);
    TRACE(ENG_PARSE_INFO, "enableGamma   = %d\r\n", pTestConfig->enableGamma);
    TRACE(ENG_PARSE_INFO, "disableGamma  = %d\r\n\n", pTestConfig->disableGamma);
    TRACE(ENG_PARSE_INFO, "enableLSC     = %d\r\n", pTestConfig->enableLSC);
    TRACE(ENG_PARSE_INFO, "disableLSC    = %d\r\n\n", pTestConfig->disableLSC);
    TRACE(ENG_PARSE_INFO, "enableMGC     = %d\r\n", pTestConfig->enableMGC);
    TRACE(ENG_PARSE_INFO, "disableMGC    = %d\r\n\n", pTestConfig->disableMGC);
    TRACE(ENG_PARSE_INFO, "enableWDR     = %d\r\n", pTestConfig->enableWDR);
    TRACE(ENG_PARSE_INFO, "disableWDR    = %d\r\n\n", pTestConfig->disableWDR);
    TRACE(ENG_PARSE_INFO, "frameNum      = %d\r\n\n", pTestConfig->frameNum);

    TRACE(ENG_PARSE_INFO, "enableBLS     = %d\r\n", pTestConfig->enableBLS);
    TRACE(ENG_PARSE_INFO, "disableBLS    = %d\r\n", pTestConfig->disableBLS);
    TRACE(ENG_PARSE_INFO, "BLSSubMode    = %d\r\n\n", pTestConfig->BLSSubMode);

    TRACE(ENG_PARSE_INFO, "enableAWB      = %d\r\n", pTestConfig->enableAWB);
    TRACE(ENG_PARSE_INFO, "disableAWB     = %d\r\n", pTestConfig->disableAWB);
    TRACE(ENG_PARSE_INFO, "AWBMeasureMode = %d\r\n\n", pTestConfig->AWBMeasureMode);

    TRACE(ENG_PARSE_INFO, "enableEXP      = %d\r\n", pTestConfig->enableEXP);
    TRACE(ENG_PARSE_INFO, "disableEXP     = %d\r\n", pTestConfig->disableEXP);
    TRACE(ENG_PARSE_INFO, "EXPMeasureMode = %d\r\n\n", pTestConfig->EXPMeasureMode);

    TRACE(ENG_PARSE_INFO, "enableFLT     = %d\r\n", pTestConfig->enableFLT);
    TRACE(ENG_PARSE_INFO, "disableFLT    = %d\r\n", pTestConfig->disableFLT);
    TRACE(ENG_PARSE_INFO, "FLTDenoiseLevel   = %d\r\n", pTestConfig->FLTDenoiseLevel);
    TRACE(ENG_PARSE_INFO, "FLTShrpenLevel    = %d\r\n", pTestConfig->FLTShrpenLevel);
    TRACE(ENG_PARSE_INFO, "enableGammaOut    = %d\r\n", pTestConfig->enableGammaOut);
    TRACE(ENG_PARSE_INFO, "disableGammaOut   = %d\r\n", pTestConfig->disableGammaOut);
    TRACE(ENG_PARSE_INFO, "enableHIST    = %d\r\n", pTestConfig->enableHIST);
    TRACE(ENG_PARSE_INFO, "disableHIST   = %d\r\n", pTestConfig->disableHIST);
    TRACE(ENG_PARSE_INFO, "enableIS      = %d\r\n", pTestConfig->enableIS);
    TRACE(ENG_PARSE_INFO, "disableIS     = %d\r\n", pTestConfig->disableIS);
#ifdef ISP_RAWIS
	TRACE(ENG_PARSE_INFO, "enableRawIS 	 = %d\r\n", pTestConfig->enableRawIS);
	TRACE(ENG_PARSE_INFO, "disableRawIS	 = %d\r\n", pTestConfig->disableRawIS);
#endif
    TRACE(ENG_PARSE_INFO, "enableSIMP    = %d\r\n", pTestConfig->enableSIMP);
    TRACE(ENG_PARSE_INFO, "disableSIMP   = %d\r\n\n", pTestConfig->disableSIMP);

    TRACE(ENG_PARSE_INFO, "IEWorkingMode = %d\r\n", pTestConfig->ieConfig.mode);
    TRACE(ENG_PARSE_INFO, "IERange = %d\r\n", pTestConfig->ieConfig.range);

    TRACE(ENG_PARSE_INFO, "enableCPROC   = %d\r\n", pTestConfig->enableCPROC);
    TRACE(ENG_PARSE_INFO, "disableCPROC  = %d\r\n\n", pTestConfig->disableCPROC);

    TRACE(ENG_PARSE_INFO, "enableCNR       = %d\r\n", pTestConfig->enableCNR);
    TRACE(ENG_PARSE_INFO, "disableCNR      = %d\r\n", pTestConfig->disableCNR);
	TRACE(ENG_PARSE_INFO, "enableCSM       = %d\r\n", pTestConfig->enableCSM);
    TRACE(ENG_PARSE_INFO, "cnrLineWidth    = %d\r\n", pTestConfig->cnrLineWidth);
    TRACE(ENG_PARSE_INFO, "cnrThreshold1   = %d\r\n", pTestConfig->cnrThreshold1);
    TRACE(ENG_PARSE_INFO, "cnrThreshold2   = %d\r\n\n", pTestConfig->cnrThreshold2);

    TRACE(ENG_PARSE_INFO, "enableWDR2   = %d\r\n", pTestConfig->enableWDR2);
    TRACE(ENG_PARSE_INFO, "WDR2Strength = %2.2f\r\n\n", pTestConfig->WDR2Strength);
    TRACE(ENG_PARSE_INFO, "enableDual = %d\r\n", pTestConfig->enableDual);

    TRACE(ENG_PARSE_INFO, "enableWDR3          = %d\r\n", pTestConfig->enableWDR3);
    TRACE(ENG_PARSE_INFO, "WDR3Strength        = %d\r\n", pTestConfig->Wdr3Strength);
    TRACE(ENG_PARSE_INFO, "Wdr3MaxGain         = %d\r\n", pTestConfig->Wdr3MaxGain);
    TRACE(ENG_PARSE_INFO, "Wdr3GlobalStrength  = %d\r\n\n", pTestConfig->Wdr3GlobalStrength);

    TRACE(ENG_PARSE_INFO, "enableCmpd       = %d\r\n", pTestConfig->enableCmpd);
    TRACE(ENG_PARSE_INFO, "CmpdBlsEnable    = %d\r\n", pTestConfig->cmpdBlsEnable);
    TRACE(ENG_PARSE_INFO, "compressEnable   = %d\r\n", pTestConfig->compressEnable);
    TRACE(ENG_PARSE_INFO, "expandEnable     = %d\r\n\n", pTestConfig->expandEnable);

    TRACE(ENG_PARSE_INFO, "enableStitching       = %d\r\n", pTestConfig->enableStitching);
    TRACE(ENG_PARSE_INFO, "disableStitching      = %d\r\n", pTestConfig->disableStitching);
    TRACE(ENG_PARSE_INFO, "StitchingPara         = %d\r\n", pTestConfig->StitchingPara);

    TRACE(ENG_PARSE_INFO, "enableAev2       = %d\r\n", pTestConfig->enableAev2);
    TRACE(ENG_PARSE_INFO, "Aev2 width      = %d\r\n", pTestConfig->AEV2Config.width);
	TRACE(ENG_PARSE_INFO, "Aev2 height       = %d\r\n", pTestConfig->AEV2Config.height);
    TRACE(ENG_PARSE_INFO, "Aev2 vOffset    = %d\r\n", pTestConfig->AEV2Config.vOffset);
    TRACE(ENG_PARSE_INFO, "Aev2 hOffset    = %d\r\n", pTestConfig->AEV2Config.hOffset);
    TRACE(ENG_PARSE_INFO, "Aev2 r weight   = %d\r\n\n", pTestConfig->AEV2Config.rWeight);
    TRACE(ENG_PARSE_INFO, "Aev2 gr weight         = %d\r\n", pTestConfig->AEV2Config.grWeight);
    TRACE(ENG_PARSE_INFO, "Aev2 gb weight   = %d\r\n", pTestConfig->AEV2Config.gbWeight);
    TRACE(ENG_PARSE_INFO, "Aev2 b weight  = %d\r\n", pTestConfig->AEV2Config.bWeight);

    TRACE(ENG_PARSE_INFO, "enableEe         = %d\r\n", pTestConfig->EeConfig.enabled);
    TRACE(ENG_PARSE_INFO, "inputDataFormatSelect    = %d\r\n", pTestConfig->EeConfig.inputDataFormatSelect);
    TRACE(ENG_PARSE_INFO, "srcStrength  = %d\r\n", pTestConfig->EeConfig.srcStrength);
    TRACE(ENG_PARSE_INFO, "strength   = %d\r\n", pTestConfig->EeConfig.strength);
    TRACE(ENG_PARSE_INFO, "isp_ee_y up gain     = %d\r\n", pTestConfig->EeConfig.yUpGain);
    TRACE(ENG_PARSE_INFO, "isp_ee_y down gain     = %d\r\n", pTestConfig->EeConfig.yDownGain);

    TRACE(ENG_PARSE_INFO, "uvGain     = %d\r\n", pTestConfig->EeConfig.uvGain);
    TRACE(ENG_PARSE_INFO, "edgeGain     = %d\r\n", pTestConfig->EeConfig.edgeGain);
    TRACE(ENG_PARSE_INFO, "enable2DNR       = %d\r\n", pTestConfig->enable2DNR);
    TRACE(ENG_PARSE_INFO, "disable2DNR       = %d\r\n", pTestConfig->disable2DNR);
    TRACE(ENG_PARSE_INFO, "denoise2DStren      = %d\r\n", pTestConfig->denoise2DStren);

    TRACE(ENG_PARSE_INFO, "enableAfm       = %d\r\n", pTestConfig->enableAfm);
    TRACE(ENG_PARSE_INFO, "disableAfm      = %d\r\n", pTestConfig->disableAfm);
    TRACE(ENG_PARSE_INFO, "AfmWindowId         = %d\r\n", pTestConfig->AfmWindowId);
    TRACE(ENG_PARSE_INFO, "AfmThreshold         = %d\r\n", pTestConfig->AfmThreshold);

    TRACE(ENG_PARSE_INFO, "enableVsm       = %d\r\n", pTestConfig->enableVsm);
    TRACE(ENG_PARSE_INFO, "vsmWinHoffset       = %d\r\n", pTestConfig->vsmWinHoffset);
    TRACE(ENG_PARSE_INFO, "vsmWinWoffset       = %d\r\n", pTestConfig->vsmWinVoffset);
    TRACE(ENG_PARSE_INFO, "vsmWinHeight       = %d\r\n", pTestConfig->vsmWinHeight);
    TRACE(ENG_PARSE_INFO, "vsmWinWidth       = %d\r\n", pTestConfig->vsmWinWidth);

#ifdef ISP_GREENEQUILIBRATE
    TRACE(ENG_PARSE_INFO, "enableGreenEquilibration       = %d\r\n", pTestConfig->enableGrnEqlr);
    TRACE(ENG_PARSE_INFO, "greenEquilibrationTh       = %d\r\n", pTestConfig->grnEqlrTh);
    TRACE(ENG_PARSE_INFO, "greenEquilibrationHcntDmy       = %d\r\n", pTestConfig->grnEqlrHcntDmy);
#endif
#ifdef ISP_DEMOSAIC2
    TRACE(ENG_PARSE_INFO, "DemosaicEnable       = %d\r\n", pTestConfig->dmsc2.enable);
    TRACE(ENG_PARSE_INFO, "DemosaicNoiseStren         = %d\r\n", pTestConfig->dmsc2.denoise_stren);

    TRACE(ENG_PARSE_INFO, "enable flt       = %d\r\n", pTestConfig->flt.enable);

#endif
#ifdef ISP_CA
        TRACE(ENG_PARSE_INFO, "enableCa       = %d\r\n", pTestConfig->enableCa);
        TRACE(ENG_PARSE_INFO, "CaMode         = %d\r\n", pTestConfig->caMode);

#endif

    TRACE(ENG_PARSE_INFO, "DemosaicMode         = %d\r\n", pTestConfig->DemosaicMode);
    TRACE(ENG_PARSE_INFO, "DemosaicThreshold         = %d\r\n", pTestConfig->DemosaicThreshold);
    TRACE(ENG_PARSE_INFO, "enableDenoise3d     = %d\r\n", pTestConfig->denoise3dEnable);
    TRACE(ENG_PARSE_INFO, "denoise3dStrength   = %d\r\n", pTestConfig->denoise3dStrength);
#ifdef ISP_DEC
	TRACE(ENG_PARSE_INFO, "enableDec	   = %d\r\n", pTestConfig->enableDec);
#endif
#ifdef ISP_DPF_RAW
    TRACE(ENG_PARSE_INFO, "dpf_raw_mode      = %d\r\n\n", pTestConfig->dpf_raw_mode);
#endif
#ifdef ISP_MI_FIFO_DEPTH_NANO
    TRACE(ENG_PARSE_INFO, "mi_mp_fifo_depth_en  = %d\r\n", pTestConfig->mi_mp_fifo_depth_en);
    TRACE(ENG_PARSE_INFO, "mi_mp_fifo_depth     = %d\r\n\n", pTestConfig->mi_mp_fifo_depth);
#endif
#ifdef ISP_MI_ALIGN_NANO
    TRACE(ENG_PARSE_INFO, "mi_mp_lsb_align   = %d\r\n", pTestConfig->mi_mp_lsb_align);
#endif
#ifdef ISP_MI_BYTESWAP
    TRACE(ENG_PARSE_INFO, "mi_mp_byte_swap   = %d\r\n\n", pTestConfig->mi_mp_byte_swap);
#endif
#ifdef ISP_DVP_PINMAPPING
    TRACE(ENG_PARSE_INFO, "dvp_input_pin_mapping_en = %d\r\n", pTestConfig->dvp_input_pin_mapping_en);
    TRACE(ENG_PARSE_INFO, "dvp_input_pin_mapping    = %d\r\n\n", pTestConfig->dvp_input_pin_mapping);
#endif
#ifdef ISP_MI_HANDSHAKE_NANO
    TRACE(ENG_PARSE_INFO, "mi_mp_handshake.handshk_switch    = %d\r\n", pTestConfig->mi_mp_handshake.handshk_switch );
    TRACE(ENG_PARSE_INFO, "mi_mp_handshake.storage_fmt       = %d\r\n", pTestConfig->mi_mp_handshake.storage_fmt );
    TRACE(ENG_PARSE_INFO, "mi_mp_handshake.data_fmt          = %d\r\n", pTestConfig->mi_mp_handshake.data_fmt );
    TRACE(ENG_PARSE_INFO, "mi_mp_handshake.line_per_slice_minus1     = %d\r\n", pTestConfig->mi_mp_handshake.line_per_slice_minus1 );
    TRACE(ENG_PARSE_INFO, "mi_mp_handshake.slice_per_buffer_minus1   = %d\r\n", pTestConfig->mi_mp_handshake.slice_per_buffer_minus1 );
    TRACE(ENG_PARSE_INFO, "mi_mp_handshake.ack_count                 = %d\r\n", pTestConfig->mi_mp_handshake.ack_count );
    TRACE(ENG_PARSE_INFO, "mi_mp_handshake.mp_y_llength_pixel        = %d\r\n", pTestConfig->mi_mp_handshake.mp_y_llength_pixel );
    TRACE(ENG_PARSE_INFO, "mi_mp_handshake.mp_y_slice_offset_bytes   = %d\r\n", pTestConfig->mi_mp_handshake.mp_y_slice_offset_bytes );
    TRACE(ENG_PARSE_INFO, "mi_mp_handshake.mp_c_slice_offset_bytes   = %d\r\n", pTestConfig->mi_mp_handshake.mp_c_slice_offset_bytes );
    TRACE(ENG_PARSE_INFO, "mi_mp_handshake.mp_y_pic_width_pixel      = %d\r\n", pTestConfig->mi_mp_handshake.mp_y_pic_width_pixel );
    TRACE(ENG_PARSE_INFO, "mi_mp_handshake.mp_y_pic_height_pixel     = %d\r\n\n", pTestConfig->mi_mp_handshake.mp_y_pic_height_pixel );
#endif
#ifdef ISP_GCMONO
    TRACE(ENG_PARSE_INFO, "gcmono_cxt.enabled                = %d\r\n", pTestConfig->gcmono_cxt.enabled);
#endif
#ifdef ISP8000NANO_BASE
    TRACE(ENG_PARSE_INFO, "enableFieldSelection              = %d\r\n", pTestConfig->enableFieldSelection);
    TRACE(ENG_PARSE_INFO, "field_selection                   = %d\r\n\n", pTestConfig->field_selection);
#endif  //#ifdef ISP8000NANO_BASE

    TRACE(ENG_PARSE_INFO, "input:\r\n");
    TRACE(ENG_PARSE_INFO, "goldenFileName  = %s\r\n", pTestConfig->goldenFileNameWithSuffix);
    TRACE(ENG_PARSE_INFO, "images 	      = %s\r\n", pTestConfig->images);

    TRACE(ENG_PARSE_INFO, "output:\r\n");
    TRACE(ENG_PARSE_INFO, "fileName  = %s\r\n", pTestConfig->outputFileNameNoSuffix);

    TRACE(ENG_PARSE_INFO, "------------------------->>\r\n\r\n" );
    (void)pTestConfig; //avoid compiler's complaint
}


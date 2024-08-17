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

#include "DewarpConfigParser.h"

#include <assert.h>
#include <fstream>
#include <iostream>
#include "MediaBuffer.h"
#include "log.h"

#define LOGTAG "DewarpConfigParser"

const std::map<std::string, int> dewarp_mode_map{
    { "LENS_CORRECTION",  DEWARP_MODEL_LENS_DISTORTION_CORRECTION },
    { "FISHEYE_EXPAND",  DEWARP_MODEL_FISHEYE_EXPAND },
    { "SPLIT_SCREEN",  DEWARP_MODEL_SPLIT_SCREEN},
    { "FISHEYE_DEWARP", DEWARP_MODEL_FISHEYE_DEWARP},
};

static bool readChildNodeAsDouble(const char* groupName,
                                 const Json::Value& parentNode,
                                 const char* childName,
                                 double* value) {
    assert(value);

    Json::Value childNode = parentNode[childName];
    if (!childNode.isNumeric()) {
        ALOGW("Missing or invalid field %s in record %s", childName, groupName);
        return false;
    }

    *value = childNode.asDouble();
    return true;
}

static bool readChildNodeAsInt(const char* groupName,
                                 const Json::Value& parentNode,
                                 const char* childName,
                                 uint32_t* value) {
    assert(value);

    Json::Value childNode = parentNode[childName];
    if (!childNode.isNumeric()) {
        ALOGW("Missing or invalid field %s in record %s", childName, groupName);
        return false;
    }

    *value = childNode.asInt();
    return true;
}

void loadUserMap(const char* pFileName, std::vector<uint32_t>& userMap) {
    FILE* pFile = fopen(pFileName, "r");
    if (!pFile) {
        ALOGE("can't open file: %s", pFileName);
        return;
    }
    userMap.clear();
    char sz[1024];
    uint32_t val;
    while (fgets(sz, 1024, pFile)) {
        std::string str = sz;
        sscanf(sz, "%08x", &val);
        userMap.push_back(val);
    }
    fclose(pFile);
}

bool dweParseJsonNode(Json::Value& rootNode, std::vector<uint32_t>& usermap, dewarp_distortion_map* dmap, dewarp_parameters& params) {
    int dewarpMode = 2;
    double scale_factor = 1.0;
    {
        Json::Value node = rootNode["dewarpMode"];
        const char* szNode = node.asCString();
        auto iter = dewarp_mode_map.find(szNode);
        if (iter == dewarp_mode_map.end()) {
            ALOGE("unsupported dewarpMode %s\n", szNode);
            return false;
        }
        dewarpMode = iter->second;
    }
    {
        Json::Value node = rootNode["scale"];
        readChildNodeAsInt("scale", node, "roix",       &params.roi_start.width);
        readChildNodeAsInt("scale", node, "roiy",       &params.roi_start.height);
        readChildNodeAsDouble("scale", node, "factor",  &scale_factor);
        params.scale_factor = (scale_factor * 4096);
    }
    {
        Json::Value node = rootNode["split"];
        readChildNodeAsInt("split", node, "horizon_line",          &params.split_horizon_line);
        readChildNodeAsInt("split", node, "vertical_line_up",      &params.split_vertical_line_up);
        readChildNodeAsInt("split", node, "vertical_line_down",    &params.split_vertical_line_down);
    }
    {
        Json::Value node = rootNode["hflip"];
        params.hflip = node.asBool();
        node = rootNode["vflip"];
        params.vflip = node.asBool();
        node = rootNode["bypass"];
        params.bypass = node.asBool();
    }
    {
        Json::Value node = rootNode["camera_matrix"];
        if (node.isArray()) {
            for (uint32_t i = 0; i < node.size(); i++) {
                dmap[0].camera_matrix[i] = node[i].asDouble();
                ALOGD("camera matrix %f", dmap[0].camera_matrix[i]);
            }
        }
    }
    {
        Json::Value node = rootNode["distortion_coeff"];
        if (node.isArray()) {
            for (uint32_t i = 0; i < node.size(); i++) {
                dmap[0].distortion_coeff[i] = node[i].asDouble();
                ALOGD("distortion_coeff %f", dmap[0].distortion_coeff[i]);
            }
        }
    }
    {
        Json::Value node = rootNode["perspective"];
        if (node.isArray()) {
            for (uint32_t i = 0; i < node.size(); i++) {
                dmap[0].perspective_matrix[i] = node[i].asDouble();
                ALOGD("perspective_matrix %f", dmap[0].perspective_matrix[i]);
            }
        }
    }
    {
        Json::Value node = rootNode["camera_matrix2"];
        if (node.isArray()) {
            for (uint32_t i = 0; i < node.size(); i++) {
                dmap[1].camera_matrix[i] = node[i].asDouble();
                ALOGD("camera matrix2 %f", dmap[1].camera_matrix[i]);
            }
        }
    }
    {
        Json::Value node = rootNode["distortion_coeff2"];
        if (node.isArray()) {
            for (uint32_t i = 0; i < node.size(); i++) {
                dmap[1].distortion_coeff[i] = node[i].asDouble();
                ALOGD("distortion_coeff2 %f", dmap[1].distortion_coeff[i]);
            }
        }
    }
    {
        Json::Value node = rootNode["perspective2"];
        if (node.isArray()) {
            for (uint32_t i = 0; i < node.size(); i++) {
                dmap[1].perspective_matrix[i] = node[i].asDouble();
                ALOGD("perspective_matrix2 %f", dmap[1].perspective_matrix[i]);
            }
        }
    }
    {
        Json::Value node = rootNode["userMap"];
        if (node.isString()) {
            std::string str = node.asCString();
            ALOGD("userMap %s", str.c_str());
            if (!str.empty()) {
                loadUserMap(str.c_str(), usermap);
                dmap[0].userMapSize = usermap.size();
                dmap[0].pUserMap = &usermap[0];
            }
        }
        node = rootNode["userMap2"];
        if (node.isString()) {
            std::string str = node.asCString();
            ALOGD("userMap2 %s", str.c_str());
            if (!str.empty()) {
                loadUserMap(str.c_str(), usermap);
                dmap[1].userMapSize = usermap.size();
                dmap[1].pUserMap = &usermap[0];
            }
        }
    }
    params.boundary_pixel.Y = 0;
    params.boundary_pixel.U = 128;
    params.boundary_pixel.V = 128;
    params.dewarp_type = dewarpMode;
    return true;
}

bool dweJsonFileLoadParams(const char *pFileName,struct dewarp_json_params_array *parray)
{
    std::string strInputFileName = pFileName;
    std::ifstream configStream(strInputFileName);
    Json::Reader reader;
    Json::Value rootNode;

    bool parseOk = reader.parse(configStream, rootNode, false);
    if (!parseOk) {
        ALOGE("Failed to read configuration file %s", pFileName);
        ALOGE("%s", reader.getFormatedErrorMessages().c_str());
        return false;
    }

    Json::Value array_node = rootNode["dewarpConfigArray"];
    parray->count = array_node.size();

    for (uint32_t i=0; i<parray->count; i++)
    {
        Json::Value config_node                   = array_node[i];
        struct dewarp_parameters     *pParams     = &(parray->dewarp_config[i].params);
        struct dewarp_distortion_map *pDewarpMaps = &(parray->dewarp_config[i].dmap);


        Json::Value source_node    = config_node["source_image"];
        pParams->image_size.width  = source_node["width"].asUInt();
        pParams->image_size.height = source_node["height"].asUInt();

        Json::Value type_node      = config_node["dewarpType"];
        const char* stringType     = type_node.asCString();
        auto iter = dewarp_mode_map.find(stringType);
        if (iter == dewarp_mode_map.end()) 
        {
            ALOGE("unsupported dewarpType %s\n", stringType);
            return false;
        }
        pParams->dewarp_type = iter->second;

        Json::Value scale_node     = config_node["scale"];
        pParams->roi_start.width   = scale_node["roix"].asUInt();
        pParams->roi_start.height  = scale_node["roiy"].asUInt();
        pParams->scale_factor      = scale_node["factor"].asDouble() * 4096;

        Json::Value split_node            = config_node["split"];
        pParams->split_horizon_line       = split_node["horizon_line"].asUInt();
        pParams->split_vertical_line_up   = split_node["vertical_line_up"].asUInt();
        pParams->split_vertical_line_down = split_node["vertical_line_down"].asUInt();

        pParams->bypass                   = config_node["bypass"].asBool();
        pParams->hflip                    = config_node["hflip"].asBool();
        pParams->vflip                    = config_node["vflip"].asBool();

        pParams->boundary_pixel.Y         = 0;
        pParams->boundary_pixel.U         = 128;
        pParams->boundary_pixel.V         = 128;

        Json::Value matrix_node = config_node["camera_matrix"];
        for (unsigned int index=0; index < matrix_node.size(); index++)
        {
            pDewarpMaps->camera_matrix[index] = matrix_node[index].asDouble();
        }

        Json::Value coeff_node = config_node["distortion_coeff"];
        for (unsigned int index=0; index < coeff_node.size(); index++)
        {
            pDewarpMaps->distortion_coeff[index] = coeff_node[index].asDouble();
        }

        Json::Value per_matrix_node = config_node["perspective"];
        for (unsigned int index=0; index < per_matrix_node.size(); index++)
        {
            pDewarpMaps->perspective_matrix[index] = per_matrix_node[index].asDouble();
        }

        Json::Value userMap_node = config_node["userMap"];
        if (userMap_node.isString())
        {
            std::vector<uint32_t> userMap;
            std::string str = userMap_node.asCString();
            ALOGD("userMap %s", str.c_str());

            if (!str.empty())
            {
                loadUserMap(str.c_str(), userMap);
                pDewarpMaps->userMapSize = userMap.size();
                pDewarpMaps->pUserMap = &userMap[0];
            }
        }

    }
   
    return true;
}

bool parseJsonFile(const char* pFileName, std::string& inputImageFile, std::string& outputImageFile,
        std::vector<uint32_t>& usermap, dewarp_distortion_map* dmap, dewarp_parameters& params,
        uint64_t& goldenHashKey, std::string& hashFile) {
    std::string strInputFileName = pFileName;
    std::ifstream configStream(strInputFileName);
    Json::Reader reader;
    Json::Value rootNode;

    int inputFormat, outputFormat;
    uint32_t src_w = 0;
    uint32_t src_h = 0;

    bool parseOk = reader.parse(configStream, rootNode, false /* don't need comments */);
    if (!parseOk) {
        ALOGE("Failed to read configuration file %s", pFileName);
        ALOGE("%s", reader.getFormatedErrorMessages().c_str());
        return false;
    }
    if (!dweParseJsonNode(rootNode, usermap, dmap, params)) {
        return false;
    }
    {
        Json::Value node = rootNode["input"];
        Json::Value nameNode = node.get("file", "null");
        inputImageFile = nameNode.asCString();

        int pos = strInputFileName.rfind("/") + 1;
        outputImageFile = strInputFileName.substr(pos, strInputFileName.rfind(".") - pos);
        hashFile = outputImageFile + "_verifyHash.txt";
        outputImageFile += "_output.yuv";
        ALOGI("inputfile: %s output %s", inputImageFile.c_str(), outputImageFile.c_str());
        nameNode = node.get("format", "YUV420SP");
        const char* szNode = nameNode.asCString();
        auto iter = mediaFormatStringTable.find(szNode);
        if (iter == mediaFormatStringTable.end()) {
            ALOGE("unsupported input format %s", szNode);
            return false;
        }
        inputFormat = iter->second;
        readChildNodeAsInt("input", node, "width",       &src_w);
        readChildNodeAsInt("input", node, "height",      &src_h);
        params.image_size.width = src_w;
        params.image_size.height = src_h;
    }
    {
        Json::Value node = rootNode["output"];
        Json::Value nameNode = node.get("format", "YUV420SP");
        const char* szNode = nameNode.asCString();
        auto iter = mediaFormatStringTable.find(szNode);
        if (iter == mediaFormatStringTable.end()) {
            ALOGE("unsupported output format %s", szNode);
            return false;
        }
        outputFormat = iter->second;
        readChildNodeAsInt("output", node, "width",       &params.image_size_dst.width);
        readChildNodeAsInt("output", node, "height",      &params.image_size_dst.height);
    }
    {
        Json::Value node = rootNode["goldenHashKey"];
        if (node.isString()) {
            std::string str = node.asCString();
            ALOGI("goldenHashKey %s", str.c_str());
            if (!str.empty())
                goldenHashKey = stoull(str);
        }
    }
    params.pix_fmt_in = inputFormat;
    params.pix_fmt_out = outputFormat;
    params.image_size.width = src_w;
    params.image_size.height = src_h;
    configStream.close();
    return true;
}

void updateJsonHash(const char* pFileName, const char* hash) {
    std::ifstream configStream(pFileName);
    Json::Reader reader;
    Json::Value rootNode;
    bool parseOk = reader.parse(configStream, rootNode, false);
    if (!parseOk) {
        ALOGE("Failed to read configuration file %s", pFileName);
        ALOGE("%s", reader.getFormatedErrorMessages().c_str());
        return;
    }
    configStream.close();
    Json::StyledStreamWriter sw;
    rootNode["goldenHashKey"] = hash;
    std::ofstream os;
    os.open(pFileName);
    sw.write(os, rootNode);
    os.close();
}

/****************************************************************************
*
*    Copyright 2019 - 2020 VeriSilicon Inc. All Rights Reserved.
*
*    Permission is hereby granted, free of charge, to any person obtaining
*    a copy of this software and associated documentation files (the
*    'Software'), to deal in the Software without restriction, including
*    without limitation the rights to use, copy, modify, merge, publish,
*    distribute, sub license, and/or sell copies of the Software, and to
*    permit persons to whom the Software is furnished to do so, subject
*    to the following conditions:
*
*    The above copyright notice and this permission notice (including the
*    next paragraph) shall be included in all copies or substantial
*    portions of the Software.
*
*    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
*    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
*    IN NO EVENT SHALL VIVANTE AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
*    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
*    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/

#ifndef VSI_DEC_DPB_BUFF_H
#define VSI_DEC_DPB_BUFF_H

#include "vsi_dec.h"

void dpb_init(v4l2_dec_inst* h);
void dpb_destroy(v4l2_dec_inst* h);
void dpb_receive_buffer(v4l2_dec_inst* h, uint32_t buf_idx,
                        v4l2_daemon_dec_buffers* buf);
int32_t dpb_find_buffer(v4l2_dec_inst* h, addr_t bus_addr);
struct v4l2_decoder_dbp* dpb_list_buffer(v4l2_dec_inst* h, uint32_t id);
struct DWLLinearMem* dpb_get_buffer(v4l2_dec_inst* h, uint32_t id);
void dpb_get_buffers(v4l2_dec_inst* h);
int32_t dpb_render_buffer(v4l2_dec_inst* h, vsi_v4l2_dec_picture* pic);
void dpb_refresh_all_buffers(v4l2_dec_inst* h);

#endif  // VSI_DEC_DPB_BUFF_H

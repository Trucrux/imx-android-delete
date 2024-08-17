/****************************************************************************
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 VeriSilicon Holdings Co., Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************/
#include "ioctl/v4l2-ioctl.hpp"
#include "common/macros.hpp"
#include "network/file-server.hpp"
#include "display/display-event.hpp"
#include <signal.h>

static int is_non_digit(const char* str)
{
    const char* temp = str;

    while (temp != NULL && *temp != '\0') {
        if (*temp < '0' || *temp > '9') {
            return 1;
        }
        temp++;
    }

    return 0;
}

int main(int argc, char* argv[])
{
    int videoId = 0;

    if (argc > 2) {
        ALOGE("usage: ./tuningext videoid");
        ALOGE("e.g: ./tuningext 2");
        return 1;
    } else if (argc == 2) {
        if (is_non_digit(argv[1])) {
            ALOGE("wrong videoid data type");
            ALOGE("usage: ./tuningext videoid");
            return 1;
        } else {
            videoId = atoi(argv[1]);
        }
    } else {
        videoId = 2;
    }

    pDisplayEvent = new DisplayEvent(videoId);
    DCT_ASSERT(pDisplayEvent);

    FileServer *pFileServer = new FileServer();
    DCT_ASSERT(pFileServer);

    if (pDisplayEvent) {
        delete pDisplayEvent;
    }

    if (pFileServer) {
        delete pFileServer;
    }

    return 0;
}

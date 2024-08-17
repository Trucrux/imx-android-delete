#ifdef WAYLAND_SUPPORT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <imx/linux/dma-buf.h>
#include <linux/videodev2.h>
#include <linux/dma-heap.h>
#include <g2dExt.h>

#include "log.h"
#define LOGTAG "WlDisplay"
#include "WlDisplay.h"

static void handle_ping(void *data, struct wl_shell_surface *shell_surface,
							uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
}

static void handle_configure(void *data, struct wl_shell_surface *shell_surface,
		 uint32_t edges, int32_t width, int32_t height)
{
}

static void handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
	handle_ping,
	handle_configure,
	handle_popup_done
};

static void xdg_wm_base_ping( void *data, struct xdg_wm_base *shell, uint32_t serial)
{
    xdg_wm_base_pong(shell, serial);    
}

static const struct xdg_wm_base_listener wm_base_listener {
    xdg_wm_base_ping,
};

static void registry_handle_global(void *data, struct wl_registry *registry,
                       uint32_t id, const char *interface, uint32_t version)
{
	struct display *d = (struct display *)data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor = (struct wl_compositor*)wl_registry_bind(registry,
			                 id, &wl_compositor_interface, 1);
	} else if (strcmp(interface, "xdg_wm_base") == 0) {
		d->wm_base = (struct xdg_wm_base*)wl_registry_bind(registry,
					      id, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(d->wm_base, &wm_base_listener, d);
	} else if (strcmp(interface, "zwp_linux_dmabuf_v1") == 0) {
		d->dmabuf = (struct zwp_linux_dmabuf_v1*)wl_registry_bind(registry,
		                             id, &zwp_linux_dmabuf_v1_interface, 3);
	} else if (strcmp(interface, "wp_viewporter") == 0) {
		d->viewporter = (struct wp_viewporter *)wl_registry_bind(registry, id,
						 &wp_viewporter_interface, 1);
	}
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry,
                              uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

static struct display *create_display()
{
    struct display *pdisplay;

    pdisplay = (struct display *) malloc(sizeof(struct display));
	memset(pdisplay, 0, sizeof(struct display));
    if (pdisplay == NULL) {
        ALOGE("malloc display failed");
        return NULL;
    }

    pdisplay->display = wl_display_connect(NULL);
    if (!(pdisplay->display)) {
        ALOGE("Can't connect to display\n");
        return NULL;
    }

    pdisplay->registry = wl_display_get_registry(pdisplay->display);
	wl_registry_add_listener(pdisplay->registry,
	                         &registry_listener, pdisplay);
    
    wl_display_roundtrip(pdisplay->display);
    
    if (pdisplay->dmabuf == NULL) {
        ALOGE("Can't find zwp_linux_dmabuf\n");
        return NULL;
    }

	if (pdisplay->compositor == NULL) {
        ALOGE("Can't find compositor\n");
        return NULL;
    }

    return pdisplay;
}

static void xdg_surface_handle_config(void *data, struct xdg_surface *surface, uint32_t serial)
{
    xdg_surface_ack_configure(surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener {
    xdg_surface_handle_config,
};

static struct window *create_window(struct display *pdisplay)
{
    struct window *pwindow;
    pwindow = (struct window *) malloc(sizeof(struct window));
	memset(pwindow, 0, sizeof(struct window));

    assert(pdisplay);
    pwindow->display = pdisplay;

    pwindow->surface = wl_compositor_create_surface(pdisplay->compositor);

	if (pdisplay->wm_base) {
		pwindow->viewport = wp_viewporter_get_viewport(pdisplay->viewporter,
							      pwindow->surface);

		pwindow->xdg_surface = xdg_wm_base_get_xdg_surface(pdisplay->wm_base,
						    pwindow->surface);

		assert(pwindow->xdg_surface);

		xdg_surface_add_listener(pwindow->xdg_surface,
					 &xdg_surface_listener, pwindow);

		pwindow->xdg_toplevel = xdg_surface_get_toplevel(pwindow->xdg_surface);

		assert(pwindow->xdg_toplevel);
		wl_surface_commit(pwindow->surface);	
		wl_display_roundtrip(pdisplay->display);
	} else {
        ALOGE("Get xdg_shm_base failed");
    }
    return pwindow;
}

static int get_index(struct window *pwindow, unsigned long addr)
{
    struct dma_map *map;
    for (int i = 0; i < pwindow->num_bufs; i++) {
        map = &pwindow->maps[i];
        if (map->dma_phys == addr) {
            return map->index;
        }
    }
    return -1;
}

static void g2d_format_convert(void *g2d_handle, const unsigned long src_addr, struct buffer *dst_buffer,
                                const g2d_format src_fmt, const g2d_format dst_fmt, uint32_t width, uint32_t height)
{
    struct g2d_surfaceEx src, dst;
    if (g2d_handle == NULL ) {
        ALOGE("G2D is not opened!");
        return;
    }

    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));

    src.base.right = width;
    src.base.bottom = height;
    src.base.width = width;
    src.base.height = height;
    src.base.stride = src.base.width;
    src.base.format = src_fmt;
    src.base.planes[0] = src_addr;
    src.base.planes[1] = src_addr + width * height;

    dst.base.right = dst_buffer->width;
    dst.base.bottom = dst_buffer->height;
    dst.base.width = dst_buffer->width;
    dst.base.height = dst_buffer->height;
    dst.base.stride = dst.base.width;
    dst.base.format = dst_fmt;
    dst.base.planes[0] = dst_buffer->paddr;

    g2d_blitEx(g2d_handle, &src, &dst);
    g2d_finish(g2d_handle);
}

void WlDisplay::showBufferExt(unsigned char* yuvData, unsigned long buf_addr,
                    int width, int height, int format, int size) {

    struct buffer *pbuffer;
    int index;
    index = get_index(window, buf_addr);
    if (index < 0) {
        ALOGE("buf_addr not in dma_map");
        return;
    }

    /*avoid use busy buffer*/
    while (window->buffers[index].busy) {}

    pbuffer = &window->buffers[index];
    if (window->display->format.format == V4L2_PIX_FMT_NV16) {
        if (g2d_handle == NULL && g2d_open(&g2d_handle) == -1) {
            ALOGE("Failed to open g2d!");
            return;
        }
        g2d_format_convert(g2d_handle, buf_addr , pbuffer, G2D_NV16, G2D_YUYV, width, height); 
    }

    wl_surface_attach(window->surface, pbuffer->buf, 0, 0);
	wl_surface_damage(window->surface, 0, 0,
	                  window->display->format.width,
	                  window->display->format.height);
    if (window->display->format.width < 640 || window->display->format.width < 480) {
        wp_viewport_set_destination(window->viewport, width, height);
    } else {
        wp_viewport_set_destination(window->viewport, 640, 480);
    }
    wl_surface_commit(window->surface);
    wl_display_flush(window->display->display);
    wl_display_dispatch(window->display->display);
    pbuffer->busy = 1;
}

void WlDisplay::showBuffer(unsigned char* data, int width, int height, int format, int size)
{
    
}

static void buffer_release(void *data, struct wl_buffer *buffer)
{
	struct buffer *pbuffer = (struct buffer *)data;
	pbuffer->busy = 0;
}

static const struct wl_buffer_listener buffer_listener = {
	buffer_release
};

static void create_succeeded(void *data,
		 struct zwp_linux_buffer_params_v1 *params,
		 struct wl_buffer *new_buffer)
{
	struct buffer *pbuffer = (struct buffer *)data;
	pbuffer->buf = new_buffer;
    wl_buffer_add_listener(pbuffer->buf, &buffer_listener, pbuffer);
	zwp_linux_buffer_params_v1_destroy(params);
	close(pbuffer->dma_fd);
}

static void create_failed(void *data, struct zwp_linux_buffer_params_v1 *params)
{
	struct buffer *pbuffer = (struct buffer *)data;

	pbuffer->buf = NULL;
	zwp_linux_buffer_params_v1_destroy(params);
	close(pbuffer->dma_fd);
	ALOGE("zwp_linux_buffer_params create failed.\n");
}

static const struct zwp_linux_buffer_params_v1_listener params_listener = {
	create_succeeded,
	create_failed
};

static int open_cma(char const* dev_name)
{
	int fd = open(dev_name, O_RDWR);
	if (fd < 0) {
		ALOGE("open %s failed\n", dev_name);
		return -1;
	}
	return fd;
}

static uint32_t convert_format(uint32_t v4l2_format)
{
    switch (v4l2_format) {
        case V4L2_PIX_FMT_YUYV:
            return DRM_FORMAT_YUYV;
        case V4L2_PIX_FMT_NV12:
            return DRM_FORMAT_NV12;
        case V4L2_PIX_FMT_NV16:
            return DRM_FORMAT_YUYV;
        default:
            ALOGE("Format not support");
            return 0;
    }
}

static unsigned int set_display_format(struct display *pdisplay)
{
    struct v4l2_format format;
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(pdisplay->v4l2_fd, VIDIOC_G_FMT, &format) < 0) {
        ALOGE("Get format error");
        return -1; 
    }
    pdisplay->format.width = format.fmt.pix.width;
    pdisplay->format.height = format.fmt.pix.height;
    pdisplay->format.stride = format.fmt.pix.bytesperline;
    pdisplay->format.sizeimage = format.fmt.pix.sizeimage;
    pdisplay->format.format = format.fmt.pix.pixelformat;
    pdisplay->format.type = format.type;
    pdisplay->drm_format = convert_format(pdisplay->format.format);
    if (pdisplay->drm_format == DRM_FORMAT_YUYV)
         pdisplay->format.stride = pdisplay->format.width * 2;
    return 0;
}

static int get_buffer_number(int v4l2_fd)
{
    int i = 0;
    int ret = 0;
    do {
        v4l2_buffer buffer = {};
        memset(&buffer, 0, sizeof(buffer));
        buffer.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory   = V4L2_MEMORY_MMAP;
        buffer.index    = i;
        ret =  ioctl(v4l2_fd, VIDIOC_QUERYBUF, &buffer);
    } while(ret >= 0 && ++i);
    return i;
}

static int create_cma_dmabuf(struct display *pdisplay, struct buffer *pbuffer)
{  
    struct dma_buf_phys dma_phys;
    struct dma_heap_allocation_data allocation_data = { pdisplay->format.sizeimage, 0, O_RDWR | O_CLOEXEC, DMA_HEAP_VALID_HEAP_FLAGS };

    if (ioctl(pdisplay->cma_fd, DMA_HEAP_IOCTL_ALLOC, &allocation_data) < 0)
    {
        ALOGE("DMA_HEAP_IOCTL_ALLOC: %s", strerror(errno));
        return -1;
    }

    if (ioctl(allocation_data.fd, DMA_BUF_IOCTL_PHYS, &dma_phys) < 0) {
        ALOGE("DMA_BUF_IOCTL_PHYS: %s", strerror(errno));
        return -1;
    }

    pbuffer->dma_fd = allocation_data.fd;
    pbuffer->paddr = dma_phys.phys;
    return 0;
}

static int buffer_export(struct buffer *pbuffer, struct dma_map *pdma_map)
{
    struct v4l2_exportbuffer expbuf;
    memset(&expbuf, 0, sizeof(expbuf));

    struct dma_buf_phys buf_addr;
    memset(&buf_addr, 0, sizeof(buf_addr));

    struct display *pdisplay  = pbuffer->display;

    /*export dma buffer to connect capture and display*/
    expbuf.type = pdisplay->format.type;
    expbuf.index = pbuffer->index;
    if (ioctl(pdisplay->v4l2_fd, VIDIOC_EXPBUF, &expbuf) < -1) {
        ALOGE("VIDIOC_EXPBUF: %s", strerror(errno));
        return -1;
    }
    
    if (ioctl(expbuf.fd, DMA_BUF_IOCTL_PHYS, &buf_addr) < 0 ) {
        ALOGE("DMA_BUF_IOCTL_PHYS: %s", strerror(errno));
        return -1;
    }

    pbuffer->dma_fd = expbuf.fd;
    pbuffer->width = pdisplay->format.width;
    pbuffer->height = pdisplay->format.height;
    pdma_map->dma_phys = buf_addr.phys;
    pdma_map->index = pbuffer->index;

    if (pdisplay->format.format == V4L2_PIX_FMT_NV16) {
        if (create_cma_dmabuf(pdisplay, pbuffer) < 0) {
            ALOGE("create cma dmabuf failed");
            return -1;
        }
    }

    return 0;
}

static void create_dma_buf(struct buffer *pbuffer)
{
    struct display *pdisplay;
    struct zwp_linux_buffer_params_v1 *params;
    uint64_t modifier;
    uint32_t flags; 
    flags = 0;
    modifier = 0;
    pdisplay = pbuffer->display;

    switch (pdisplay->drm_format) {
        case DRM_FORMAT_YUYV:
            pbuffer->plane = 1;
            pbuffer->plane_offset[0] = 0;
            break;
        case DRM_FORMAT_NV12:
            pbuffer->plane = 2;
            pbuffer->plane_offset[0] = 0;
            pbuffer->plane_offset[1] = pbuffer->width * pbuffer->height;
            break;
        default:
            pbuffer->plane = 1;
            pbuffer->plane_offset[0] = 0;
            break;
    }
    /*create wl_buffer*/
    params = zwp_linux_dmabuf_v1_create_params(pdisplay->dmabuf);
    for (int i = 0; i < pbuffer->plane; i++)
        zwp_linux_buffer_params_v1_add(params,
                                        pbuffer->dma_fd,
                                        i, 
                                        pbuffer->plane_offset[i],
                                        pdisplay->format.stride,
                                        modifier >> 32, 
                                        modifier & 0xffffffff);


    zwp_linux_buffer_params_v1_add_listener(params, &params_listener, pbuffer);
	zwp_linux_buffer_params_v1_create(params,
	                                  pdisplay->format.width,
	                                  pdisplay->format.height,
	                                  pdisplay->drm_format,
	                                  flags);
    wl_display_roundtrip(pbuffer->display->display);
}

static int queue_initial_buffers(struct window *pwindow)
{
    int index;
    int ret;
    struct buffer *pbuffer;
    struct dma_map *pdma_map;
    pwindow->buffers = (struct buffer *)calloc(pwindow->num_bufs, sizeof(struct buffer));
    pwindow->maps = (struct dma_map *)calloc(pwindow->num_bufs, sizeof(struct dma_map));
    for (index = 0; index < pwindow->num_bufs; index++) {
        pbuffer = &pwindow->buffers[index];
        pdma_map = &pwindow->maps[index];
		pbuffer->display = pwindow->display;
		pbuffer->index = index;
        ret = buffer_export(pbuffer, pdma_map);
        if (ret < 0) {
            return -1;
        }
        create_dma_buf(pbuffer);
    }
    return 0;
}

int WlDisplay::connect_v4l2(int v4l2_fd)
{
    int ret;
    struct display *pdisplay = window->display;
    pdisplay->v4l2_fd = v4l2_fd;
    window->num_bufs = get_buffer_number(v4l2_fd);
    ret = set_display_format(pdisplay);
    if (ret < 0)
    {
        return -1;
    }

    /*V4L2_PIX_FMT_NV16 need to use g2d to convert*/
    if (pdisplay->format.format == V4L2_PIX_FMT_NV16) {
        pdisplay->cma_fd = open_cma("/dev/dma_heap/linux,cma");
        if(pdisplay->cma_fd < 0) {
            return -1;
        }
    }

    ret = queue_initial_buffers(window);
    if (ret < 0) {
        ALOGE("queue init buffers failed");
        return -1;
    }
    return 0;
} 

WlDisplay::WlDisplay()
{
    struct display *pdisplay;
    pdisplay = create_display();
    window = create_window(pdisplay);
}

static void destroy_display(struct display *pdisplay)
{
	if (pdisplay->dmabuf)
		zwp_linux_dmabuf_v1_destroy(pdisplay->dmabuf);

	if (pdisplay->wm_base)
		xdg_wm_base_destroy(pdisplay->wm_base);

	if (pdisplay->compositor)
		wl_compositor_destroy(pdisplay->compositor);

	wl_registry_destroy(pdisplay->registry);
	wl_display_flush(pdisplay->display);
	wl_display_disconnect(pdisplay->display);
    if (pdisplay->cma_fd > 0)
        close(pdisplay->cma_fd);
    free(pdisplay);
}


static void destroy_window(struct window *pwindow)
{
	int i;
	if (pwindow->xdg_toplevel)
		xdg_toplevel_destroy(pwindow->xdg_toplevel);
	if (pwindow->xdg_surface)
		xdg_surface_destroy(pwindow->xdg_surface);
	wl_surface_destroy(pwindow->surface);

	for (i = 0; i < pwindow->num_bufs; i++) {
        close(pwindow->buffers[i].dma_fd);
		if (!pwindow->buffers[i].buf)
			continue;
		wl_buffer_destroy(pwindow->buffers[i].buf);
		
	}

    free(pwindow->buffers);
    free(pwindow->maps);
	free(pwindow);
}

WlDisplay::~WlDisplay()
{
    struct display *pdisplay = window->display;
    destroy_window(window);
    destroy_display(pdisplay);

    if (g2d_handle) {
        g2d_close(g2d_handle);
        g2d_handle = NULL;
    }
}
#endif //WAYLAND_SUPPORT End

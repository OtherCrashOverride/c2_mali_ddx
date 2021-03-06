/*
 * Copyright (C) 2010 ARM Limited. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <xorg-server.h>
#include <xf86.h>

#include "mali_def.h"
#include "mali_fbdev.h"
#include "mali_exa.h"
#include "umplock/umplock_ioctl.h"
#include <linux/fb.h>
#include "ge2d.h"
#include "ge2d_cmd.h"

#define MALI_EXA_FUNC(s) exa->s = mali ## s
#define MALI_ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))


static int ge2d_fd = -1;
static PixmapPtr SrcPixmap = 0;
static int memclient_fd = -1;

typedef struct
{
	int handle;
	int _padding00;
	unsigned long physical_address;
	unsigned int length;
} memclient_attach_dmabuf_param_t;

#define MEMCLIENT_ATTACH_UMP		_IOWR('M', 0x03, memclient_attach_dmabuf_param_t)
#define MEMCLIENT_RELEASE_UMP		_IOWR('M', 0x04, int)


static void init_ge2d()
{
	ge2d_fd = open("/dev/ge2d", O_RDWR);
	if (ge2d_fd < 0)
	{
		ERROR_STR("Can't open /dev/ge2d\n");
	}


	memclient_fd = open("/dev/memclient", O_RDWR);
	if (memclient_fd < 0)
	{
		ERROR_STR("Can't open /dev/memclient\n");
	}
}

static Bool maliPrepareSolid(PixmapPtr pPixmap, int alu, Pixel planemask, Pixel fg)
{
	IGNORE(pPixmap);
	IGNORE(alu);
	IGNORE(planemask);
	IGNORE(fg);

	return FALSE;
}

static void maliSolid(PixmapPtr pPixmap, int x1, int y1, int x2, int y2)
{
	IGNORE(pPixmap);
	IGNORE(x1);
	IGNORE(y1);
	IGNORE(x2);
	IGNORE(y2);
}

static void maliDoneSolid(PixmapPtr pPixmap)
{
	IGNORE(pPixmap);
}

static Bool maliPrepareCopy(PixmapPtr pSrcPixmap, PixmapPtr pDstPixmap, int xdir, int ydir, int alu, Pixel planemask)
{
	PrivPixmap *srcPrivPixmap = (PrivPixmap *)exaGetPixmapDriverPrivate(pSrcPixmap);
	PrivPixmap *dstPrivPixmap = (PrivPixmap *)exaGetPixmapDriverPrivate(pDstPixmap);

	IGNORE(pSrcPixmap);
	IGNORE(pDstPixmap);
	IGNORE(xdir);
	IGNORE(ydir);
	IGNORE(alu);
	IGNORE(planemask);

	if (srcPrivPixmap->isFrameBuffer &&
		dstPrivPixmap->isFrameBuffer)
	{
		//if (srcPrivPixmap->frameBufferNumber != 0 ||
		//	srcPrivPixmap->frameBufferNumber != 0)
		//{
		//	ERROR_STR("%s: pSrcPixmap=%p (buffer# %d) pDstPixmap=%p (buffer# %d) xdir=%d ydir=%d alu=%x\n",
		//		__FUNCTION__, pSrcPixmap, srcPrivPixmap->frameBufferNumber, pDstPixmap, dstPrivPixmap->frameBufferNumber, xdir, ydir, alu);
		//}

		SrcPixmap = pSrcPixmap;

		if (ge2d_fd < 0)
		{
			init_ge2d();
		}


		return TRUE;
	}

	//if (srcPrivPixmap->mem_info &&
	//	dstPrivPixmap->mem_info)
	//{
	//	//if (srcPrivPixmap->mem_info->handle != UMP_INVALID_SECURE_ID &&
	//	//	dstPrivPixmap->mem_info->handle != UMP_INVALID_SECURE_ID)
	//	{
	//		SrcPixmap = pSrcPixmap;

	//		if (ge2d_fd < 0)
	//		{
	//			init_ge2d();
	//		}

	//		return TRUE;
	//	}
	//}

	return FALSE;
}

static void maliCopy(PixmapPtr pDstPixmap, int srcX, int srcY, int dstX, int dstY, int width, int height)
{
	PrivPixmap *srcPrivPixmap = (PrivPixmap *)exaGetPixmapDriverPrivate(SrcPixmap);
	PrivPixmap *dstPrivPixmap = (PrivPixmap *)exaGetPixmapDriverPrivate(pDstPixmap);


	IGNORE(pDstPixmap);
	IGNORE(srcX);
	IGNORE(srcY);
	IGNORE(dstX);
	IGNORE(dstY);
	IGNORE(width);
	IGNORE(height);


	//ERROR_STR("%s: pDstPixmap=%p srcX=%d srcY=%d dstX=%d dstY=%d width=%d height=%d\n",
	//	__FUNCTION__, pDstPixmap, srcX, srcY, dstX, dstY, width, height);


	ScreenPtr pScreen = SrcPixmap->drawable.pScreen;
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	MaliPtr fPtr = MALIPTR(pScrn);

	struct fb_var_screeninfo var_info;
	int ret = ioctl(fPtr->fb_lcd_fd, FBIOGET_VSCREENINFO, &var_info);
	if (ret < 0)
	{
		ERROR_STR("FBIOGET_VSCREENINFO failed.\n");
	}


	unsigned char revX = 0;
	unsigned char revY = 0;

	if (dstY > srcY)
	{
		revY = 1;
	}

	if (dstX > srcX)
	{
		revX = 1;
	}

	//if (srcPrivPixmap->frameBufferNumber != 0)
	//{
	//	srcY += (var_info.yres * srcPrivPixmap->frameBufferNumber);
	//}

	//if (dstPrivPixmap->frameBufferNumber != 0)
	//{
	//	dstY += (var_info.yres * dstPrivPixmap->frameBufferNumber);
	//}

	ump_secure_id srcID = ump_secure_id_get(srcPrivPixmap->mem_info->handle);

	memclient_attach_dmabuf_param_t srcParam;
	srcParam.handle = srcID;
	ret = ioctl(memclient_fd, MEMCLIENT_ATTACH_UMP, &srcParam);
	if (ret < 0)
	{
		ERROR_STR("srcParam MEMCLIENT_ATTACH_UMP failed.\n");
	}


	ump_secure_id dstID = ump_secure_id_get(dstPrivPixmap->mem_info->handle);

	memclient_attach_dmabuf_param_t dstParam;
	dstParam.handle = dstID;
	ret = ioctl(memclient_fd, MEMCLIENT_ATTACH_UMP, &dstParam);
	if (ret < 0)
	{
		ERROR_STR("dstParam MEMCLIENT_ATTACH_UMP failed.\n");
	}



	// Configure GE2D
	struct config_para_ex_s configex;
	memset(&configex, 0x00, sizeof(configex));

	configex.src_para.mem_type = CANVAS_ALLOC;	//CANVAS_OSD0
	configex.src_para.left = 0;
	configex.src_para.top = 0;
	configex.src_para.width = var_info.xres;
	configex.src_para.height = var_info.yres;
	configex.src_para.x_rev = revX;
	configex.src_para.y_rev = revY;
	configex.src_planes[0].addr = srcParam.physical_address;
	configex.src_planes[0].w = configex.src_para.width;
	configex.src_planes[0].h = configex.src_para.height;
	configex.src_para.format = GE2D_FORMAT_S32_ARGB; // GE2D_FORMAT_S32_BGRA; // GE2D_FMT_S32_RGBA;


	configex.src2_para.mem_type = CANVAS_TYPE_INVALID;


	configex.dst_para.mem_type = CANVAS_ALLOC; // CANVAS_OSD0;
	configex.dst_para.left = 0;
	configex.dst_para.top = 0;
	configex.dst_para.width = configex.src_para.width;
	configex.dst_para.height = configex.src_para.height;
	configex.dst_para.x_rev = configex.src_para.x_rev;
	configex.dst_para.y_rev = configex.src_para.y_rev;
	configex.dst_planes[0].addr = dstParam.physical_address;
	configex.dst_planes[0].w = configex.dst_para.width;
	configex.dst_planes[0].h = configex.dst_para.height;
	configex.dst_para.format = GE2D_FORMAT_S32_ARGB;


	ret = ioctl(ge2d_fd, GE2D_CONFIG_EX, &configex);
	if (ret < 0)
	{
		ERROR_STR("GE2D_CONFIG_EX failed.\n");
	}

	// Perform the blit operation
	struct ge2d_para_s blitRect;
	memset(&blitRect, 0, sizeof(blitRect));

	blitRect.src1_rect.x = srcX;
	blitRect.src1_rect.y = srcY;
	blitRect.src1_rect.w = width;
	blitRect.src1_rect.h = height;
	blitRect.dst_rect.x = dstX;
	blitRect.dst_rect.y = dstY;

	if (revY) // && height > 1
	{
		blitRect.src1_rect.y -= 1;
	}

	ret = ioctl(ge2d_fd, GE2D_BLIT, &blitRect);
	if (ret < 0)
	{
		ERROR_STR("GE2D_BLIT failed.\n");
	}


	ret = ioctl(memclient_fd, MEMCLIENT_RELEASE_UMP, dstID);
	if (ret < 0)
	{
		ERROR_STR("dstID MEMCLIENT_RELEASE_UMP failed.\n");
	}

	ret = ioctl(memclient_fd, MEMCLIENT_RELEASE_UMP, srcID);
	if (ret < 0)
	{
		ERROR_STR("srcID MEMCLIENT_RELEASE_UMP failed.\n");
	}

}

static void maliDoneCopy(PixmapPtr pDstPixmap)
{
	IGNORE(pDstPixmap);
}

static void maliWaitMarker(ScreenPtr pScreen, int marker)
{
	IGNORE(pScreen);
	IGNORE(marker);
}

static void *maliCreatePixmap(ScreenPtr pScreen, int size, int align)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	MaliPtr fPtr = MALIPTR(pScrn);

	PrivPixmap *privPixmap = calloc(1, sizeof(PrivPixmap));

	if (NULL == privPixmap)
	{
		return NULL;
	}

	IGNORE(pScreen);
	IGNORE(size);
	IGNORE(align);

	privPixmap->isFrameBuffer  = FALSE;
	privPixmap->bits_per_pixel = 0;
	privPixmap->mem_info = NULL;
	privPixmap->buf_info = NULL;

	return privPixmap;
}

static void maliDestroyPixmap(ScreenPtr pScreen, void *driverPriv)
{
	PrivPixmap *privPixmap = (PrivPixmap *)driverPriv;

	IGNORE(pScreen);

	if (NULL != privPixmap->mem_info)
	{
		/* frame buffer pixmap should not be called here */
		assert(!privPixmap->isFrameBuffer);
		ump_reference_release(privPixmap->mem_info->handle);
		free(privPixmap->mem_info);
		free(privPixmap);
	}
}

static unsigned int current_buf = 0;

static Bool maliModifyPixmapHeader(PixmapPtr pPixmap, int width, int height, int depth, int bitsPerPixel, int devKind, pointer pPixData)
{
	unsigned int size;
	PrivPixmap *privPixmap = (PrivPixmap *)exaGetPixmapDriverPrivate(pPixmap);
	mali_mem_info *mem_info;
	ScreenPtr pScreen = pPixmap->drawable.pScreen;
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	MaliPtr fPtr = MALIPTR(pScrn);

	if (!pPixmap)
	{
		return FALSE;
	}

	miModifyPixmapHeader(pPixmap, width, height, depth, bitsPerPixel, devKind, pPixData);

	if ((pPixData == fPtr->fbmem) || current_buf)
	{
		/* Wrap one of the fbdev virtual buffers */
		ump_secure_id ump_id = UMP_INVALID_SECURE_ID;

		privPixmap->isFrameBuffer = TRUE;
		privPixmap->frameBufferNumber = current_buf;
		mem_info = privPixmap->mem_info;

		if (mem_info)
		{
			return TRUE;
		}

		/* create new mem_info for the on-screen buffer */
		mem_info = calloc(1, sizeof(*mem_info));

		if (!mem_info)
		{
			ERROR_MSG("failed to allocate for memory metadata");
			return FALSE;
		}

		/* get the secure ID for the framebuffers */
		if (ioctl(fPtr->fb_lcd_fd, GET_UMP_SECURE_ID_BUF(current_buf), &ump_id) < 0
		        || UMP_INVALID_SECURE_ID == ump_id)
		{
			free(mem_info);
			privPixmap->mem_info = NULL;
			ERROR_MSG("UMP failed to retrieve secure id, current_buf: %d", current_buf);
			return FALSE;
		}

		INFO_MSG("GET_UMP_SECURE_ID_BUF(%d) returned 0x%x", current_buf, ump_id);

		mem_info->handle = ump_handle_create_from_secure_id(ump_id);

		if (UMP_INVALID_MEMORY_HANDLE == mem_info->handle)
		{
			ERROR_MSG("UMP failed to create handle from secure id");
			free(mem_info);
			privPixmap->mem_info = NULL;
			return FALSE;
		}

		size = exaGetPixmapPitch(pPixmap) * pPixmap->drawable.height;
		mem_info->usize = size;

		privPixmap->mem_info = mem_info;

		if (bitsPerPixel != 0)
		{
			privPixmap->bits_per_pixel = bitsPerPixel;
		}

		/* When this is called directly from X to create the front buffer, current_buf is zero as expected. When this
		 * function is called recursively to create the back buffers, current_buf is increased to the next buffer */
		privPixmap->mem_info->offset = current_buf * size;

		if (pPixData == fPtr->fbmem)
		{
			/* This is executed only when this function is called directly from X. We need to create the other
			 * back buffers now because we can't "wrap" existing memory in a pixmap during DRI2CreateBuffer
			 * for the back buffer of the framebuffer. In DRI2CreateBuffer instead of allocating a new
			 * pixmap for the back buffer like we do for non-swappable windows, we'll just use the 'current_pixmap'
			 * to grab this pointer from the screen pixmap and return it. */

			PrivPixmap *current_privPixmap = privPixmap;
			int i;
			PrivBuffer *buf_info = calloc(1, sizeof(*buf_info));

			if (NULL == buf_info)
			{
				ERROR_MSG("Failed to allocate buf_info memory");
				free(mem_info);
				privPixmap->mem_info = NULL;
				return FALSE;
			}

			buf_info->current_pixmap = 0;
			buf_info->num_pixmaps = fPtr->dri2_num_buffers;
			buf_info->pPixmaps[0] = pPixmap;
			current_privPixmap->buf_info = buf_info;

			for (i = 1; i < buf_info->num_pixmaps; i++)
			{
				current_buf++;
				buf_info->pPixmaps[i] = (*pScreen->CreatePixmap)(pScreen, width, height, depth, 0);
				assert(buf_info->pPixmaps[i]);
				current_privPixmap = (PrivPixmap *)exaGetPixmapDriverPrivate(buf_info->pPixmaps[i]);
				current_privPixmap->buf_info = buf_info;
			}

			current_buf = 0;
		}

		INFO_MSG("Creating FRAMEBUFFER pixmap %p at offset %lu, privPixmap=%p", pPixmap, privPixmap->mem_info->offset, privPixmap);

		return TRUE;
	}

	if (pPixData)
	{
		/* When this happens we're being told to wrap existing pixmap data for which we don't know the UMP
		 * handle. We can and still need to wrap it but it won't be offscreen - we can't accelerate it in any
		 * way. */

		if (privPixmap->mem_info != NULL)
		{
			return TRUE;
		}

		return FALSE;
	}

	pPixmap->devKind = ((pPixmap->drawable.width * pPixmap->drawable.bitsPerPixel) + 7) / 8;
	pPixmap->devKind = MALI_ALIGN(pPixmap->devKind, 8);

	size = exaGetPixmapPitch(pPixmap) * pPixmap->drawable.height;

	/* allocate pixmap data */
	mem_info = privPixmap->mem_info;

	if (mem_info && mem_info->usize == size)
	{
		return TRUE;
	}

	if (mem_info && mem_info->usize != 0)
	{
		ump_reference_release(mem_info->handle);
		mem_info->handle = NULL;
		memset(privPixmap, 0, sizeof(*privPixmap));

		return TRUE;
	}

	if (!size)
	{
		return TRUE;
	}

	if (NULL == mem_info)
	{
		mem_info = calloc(1, sizeof(*mem_info));

		if (!mem_info)
		{
			ERROR_MSG("failed to allocate memory metadata");
			return FALSE;
		}
	}

	if (fPtr->use_cached_ump)
	{
		mem_info->handle = ump_ref_drv_allocate(size, UMP_REF_DRV_CONSTRAINT_PHYSICALLY_LINEAR | UMP_REF_DRV_CONSTRAINT_USE_CACHE);
	}
	else
	{
		mem_info->handle = ump_ref_drv_allocate(size, UMP_REF_DRV_CONSTRAINT_PHYSICALLY_LINEAR);
	}

	if (UMP_INVALID_MEMORY_HANDLE == mem_info->handle)
	{
		ERROR_MSG("failed to allocate UMP memory (%i bytes)", size);
		return FALSE;
	}

	mem_info->usize = size;
	privPixmap->mem_info = mem_info;
	privPixmap->mem_info->usize = size;
	privPixmap->bits_per_pixel = 16;

	return TRUE;
}

static Bool maliPixmapIsOffscreen(PixmapPtr pPix)
{
	ScreenPtr pScreen = pPix->drawable.pScreen;
	PrivPixmap *privPixmap = (PrivPixmap *)exaGetPixmapDriverPrivate(pPix);

	if (pScreen->GetScreenPixmap(pScreen) == pPix)
	{
		return TRUE;
	}

	if (privPixmap)
	{
		return pPix->devPrivate.ptr ? FALSE : TRUE;
	}

	return FALSE;
}

static Bool maliPrepareAccess(PixmapPtr pPix, int index)
{
	ScreenPtr pScreen = pPix->drawable.pScreen;
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	MaliPtr fPtr = MALIPTR(pScrn);

	PrivPixmap *privPixmap = (PrivPixmap *)exaGetPixmapDriverPrivate(pPix);
	mali_mem_info *mem_info;

	IGNORE(index);

	if (!privPixmap)
	{
		ERROR_MSG("Failed to get private pixmap data");
		return FALSE;
	}

	mem_info = privPixmap->mem_info;

	if (NULL != mem_info)
	{
		if (privPixmap->refs == 0)
		{
			if (privPixmap->isFrameBuffer)
			{
				privPixmap->addr = (unsigned long)fPtr->fbmem;
			}
			else
			{
				privPixmap->addr = (unsigned long)ump_mapped_pointer_get(mem_info->handle);
			}

			privPixmap->addr += mem_info->offset;
		}
	}
	else
	{
		ERROR_MSG("No mem_info on pixmap");
		return FALSE;
	}

	pPix->devPrivate.ptr = (void *)(privPixmap->addr);

	if (NULL == pPix->devPrivate.ptr)
	{
		ERROR_MSG("cpu address not set");
		return FALSE;
	}

	privPixmap->refs++;

	if (!privPixmap->isFrameBuffer)
	{
		int secure_id = 0;
		_lock_item_s item;

		secure_id = ump_secure_id_get(mem_info->handle);

		if (secure_id)
		{
			item.secure_id = secure_id;
			item.usage = _LOCK_ACCESS_CPU_WRITE;

			if (fPtr->fd_umplock > 0)
			{
				ioctl(fPtr->fd_umplock, LOCK_IOCTL_CREATE, &item);

				if (ioctl(fPtr->fd_umplock, LOCK_IOCTL_PROCESS, &item) < 0)
				{
					int max_retries = 5;
					ERROR_MSG("Unable to process lock item with ID 0x%x - throttling\n", item.secure_id);

					while ((ioctl(fPtr->fd_umplock, LOCK_IOCTL_PROCESS, &item) < 0) && max_retries)
					{
						usleep(2000);
						max_retries--;
					}

					if (max_retries == 0)
					{
						ERROR_MSG("Warning: Max retries == 0\n");
					}
				}
			}

			if (fPtr->use_cached_ump)
			{
				ump_cache_operations_control(UMP_CACHE_OP_START);
				ump_switch_hw_usage_secure_id(item.secure_id, UMP_USED_BY_CPU);
				ump_cache_operations_control(UMP_CACHE_OP_FINISH);
			}
		}
	}

	return TRUE;
}

static void maliFinishAccess(PixmapPtr pPix, int index)
{
	PrivPixmap *privPixmap = (PrivPixmap *)exaGetPixmapDriverPrivate(pPix);
	mali_mem_info *mem_info;
	ScreenPtr pScreen = pPix->drawable.pScreen;
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	MaliPtr fPtr = MALIPTR(pScrn);

	IGNORE(index);

	if (!privPixmap)
	{
		return;
	}

	if (!pPix)
	{
		return;
	}

	mem_info = privPixmap->mem_info;

	if (!privPixmap->isFrameBuffer)
	{
		int secure_id = 0;
		_lock_item_s item;

		secure_id = ump_secure_id_get(mem_info->handle);

		if (secure_id)
		{
			item.secure_id = secure_id;
			item.usage = _LOCK_ACCESS_CPU_WRITE;

			if (fPtr->fd_umplock > 0)
			{
				ioctl(fPtr->fd_umplock, LOCK_IOCTL_RELEASE, &item);
			}
		}

		if (privPixmap->refs == 1)
		{
			if (NULL != mem_info)
			{
				ump_mapped_pointer_release(mem_info->handle);
			}
		}
	}

	pPix->devPrivate.ptr = NULL;
	privPixmap->refs--;
}

static Bool maliCheckComposite(int op, PicturePtr pSrcPicture, PicturePtr pMaskPicture, PicturePtr pDstPicture)
{
	IGNORE(op);
	IGNORE(pSrcPicture);
	IGNORE(pMaskPicture);
	IGNORE(pDstPicture);

	return FALSE;
}

static Bool maliPrepareComposite(int op, PicturePtr pSrcPicture, PicturePtr pMaskPicture, PicturePtr pDstPicture, PixmapPtr pSrcPixmap, PixmapPtr pMask, PixmapPtr pDstPixmap)
{
	IGNORE(op);
	IGNORE(pSrcPicture);
	IGNORE(pMaskPicture);
	IGNORE(pDstPicture);
	IGNORE(pSrcPixmap);
	IGNORE(pMask);
	IGNORE(pDstPixmap);

	return FALSE;
}

static void maliComposite(PixmapPtr pDstPixmap, int srcX, int srcY, int maskX, int maskY, int dstX, int dstY, int width, int height)
{
	IGNORE(pDstPixmap);
	IGNORE(srcX);
	IGNORE(srcY);
	IGNORE(maskX);
	IGNORE(maskY);
	IGNORE(dstX);
	IGNORE(dstY);
	IGNORE(width);
	IGNORE(height);
}

static void maliDoneComposite(PixmapPtr pDst)
{
	IGNORE(pDst);
}

Bool maliSetupExa(ScreenPtr pScreen, ExaDriverPtr exa)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	MaliPtr fPtr = MALIPTR(pScrn);

	if (NULL == exa)
	{
		return FALSE;
	}

	TRACE_ENTER();

	exa->exa_major = 2;
	exa->exa_minor = 0;
	exa->memoryBase = fPtr->fbmem;
	exa->maxX = fPtr->fb_lcd_var.xres_virtual;
	exa->maxY = fPtr->fb_lcd_var.yres_virtual;
	exa->flags = EXA_OFFSCREEN_PIXMAPS | EXA_HANDLES_PIXMAPS | EXA_SUPPORTS_PREPARE_AUX;
	exa->offScreenBase = (fPtr->fb_lcd_fix.line_length * fPtr->fb_lcd_var.yres);
	exa->memorySize = fPtr->fb_lcd_fix.smem_len;
	exa->pixmapOffsetAlign = 4096;
	exa->pixmapPitchAlign = 8;

	MALI_EXA_FUNC(PrepareSolid);
	MALI_EXA_FUNC(Solid);
	MALI_EXA_FUNC(DoneSolid);

	MALI_EXA_FUNC(PrepareCopy);
	MALI_EXA_FUNC(Copy);
	MALI_EXA_FUNC(DoneCopy);

	MALI_EXA_FUNC(CheckComposite);
	MALI_EXA_FUNC(PrepareComposite);
	MALI_EXA_FUNC(Composite);
	MALI_EXA_FUNC(DoneComposite);

	MALI_EXA_FUNC(WaitMarker);

	MALI_EXA_FUNC(CreatePixmap);
	MALI_EXA_FUNC(DestroyPixmap);
	MALI_EXA_FUNC(ModifyPixmapHeader);
	MALI_EXA_FUNC(PixmapIsOffscreen);

	MALI_EXA_FUNC(PrepareAccess);
	MALI_EXA_FUNC(FinishAccess);

	if (UMP_OK != ump_open())
	{
		ERROR_MSG("failed to open UMP subsystem");
		TRACE_EXIT();
		return FALSE;
	}

	INFO_MSG("Mali EXA driver is loaded successfully");
	TRACE_EXIT();

	return TRUE;
}

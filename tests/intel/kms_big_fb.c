/*
 * Copyright © 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * TEST: kms big fb
 * Category: Display
 * Description: Test big framebuffers
 * Driver requirement: i915, xe
 * Mega feature: General Display Features
 * Test category: functionality test
 */

#include "igt.h"
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "i915/gem_create.h"
#include "intel_mocs.h"
#include "intel_pat.h"
#include "xe/xe_ioctl.h"
#include "xe/xe_query.h"

/**
 * SUBTEST: linear-%s-rotate-%d
 * Description: Sanity check if addfb ioctl works correctly for given combination
 *              of Linear modifier with %arg[1]-bpp & %arg[2]-rotation
 * Functionality: big_fbs, kms_gem_interop, rotation
 *
 * SUBTEST: linear-%s-rotate-%d-hflip
 * Description: Sanity check if addfb ioctl works correctly for given combination
 *              of Linear modifier with %arg[1]-bpp & %arg[2]-rotation
 * Functionality: big_fbs, kms_gem_interop, rotation
 *
 * arg[1].values:       8bpp, 16bpp, 32bpp, 64bpp, nv12, p016
 * arg[2].values:       0, 90, 180, 270
 */

/**
 * SUBTEST: %s-%s-rotate-%d
 * Description: Sanity check if addfb ioctl works correctly for given combination
 *              of %arg[1] with %arg[2]-bpp & %arg[3]-rotation
 * Functionality: big_fbs, kms_gem_interop, rotation, tiling
 *
 * SUBTEST: %s-%s-rotate-%d-hflip
 * Description: Sanity check if addfb ioctl works correctly for given combination
 *              of %arg[1] with %arg[2]-bpp & %arg[3]-rotation
 * Functionality: big_fbs, kms_gem_interop, rotation, tiling
*
 * arg[1]:
 *
 * @4-tiled:            TILE-4 modifier
 * @x-tiled:            TILE-X modifier
 * @y-tiled:            TILE-Y modifier
 * @yf-tiled:           TILE-YF modifier
 * @y-tiled-ccs:        TILE-Y+CCS modifier
 * @yf-tiled-ccs:       TILE-YF+CCS modifier
 * @y-tiled-rc-ccs:     TILE-Y+CCS modifier
 * @y-tiled-rc-ccs-cc:  TILE-Y+CCS+CC modifier
 * @4-tiled-mtl-rc-ccs:     TILE-4+CCS modifier (MTL)
 * @4-tiled-mtl-rc-ccs-cc:  TILE-4+CCS+CC modifier (MTL)
 * @4-tiled-dg2-rc-ccs:     TILE-4+CCS modifier (DG2)
 * @4-tiled-dg2-rc-ccs-cc:  TILE-4+CCS+CC modifier (DG2)
 *
 * arg[2].values:       8bpp, 16bpp, 32bpp, 64bpp, nv12, p016
 * arg[3].values:       0, 90, 180, 270
 *
 * arg[4]:
 */

/**
 * SUBTEST: linear-max-hw-stride-%dbpp-rotate-%d
 * Description: Test maximum hardware supported stride length for given combination
 *              of linear modifier with max hardware stride length, %arg[1]-bpp,
 *              and %arg[2]-rotation
 * Functionality: big_fbs, kms_gem_interop, rotation
 *
 * SUBTEST: linear-max-hw-stride-%dbpp-rotate-%d-async-flip
 * Description: Test maximum hardware supported stride length for given combination
 *              of linear modifier with max hardware stride length, %arg[1]-bpp,
 *              and %arg[2]-rotation
 * Functionality: async_flips, big_fbs, kms_gem_interop, rotation
 *
 * arg[1].values:       32, 64
 * arg[2].values:       0, 180
 */

/**
 * SUBTEST: %s-max-hw-stride-%dbpp-rotate-%d
 * Description: Test maximum hardware supported stride length for given combination
 *              of %arg[1] modifier with max hardware stride length, %arg[2]-bpp,
 *              and %arg[3]-rotation
 * Functionality: big_fbs, kms_gem_interop, rotation, tiling
 *
 * arg[1]:
 *
 * @4-tiled:            TILE-4 modifier
 * @x-tiled:            TILE-X modifier
 * @y-tiled:            TILE-Y modifier
 * @yf-tiled:           TILE-YF modifier
 * @y-tiled-ccs:        TILE-Y+CCS modifier
 * @yf-tiled-ccs:       TILE-YF+CCS modifier
 * @y-tiled-rc-ccs:     TILE-Y+CCS modifier
 * @y-tiled-rc-ccs-cc:  TILE-Y+CCS+CC modifier
 * @4-tiled-mtl-rc-ccs:     TILE-4+CCS modifier (MTL)
 * @4-tiled-mtl-rc-ccs-cc:  TILE-4+CCS+CC modifier (MTL)
 * @4-tiled-dg2-rc-ccs:     TILE-4+CCS modifier (DG2)
 * @4-tiled-dg2-rc-ccs-cc:  TILE-4+CCS+CC modifier (DG2)
 *
 * arg[2].values:       32, 64
 * arg[3].values:       0, 180
 */

/**
 * SUBTEST: %s-max-hw-stride-%dbpp-rotate-%d-hflip
 * Description: Test maximum hardware supported stride length for given combination
 *              of %arg[1] modifier with max hardware stride length, %arg[2]-bpp,
 *              and %arg[3]-rotation with H-flip mode
 * Functionality: big_fbs, kms_gem_interop, rotation, tiling
 *
 * SUBTEST: %s-max-hw-stride-%dbpp-rotate-%d-%s
 * Description: Test maximum hardware supported stride length for given combination
 *              of %arg[1] modifier with max hardware stride length, %arg[2]-bpp,
 *              and %arg[3]-rotation with %arg[4] mode
 * Functionality: async_flips, big_fbs, kms_gem_interop, rotation, tiling
 *
 * arg[1]:
 *
 * @4-tiled:            TILE-4 modifier
 * @x-tiled:            TILE-X modifier
 * @y-tiled:            TILE-Y modifier
 * @yf-tiled:           TILE-YF modifier
 * @y-tiled-ccs:        TILE-Y+CCS modifier
 * @yf-tiled-ccs:       TILE-YF+CCS modifier
 * @y-tiled-rc-ccs:     TILE-Y+CCS modifier
 * @y-tiled-rc-ccs-cc:  TILE-Y+CCS+CC modifier
 * @4-tiled-mtl-rc-ccs:     TILE-4+CCS modifier (MTL)
 * @4-tiled-mtl-rc-ccs-cc:  TILE-4+CCS+CC modifier (MTL)
 * @4-tiled-dg2-rc-ccs:     TILE-4+CCS modifier (DG2)
 * @4-tiled-dg2-rc-ccs-cc:  TILE-4+CCS+CC modifier (DG2)
 *
 * arg[2].values:       32, 64
 * arg[3].values:       0, 180
 *
 * arg[4]:
 *
 * @async-flip:         Async flip
 * @hflip-async-flip:   Async & H-flip
 */

/**
 * SUBTEST: linear-addfb
 * Description: Sanity check if addfb ioctl works correctly with Linear modifier
 *              for given size and strides of fb
 * Functionality: big_fbs, kms_gem_interop
 *
 * SUBTEST: %s-addfb
 * Description: Sanity check if addfb ioctl works correctly with %arg[1] modifier
 *              for given size and strides of fb
 * Functionality: big_fbs, kms_gem_interop, tiling
 *
 * SUBTEST: %s-addfb-size-overflow
 * Description: Sanity check if addfb ioctl fails correctly for (%arg[1]) modifier
 *              with small bo.
 * Functionality: big_fbs, kms_gem_interop, tiling
 *
 * SUBTEST: %s-addfb-size-offset-overflow
 * Description: Sanity check if addfb ioctl fails correctly for (%arg[1]) modifier
 *              and offsets with small bo
 * Functionality: big_fbs, kms_gem_interop, tiling
 *
 * arg[1]:
 *
 * @4-tiled:    TILE-4
 * @x-tiled:    TILE-X
 * @y-tiled:    TILE-Y
 * @yf-tiled:   TILE-YF
 * @y-tiled-ccs:        TILE-Y+CCS modifier
 * @yf-tiled-ccs:       TILE-YF+CCS modifier
 * @y-tiled-rc-ccs:     TILE-Y+CCS modifier
 * @y-tiled-rc-ccs-cc:  TILE-Y+CCS+CC modifier
 * @4-tiled-mtl-rc-ccs:     TILE-4+CCS modifier (MTL)
 * @4-tiled-mtl-rc-ccs-cc:  TILE-4+CCS+CC modifier (MTL)
 * @4-tiled-dg2-rc-ccs:     TILE-4+CCS modifier (DG2)
 * @4-tiled-dg2-rc-ccs-cc:  TILE-4+CCS+CC modifier (DG2)
 */

IGT_TEST_DESCRIPTION("Test big framebuffers");

typedef struct {
	int drm_fd;
	uint32_t devid;
	igt_display_t display;
	enum pipe pipe;
	igt_output_t *output;
	igt_plane_t *plane;
	igt_pipe_crc_t *pipe_crc;
	struct igt_fb small_fb, big_fb, big_fb_solid;
	uint32_t format;
	uint64_t modifier;
	int width, height;
	igt_rotation_t rotation;
	int max_fb_width, max_fb_height;
	int big_fb_width, big_fb_height;
	uint64_t ram_size, aper_size, mappable_size;
	igt_render_copyfunc_t render_copy;
	struct buf_ops *bops;
	struct intel_bb *ibb;
	bool max_hw_stride_test;
	bool async_flip_test;
	int max_hw_stride_pixels;
	int max_hw_stride_bytes;
} data_t;

static struct intel_buf *init_buf(data_t *data,
				  const struct igt_fb *fb,
				  const char *buf_name)
{
	return igt_fb_create_intel_buf(data->drm_fd, data->bops, fb, buf_name);
}

static void fini_buf(struct intel_buf *buf)
{
	intel_buf_destroy(buf);
}

static void copy_pattern(data_t *data,
			 struct igt_fb *dst_fb, int dx, int dy,
			 struct igt_fb *src_fb, int sx, int sy,
			 int w, int h)
{
	struct intel_buf *src, *dst;

	src = init_buf(data, src_fb, "big fb src");
	dst = init_buf(data, dst_fb, "big fb dst");

	if (is_i915_device(data->drm_fd)) {
		gem_set_domain(data->drm_fd, dst_fb->gem_handle,
			       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
		gem_set_domain(data->drm_fd, src_fb->gem_handle,
			       I915_GEM_DOMAIN_GTT, 0);
	}

	/*
	 * We expect the kernel to limit the max fb
	 * size/stride to something that can still
	 * rendered with the blitter/render engine.
	 */
	if (data->render_copy) {
		data->render_copy(data->ibb, src, sx, sy, w, h, dst, dx, dy);

		/* FIXME rendercopy should do this for us perhaps? */
		if (igt_format_is_yuv_semiplanar(data->format)) {
			igt_assert(!igt_fb_is_ccs_modifier(data->modifier));

			src->bpp *= 2;
			src->surface[0] = src->surface[1];
			dst->bpp *= 2;
			dst->surface[0] = dst->surface[1];

			data->render_copy(data->ibb, src, sx/2, sy/2, w/2, h/2, dst, dx/2, dy/2);
		}
	} else {
		w = min(w, src_fb->width - sx);
		w = min(w, dst_fb->width - dx);

		h = min(h, src_fb->height - sy);
		h = min(h, dst_fb->height - dy);

		intel_bb_blt_copy(data->ibb, src, sx, sy, src->surface[0].stride,
				  dst, dx, dy, dst->surface[0].stride, w, h, dst->bpp);
	}

	fini_buf(dst);
	fini_buf(src);

	/* intel_bb cache doesn't know when objects dissappear, so
	 * let's purge the cache */
	intel_bb_reset(data->ibb, true);
}

static void generate_pattern(data_t *data,
			     struct igt_fb *fb,
			     int w, int h)
{
	struct igt_fb pat_fb;

	igt_create_pattern_fb(data->drm_fd, w, h,
			      data->format, data->modifier,
			      &pat_fb);

	for (int y = 0; y < fb->height; y += h) {
		for (int x = 0; x < fb->width; x += w) {
			copy_pattern(data, fb, x, y,
				     &pat_fb, 0, 0,
				     pat_fb.width, pat_fb.height);
			w++;
			h++;
			if (igt_format_is_yuv_semiplanar(data->format)) {
				w++;
				h++;
			}
		}
	}

	igt_remove_fb(data->drm_fd, &pat_fb);
}

static bool uses_ggtt(data_t *data)
{
	/* DPT uses very little GGTT so no need to worry about it. */
	return intel_display_ver(data->devid) < 13 ||
		data->modifier == DRM_FORMAT_MOD_LINEAR;
}

static bool uses_mappable(data_t *data)
{
	/*
	 * The kernel limits scanout to the
	 * mappable portion of ggtt on gmch platforms.
	 */
	return intel_display_ver(data->devid) < 5 ||
		IS_VALLEYVIEW(data->devid) ||
		IS_CHERRYVIEW(data->devid);
}

static bool size_ok(data_t *data, uint64_t size)
{
	/* Limit the big fb size based on available RAM or aperture size */
	float limit = data->async_flip_test ? 2.5f : 1.5f;

	if (uses_mappable(data) && size > data->mappable_size / limit)
		return false;

	if (uses_ggtt(data) && size > data->aper_size / limit)
		return false;

	if (size > data->ram_size / limit)
		return false;

	return true;
}


static void max_fb_size(data_t *data, int *width, int *height,
			uint32_t format, uint64_t modifier,
			igt_rotation_t rotation)
{
	int max_width, max_height;
	struct igt_fb fb;
	int i = 0;

	if (intel_display_ver(data->devid) < 13 && igt_fb_is_ccs_modifier(modifier)) {
		/* FIXME figure out what's correct */
		max_width = 8192;
		max_height = 8192;
	} else {
		max_width = data->max_fb_width;
		max_height = data->max_fb_height;
	}

	*width = max_width;
	*height = max_height;

	/*
	 * max fence stride is only 8k bytes on gen3 vs. 4k max fb width,
	 * and remapping isn't implemented currently on gen2/3.
	 */
	if (data->max_hw_stride_test || intel_display_ver(data->devid) < 4) {
		int cpp = igt_drm_format_to_bpp(format) / 8;

		if (igt_rotation_90_or_270(data->rotation)) {
			*height = min(*height, data->max_hw_stride_pixels);
			*height = min(*height, data->max_hw_stride_bytes / cpp);
		} else {
			*width = min(*width, data->max_hw_stride_pixels);
			*width = min(*width, data->max_hw_stride_bytes / cpp);
		}
	}

	igt_init_fb(&fb, data->drm_fd, *width, *height, format, modifier,
		    IGT_COLOR_YCBCR_BT709, IGT_COLOR_YCBCR_LIMITED_RANGE);
	igt_calc_fb_size(&fb);

	while (!size_ok(data, fb.size)) {
		if (data->max_hw_stride_test) {
			/* try to keep the hw plane stride at the max */
			if (igt_rotation_90_or_270(data->rotation))
				*width >>= 1;
			else
				*height >>= 1;
		} else {
			/* try to maintain roughly square dimensions */
			if (i++ & 1)
				*width >>= 1;
			else
				*height >>= 1;
		}

		igt_init_fb(&fb, data->drm_fd, *width, *height, format, modifier,
			    IGT_COLOR_YCBCR_BT709, IGT_COLOR_YCBCR_LIMITED_RANGE);
		igt_calc_fb_size(&fb);
	}

	igt_info("Max usable framebuffer size for format "IGT_FORMAT_FMT" / modifier 0x%"PRIx64": %dx%d (max reported %dx%d)\n",
		 IGT_FORMAT_ARGS(format), modifier,
		 *width, *height, max_width, max_height);
}

static void prep_small_fb(data_t *data, int width, int height)
{
	if (data->small_fb.fb_id &&
	    (data->small_fb.width != width ||
	     data->small_fb.height != height))
		igt_remove_fb(data->drm_fd, &data->small_fb);

	if (!data->small_fb.fb_id)
		igt_create_fb(data->drm_fd, width, height,
			      data->format, data->modifier, &data->small_fb);
}

static void prep_big_fb(data_t *data)
{
	if (data->big_fb.fb_id &&
	    (data->big_fb.width != data->big_fb_width ||
	     data->big_fb.height != data->big_fb_height)) {
		igt_remove_fb(data->drm_fd, &data->big_fb_solid);
		igt_remove_fb(data->drm_fd, &data->big_fb);
	}

	if (!data->big_fb.fb_id) {
		igt_create_fb(data->drm_fd,
			      data->big_fb_width, data->big_fb_height,
			      data->format, data->modifier,
			      &data->big_fb);
		generate_pattern(data, &data->big_fb, 640, 480);
	}

	if (data->async_flip_test && !data->big_fb_solid.fb_id) {
		igt_create_color_fb(data->drm_fd,
				    data->big_fb_width, data->big_fb_height,
				    data->format, data->modifier,
				    0.0, 1.0, 0.0,
				    &data->big_fb_solid);
	}
}

static void set_c8_lut(data_t *data)
{
	igt_pipe_t *pipe = &data->display.pipes[data->pipe];
	struct drm_color_lut *lut;
	int i, lut_size = 256;

	lut = calloc(lut_size, sizeof(lut[0]));

	/* igt_fb uses RGB332 for C8 */
	for (i = 0; i < lut_size; i++) {
		lut[i].red = ((i & 0xe0) >> 5) * 0xffff / 0x7;
		lut[i].green = ((i & 0x1c) >> 2) * 0xffff / 0x7;
		lut[i].blue = ((i & 0x03) >> 0) * 0xffff / 0x3;
	}

	igt_pipe_obj_replace_prop_blob(pipe, IGT_CRTC_GAMMA_LUT, lut,
				       lut_size * sizeof(lut[0]));

	free(lut);
}

static void unset_lut(data_t *data)
{
	igt_pipe_t *pipe = &data->display.pipes[data->pipe];

	igt_pipe_obj_replace_prop_blob(pipe, IGT_CRTC_GAMMA_LUT, NULL, 0);
}

static void wait_flip_event(data_t *data)
{
	drmEventContext evctx = {
		.version = 2,
	};
	struct pollfd pfd = {
		.fd = data->drm_fd,
		.events = POLLIN,
	};
	int ret;

	ret = poll(&pfd, 1, -1);
	igt_assert_eq(ret, 1);

	ret = drmHandleEvent(data->drm_fd, &evctx);
	igt_assert_eq(ret, 0);
}

static unsigned int mode_frame_time(const drmModeModeInfo *mode)
{
	return 1000ULL * mode->htotal * mode->vtotal / mode->clock;
}

static void do_async_flip(data_t *data)
{
	const drmModeModeInfo *mode = igt_output_get_mode(data->output);
	int ret;

	/* first async flip maybe be turned into a sync flip, keep the solid fb */
	ret = drmModePageFlip(data->drm_fd, data->output->config.crtc->crtc_id,
			      data->big_fb_solid.fb_id,
			      DRM_MODE_PAGE_FLIP_ASYNC | DRM_MODE_PAGE_FLIP_EVENT, data);
	igt_assert_eq(ret, 0);
	wait_flip_event(data);

	/* try to make sure we see the tear, for a bit of visual feedback only */
	igt_wait_for_vblank(data->drm_fd, data->pipe);
	usleep(mode_frame_time(mode) * (mode->vtotal - mode->vdisplay * 2 / 3) / mode->vtotal);

	/* first guaranteed real async flip, now change to the patterned fb */
	ret = drmModePageFlip(data->drm_fd, data->output->config.crtc->crtc_id,
			      data->big_fb.fb_id,
			      DRM_MODE_PAGE_FLIP_ASYNC | DRM_MODE_PAGE_FLIP_EVENT, data);
	igt_assert_eq(ret, 0);
	wait_flip_event(data);

	/* next frame should have the correct crc */
	igt_wait_for_vblank(data->drm_fd, data->pipe);
}

static bool test_plane(data_t *data)
{
	igt_plane_t *plane = data->plane;
	struct igt_fb *small_fb = &data->small_fb;
	int w = data->big_fb_width - small_fb->width;
	int h = data->big_fb_height - small_fb->height;
	bool run_in_simulation = igt_run_in_simulation();
	struct {
		int x, y;
	} coords[] = {
		/* bunch of coordinates pulled out of thin air */
		{ 0, 0, },
		{ w * 4 / 7, h / 5, },
		{ w * 3 / 7, h / 3, },
		{ w / 2, h / 2, },
		{ w / 3, h * 3 / 4, },
		{ w, h, },
	};

	if (!igt_plane_has_format_mod(plane, data->format, data->modifier))
		return false;

	if (!igt_plane_has_rotation(plane, data->rotation))
		return false;

	if (igt_plane_has_prop(plane, IGT_PLANE_ROTATION))
		igt_plane_set_rotation(plane, data->rotation);
	igt_plane_set_position(plane, 0, 0);

	for (int i = 0; i < ARRAY_SIZE(coords); i++) {
		igt_crc_t small_crc, big_crc;
		struct igt_fb *big_fb;
		int x, y;

		if (run_in_simulation)
			i = ARRAY_SIZE(coords) - 1;

		x = coords[i].x;
		y = coords[i].y;

		/* Hardware limitation */
		if ((data->format == DRM_FORMAT_RGB565 &&
		     igt_rotation_90_or_270(data->rotation)) ||
		    igt_format_is_yuv_semiplanar(data->format)) {
			x &= ~1;
			y &= ~1;
		}

		igt_plane_set_fb(plane, small_fb);
		igt_plane_set_size(plane, data->width, data->height);

		/*
		 * Try to check that the rotation+format+modifier
		 * combo is supported.
		 */
		if (i == 0 && data->display.is_atomic &&
		    igt_display_try_commit_atomic(&data->display,
						  DRM_MODE_ATOMIC_ALLOW_MODESET |
						  DRM_MODE_ATOMIC_TEST_ONLY,
						  NULL) != 0) {
			if (igt_plane_has_prop(plane, IGT_PLANE_ROTATION))
				igt_plane_set_rotation(plane, IGT_ROTATION_0);
			igt_plane_set_fb(plane, NULL);
			return false;
		}

		/*
		 * To speed up skips we delay the big fb creation until
		 * the above rotation related check has been performed.
		 */
		prep_big_fb(data);

		/*
		 * Make a 1:1 copy of the desired part of the big fb
		 * rather than try to render the same pattern (translated
		 * accordinly) again via cairo. Something in cairo's
		 * rendering pipeline introduces slight differences into
		 * the result if we try that, and so the crc will not match.
		 */
		copy_pattern(data, small_fb, 0, 0, &data->big_fb, x, y,
			     small_fb->width, small_fb->height);

		igt_display_commit2(&data->display, data->display.is_atomic ?
				    COMMIT_ATOMIC : COMMIT_UNIVERSAL);
		igt_pipe_crc_start(data->pipe_crc);
		igt_pipe_crc_get_current(data->display.drm_fd, data->pipe_crc, &small_crc);

		if (data->async_flip_test)
			big_fb = &data->big_fb_solid;
		else
			big_fb = &data->big_fb;

		igt_plane_set_fb(plane, big_fb);
		igt_fb_set_position(big_fb, plane, x, y);
		igt_fb_set_size(big_fb, plane, small_fb->width, small_fb->height);
		igt_plane_set_size(plane, data->width, data->height);
		igt_display_commit2(&data->display, data->display.is_atomic ?
				    COMMIT_ATOMIC : COMMIT_UNIVERSAL);

		if (data->async_flip_test)
			do_async_flip(data);

		igt_pipe_crc_get_current(data->display.drm_fd, data->pipe_crc, &big_crc);

		igt_plane_set_fb(plane, NULL);

		igt_assert_crc_equal(&big_crc, &small_crc);
		igt_pipe_crc_stop(data->pipe_crc);
	}

	return true;
}

static bool test_pipe(data_t *data)
{
	uint16_t width, height;
	drmModeModeInfo *mode;
	igt_plane_t *primary;
	bool ret = false;
	bool run_in_simulation = igt_run_in_simulation();

	igt_info("Using (pipe %s + %s) to run the subtest.\n",
		 kmstest_pipe_name(data->pipe), igt_output_name(data->output));

	if (data->format == DRM_FORMAT_C8 &&
	    !igt_pipe_obj_has_prop(&data->display.pipes[data->pipe],
				   IGT_CRTC_GAMMA_LUT))
		return false;

	data->ibb = intel_bb_create(data->drm_fd, 4096);

	mode = igt_output_get_mode(data->output);

	data->width = mode->hdisplay;
	data->height = mode->vdisplay;

	width = mode->hdisplay;
	height = mode->vdisplay;
	if (igt_rotation_90_or_270(data->rotation))
		igt_swap(width, height);

	prep_small_fb(data, width, height);

	igt_output_set_pipe(data->output, data->pipe);

	primary = igt_output_get_plane_type(data->output, DRM_PLANE_TYPE_PRIMARY);
	igt_plane_set_fb(primary, NULL);

	if (!data->display.is_atomic) {
		struct igt_fb fb;

		igt_create_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
			      DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR,
			      &fb);

		/* legacy setcrtc needs an fb */
		igt_plane_set_fb(primary, &fb);
		igt_display_commit2(&data->display, COMMIT_LEGACY);

		igt_plane_set_fb(primary, NULL);
		igt_display_commit2(&data->display, COMMIT_UNIVERSAL);

		igt_remove_fb(data->drm_fd, &fb);
	}

	if (data->format == DRM_FORMAT_C8)
		set_c8_lut(data);

	igt_display_commit2(&data->display, data->display.is_atomic ?
			    COMMIT_ATOMIC : COMMIT_UNIVERSAL);

	data->pipe_crc = igt_pipe_crc_new(data->drm_fd, data->pipe,
					  IGT_PIPE_CRC_SOURCE_AUTO);

	for_each_plane_on_pipe(&data->display, data->pipe, data->plane) {
		ret = test_plane(data);
		if (ret || run_in_simulation)
			break;
	}

	if (data->format == DRM_FORMAT_C8)
		unset_lut(data);

	intel_bb_destroy(data->ibb);

	return ret;
}

static void test_scanout(data_t *data)
{
	igt_require(data->format == DRM_FORMAT_C8 ||
		    igt_fb_supported_format(data->format));

	igt_require(igt_display_has_format_mod(&data->display, data->format, data->modifier));

	max_fb_size(data, &data->big_fb_width, &data->big_fb_height,
		    data->format, data->modifier, data->rotation);

	for_each_pipe_with_valid_output(&data->display, data->pipe, data->output) {
		igt_display_reset(&data->display);

		igt_output_set_pipe(data->output, data->pipe);
		if (!intel_pipe_output_combo_valid(&data->display))
			continue;

		if (test_pipe(data))
			return;
		break;
	}

	igt_skip("unsupported configuration\n");
}

static void
test_size_overflow(data_t *data)
{
	uint32_t fb_id;
	uint32_t bo;
	uint32_t offsets[4] = {};
	uint32_t strides[4] = { 256*1024, };
	int ret;

	igt_require(igt_display_has_format_mod(&data->display,
					       DRM_FORMAT_XRGB8888,
					       data->modifier));

	/*
	 * Try to hit a specific integer overflow in intel fb size
	 * calculations. 256k * 16k == 1<<32 which is checked
	 * against the bo size. The check should fail on account
	 * of the bo being smaller, but due to the overflow the
	 * computed fb size is 0 and thus the check never trips.
	 */
	igt_require(data->max_fb_width >= 16383 &&
		    data->max_fb_height >= 16383);

	if (is_i915_device(data->drm_fd))
		bo = gem_buffer_create_fb_obj(data->drm_fd, (1ULL << 32) - 4096);
	else
		bo = xe_bo_create(data->drm_fd, 0,
				  ALIGN(((1ULL << 32) - 4096),
					xe_get_default_alignment(data->drm_fd)),
				  vram_if_possible(data->drm_fd, 0), 0);
	igt_require(bo);

	ret = __kms_addfb(data->drm_fd, bo,
			  16383, 16383,
			  DRM_FORMAT_XRGB8888,
			  data->modifier,
			  strides, offsets, 1,
			  DRM_MODE_FB_MODIFIERS, &fb_id);

	igt_assert_neq(ret, 0);

	gem_close(data->drm_fd, bo);
}

static void
test_size_offset_overflow(data_t *data)
{
	uint32_t fb_id;
	uint32_t bo;
	uint32_t offsets[4] = {};
	uint32_t strides[4] = { 8192, };
	int ret;

	igt_require(igt_display_has_format_mod(&data->display,
					       DRM_FORMAT_NV12,
					       data->modifier));

	/*
	 * Try to hit a specific integer overflow in intel fb size
	 * calculations. This time it's offsets[1] + the tile
	 * aligned chroma plane size that overflows and
	 * incorrectly passes the bo size check.
	 */
	igt_require(igt_display_has_format_mod(&data->display,
					       DRM_FORMAT_NV12,
					       data->modifier));

	if (is_i915_device(data->drm_fd))
		bo = gem_buffer_create_fb_obj(data->drm_fd, (1ULL << 32) - 4096);
	else
		bo = xe_bo_create(data->drm_fd, 0,
				  ALIGN(((1ULL << 32) - 4096),
					xe_get_default_alignment(data->drm_fd)),
				  vram_if_possible(data->drm_fd, 0), 0);
	igt_require(bo);

	offsets[0] = 0;
	offsets[1] = (1ULL << 32) - 8192 * 4096;

	ret = __kms_addfb(data->drm_fd, bo,
			  8192, 8188,
			  DRM_FORMAT_NV12,
			  data->modifier,
			  strides, offsets, 1,
			  DRM_MODE_FB_MODIFIERS, &fb_id);
	igt_assert_neq(ret, 0);

	gem_close(data->drm_fd, bo);
}

static int rmfb(int fd, uint32_t id)
{
	int err;

	err = 0;
	if (igt_ioctl(fd, DRM_IOCTL_MODE_RMFB, &id))
		err = -errno;

	errno = 0;
	return err;
}

static void
test_addfb(data_t *data)
{
	struct igt_fb fb;
	uint32_t fb_id;
	uint32_t bo;
	uint32_t format;
	int width, height;
	int ret;

	/*
	 * gen3 max tiled stride is 8k bytes, but
	 * max fb size of 4k pixels, hence we can't test
	 * with 32bpp and must use 16bpp instead.
	 */
	if (intel_display_ver(data->devid) == 3)
		format = DRM_FORMAT_RGB565;
	else
		format = DRM_FORMAT_XRGB8888;

	igt_require(igt_display_has_format_mod(&data->display,
					       format, data->modifier));

	max_fb_size(data, &width, &height, format, data->modifier,
		    IGT_ROTATION_0);

	igt_init_fb(&fb, data->drm_fd, width, height,
		    format, data->modifier,
		    IGT_COLOR_YCBCR_BT709, IGT_COLOR_YCBCR_LIMITED_RANGE);
	igt_calc_fb_size(&fb);

	if (is_i915_device(data->drm_fd))
		bo = gem_buffer_create_fb_obj(data->drm_fd, fb.size);
	else
		bo = xe_bo_create(data->drm_fd, 0,
				  ALIGN(fb.size, xe_get_default_alignment(data->drm_fd)),
				  vram_if_possible(data->drm_fd, 0), 0);
	igt_require(bo);

	if (is_i915_device(data->drm_fd) && intel_display_ver(data->devid) < 4)
		gem_set_tiling(data->drm_fd, bo,
			       igt_fb_mod_to_tiling(data->modifier), fb.strides[0]);

	ret = __kms_addfb(data->drm_fd, bo,
			  width, height,
			  format, data->modifier,
			  fb.strides, fb.offsets, fb.num_planes,
			  DRM_MODE_FB_MODIFIERS, &fb_id);
	igt_assert_eq(ret, 0);

	rmfb(data->drm_fd, fb_id);
	gem_close(data->drm_fd, bo);
}

static void intel_max_hw_stride(uint32_t devid,
				uint64_t modifier,
				int *pixels, int *bytes)
{
	/* should be kept in sync with the kernel imposed limits */
	if (intel_display_ver(devid) >= 13) {
		*pixels = 65536;
		*bytes = 128 * 1024;
	} else if (intel_display_ver(devid) == 12) {
		*pixels = 8192;
		*bytes = 64 * 1024;
	} else if (intel_display_ver(devid) == 11) {
		*pixels = 8192;
		*bytes = 64 * 1024;
		if (modifier == DRM_FORMAT_MOD_LINEAR)
			*bytes -= 64;
	} else if (intel_display_ver(devid) >= 9 ||
		   IS_BROADWELL(devid) || IS_HASWELL(devid)) {
		*pixels = 8192;
		*bytes = 32 * 1024;
	} else if (intel_display_ver(devid) >= 4) {
		if (modifier == I915_FORMAT_MOD_X_TILED)
			*pixels = 4096;
		else
			*pixels = INT_MAX;
		*bytes = 32 * 1024;
	} else if (intel_display_ver(devid) == 3) {
		*pixels = INT_MAX;
		if (modifier == I915_FORMAT_MOD_X_TILED)
			*bytes = 8 * 1024;
		else
			*bytes = 16 * 1024;
	} else if (intel_display_ver(devid) == 2) {
		*pixels = INT_MAX;
		*bytes = 8 * 1024;
	}
}

static void test_cleanup(data_t *data)
{
	if (!data->output)
		return;

	igt_pipe_crc_free(data->pipe_crc);
	igt_output_set_pipe(data->output, PIPE_NONE);
	igt_remove_fb(data->drm_fd, &data->big_fb_solid);
	igt_remove_fb(data->drm_fd, &data->big_fb);
	igt_remove_fb(data->drm_fd, &data->small_fb);

	data->output = NULL;
}

static bool has_async_flip(data_t *data)
{
	/*
	 * TODO: preferably probe all this stuff with
	 * TEST_ONLY rather than hardcoding it...
	 */
	if (igt_fb_is_ccs_modifier(data->modifier))
		return false;

	if (igt_format_is_yuv_semiplanar(data->format))
		return false;

	if (intel_display_ver(data->devid) < 12 &&
	    data->modifier == DRM_FORMAT_MOD_LINEAR)
		return false;

	return igt_has_drm_cap(data->drm_fd, DRM_CAP_ASYNC_PAGE_FLIP);
}

static data_t data = {};

static const struct {
	uint64_t modifier;
	const char *name;
} modifiers[] = {
	{ DRM_FORMAT_MOD_LINEAR, "linear", },
	{ I915_FORMAT_MOD_X_TILED, "x-tiled", },
	{ I915_FORMAT_MOD_Y_TILED, "y-tiled", },
	{ I915_FORMAT_MOD_Yf_TILED, "yf-tiled", },
	{ I915_FORMAT_MOD_4_TILED, "4-tiled", },
	{ I915_FORMAT_MOD_Y_TILED_CCS, "y-tiled-ccs", },
	{ I915_FORMAT_MOD_Yf_TILED_CCS, "yf-tiled-ccs", },
	{ I915_FORMAT_MOD_Y_TILED_GEN12_RC_CCS, "y-tiled-rc-ccs", },
	{ I915_FORMAT_MOD_Y_TILED_GEN12_RC_CCS_CC, "y-tiled-rc-ccs-cc", },
	{ I915_FORMAT_MOD_4_TILED_MTL_RC_CCS, "4-tiled-mtl-rc-ccs", },
	{ I915_FORMAT_MOD_4_TILED_MTL_RC_CCS_CC, "4-tiled-mtl-rc-ccs-cc", },
	{ I915_FORMAT_MOD_4_TILED_DG2_RC_CCS, "4-tiled-dg2-rc-ccs", },
	{ I915_FORMAT_MOD_4_TILED_DG2_RC_CCS_CC, "4-tiled-dg2-rc-ccs-cc", },
};

static const struct {
	uint32_t format;
	const char *name;
} formats[] = {
	{ DRM_FORMAT_C8, "8bpp", },
	{ DRM_FORMAT_RGB565, "16bpp", },
	{ DRM_FORMAT_XRGB8888, "32bpp", },
	{ DRM_FORMAT_XBGR16161616F, "64bpp", },
	{ DRM_FORMAT_NV12, "nv12", },
	{ DRM_FORMAT_P016, "p016", },
};

static const igt_rotation_t rotations[] = {
	IGT_ROTATION_0,
	IGT_ROTATION_0 | IGT_REFLECT_X,
	IGT_ROTATION_180,
	IGT_ROTATION_180 | IGT_REFLECT_X,
	IGT_ROTATION_90,
	IGT_ROTATION_90 | IGT_REFLECT_X,
	IGT_ROTATION_270,
	IGT_ROTATION_270 | IGT_REFLECT_X,
};

static const char *rotation_flip_str(igt_rotation_t rotation)
{
	return rotation & IGT_REFLECT_X ? "-hflip" : "";
}

igt_main
{
	igt_fixture {
		drmModeResPtr res;

		data.drm_fd = drm_open_driver_master(DRIVER_INTEL | DRIVER_XE);

		igt_require(is_intel_device(data.drm_fd));

		data.devid = intel_get_drm_devid(data.drm_fd);

		kmstest_set_vt_graphics_mode();

		igt_require_pipe_crc(data.drm_fd);
		igt_display_require(&data.display, data.drm_fd);
		igt_display_require_output(&data.display);

		res = drmModeGetResources(data.drm_fd);
		igt_assert(res);

		data.max_fb_width = res->max_width;
		data.max_fb_height = res->max_height;

		drmModeFreeResources(res);

		igt_info("Max driver framebuffer size %dx%d\n",
			 data.max_fb_width, data.max_fb_height);

		data.ram_size = igt_get_total_ram_mb() << 20;

		if (is_i915_device(data.drm_fd))
			data.aper_size = gem_aperture_size(data.drm_fd);
		else
			data.aper_size = (1ULL << xe_va_bits(data.drm_fd));
		data.mappable_size = gem_mappable_aperture_size(data.drm_fd);

		igt_info("RAM: %"PRIu64" MiB, GPU address space: %"PRId64" MiB, GGTT mappable size: %"PRId64" MiB\n",
			 data.ram_size >> 20, data.aper_size >> 20,
			 data.mappable_size >> 20);

		/*
		 * Gen3 render engine is limited to 2kx2k, whereas
		 * the display engine can do 4kx4k. Use the blitter
		 * on gen3 to avoid exceeding the render engine limits.
		 * On gen2 we could use either, but let's go for the
		 * blitter there as well.
		 */
		if (intel_display_ver(data.devid) >= 4)
			data.render_copy = igt_get_render_copyfunc(data.devid);

		data.bops = buf_ops_create(data.drm_fd);
	}

	/*
	 * Skip linear as it doesn't hit the overflow we want
	 * on account of the tile height being effectively one,
	 * and thus the kenrnel rounding up to the next tile
	 * height won't do anything.
	 */
	igt_describe("Sanity check if addfb ioctl fails correctly for given modifier with small bo");
	for (int i = 1; i < ARRAY_SIZE(modifiers); i++) {
		igt_subtest_f("%s-addfb-size-overflow",
			      modifiers[i].name) {
			data.modifier = modifiers[i].modifier;
			test_size_overflow(&data);
		}
	}

	igt_describe("Sanity check if addfb ioctl fails correctly for given modifier and offsets with small bo");
	for (int i = 1; i < ARRAY_SIZE(modifiers); i++) {
		igt_subtest_f("%s-addfb-size-offset-overflow",
			      modifiers[i].name) {
			data.modifier = modifiers[i].modifier;
			test_size_offset_overflow(&data);
		}
	}

	igt_describe("Sanity check if addfb ioctl works correctly for given size and strides of fb");
	for (int i = 0; i < ARRAY_SIZE(modifiers); i++) {
		igt_subtest_f("%s-addfb", modifiers[i].name) {
			data.modifier = modifiers[i].modifier;
			test_addfb(&data);
		}
	}

	for (int i = 0; i < ARRAY_SIZE(modifiers); i++) {
		data.modifier = modifiers[i].modifier;

		for (int j = 0; j < ARRAY_SIZE(formats); j++) {
			data.format = formats[j].format;

			for (int k = 0; k < ARRAY_SIZE(rotations); k++) {
				data.rotation = rotations[k];

				igt_describe("Sanity check if scanout of big framebuffers works "
					     "correctly for given combination of modifier formats "
					     "and rotation");
				igt_subtest_f("%s-%s-rotate-%s%s",
					      modifiers[i].name, formats[j].name,
					      igt_plane_rotation_name(data.rotation),
					      rotation_flip_str(data.rotation))
					test_scanout(&data);

				data.async_flip_test = true;
				igt_describe("Sanity check if scanout of big framebuffers works "
					     "correctly for given combination of modifier formats "
					     "and rotation, using async flips");
				igt_subtest_f("%s-%s-rotate-%s%s-async-flip",
					      modifiers[i].name, formats[j].name,
					      igt_plane_rotation_name(data.rotation),
					      rotation_flip_str(data.rotation)) {
					igt_require(has_async_flip(&data));
					test_scanout(&data);
				}
				data.async_flip_test = false;
			}

			igt_fixture
				test_cleanup(&data);
		}
	}

	data.max_hw_stride_test = true;
	for (int i = 0; i < ARRAY_SIZE(modifiers); i++) {
		data.modifier = modifiers[i].modifier;

		intel_max_hw_stride(data.devid, data.modifier,
				    &data.max_hw_stride_pixels,
				    &data.max_hw_stride_bytes);

		for (int j = 0; j < ARRAY_SIZE(formats); j++) {
			data.format = formats[j].format;

			for (int k = 0; k < ARRAY_SIZE(rotations); k++) {
				data.rotation = rotations[k];

				igt_describe("Test maximum hardware stride for given format and modifier.");
				igt_subtest_f("%s-max-hw-stride-%s-rotate-%s%s",
					      modifiers[i].name, formats[j].name,
					      igt_plane_rotation_name(data.rotation),
					      rotation_flip_str(data.rotation)) {
					igt_require(intel_display_ver(intel_get_drm_devid(data.drm_fd)) >= 5);
					test_scanout(&data);
				}

				data.async_flip_test = true;
				igt_describe("Test maximum hardware stride for given format and modifier, using async flips.");
				igt_subtest_f("%s-max-hw-stride-%s-rotate-%s%s-async-flip",
					      modifiers[i].name, formats[j].name,
					      igt_plane_rotation_name(data.rotation),
					      rotation_flip_str(data.rotation)) {
					igt_require(has_async_flip(&data));
					test_scanout(&data);
				}
				data.async_flip_test = false;

				igt_fixture
					test_cleanup(&data);
			}
		}
	}
	data.max_hw_stride_test = false;

	igt_fixture {
		igt_display_fini(&data.display);
		buf_ops_destroy(data.bops);
		drm_close_driver(data.drm_fd);
	}
}

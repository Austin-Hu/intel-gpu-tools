#include "igt.h"

typedef struct {
	int drm_fd;
	igt_display_t display;
	igt_output_t *output;
	igt_crtc_t *crtc;
} data_t;

static const drmModeModeInfo *pick_mode(igt_output_t *output)
{
	static const drmModeModeInfo mode = {
		//.clock = 297000,
		.clock = 284000,
		//.clock = 234000,
		.hdisplay = 3840,
		.hsync_start = 4016,
		.hsync_end = 4104,
		.htotal = 4400,
		.vdisplay = 2160,
		.vsync_start = 2168,
		.vsync_end = 2178,
		.vtotal = 2250,
		.flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
	};

	return &mode;
}

static void test(data_t *data)
{
	struct igt_fb fb = {};
	const drmModeModeInfo *mode;
	int sw, sh, dw, dh;
	igt_plane_t *plane;
	uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET;

	// NOTE: with 4K@60Hz setting for some external panel which doesn't support
	// the timing would induce pipe B FIFO underrun during testing
	// mode = pick_mode(data->output);
	mode = igt_output_get_mode(data->output);
	if (!mode) {
		igt_critical("Couldn't get the current mode of output %s\n",
				data->output->name);
		return;
	}

	// Change some timing values of current mode to override and test.
	// mode->clock = 297000;
	// mode->clock = 234000;
	// mode->flags |= DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC;

	igt_output_override_mode(data->output, mode);

	igt_create_pattern_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
			      DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR, &fb);

	igt_output_set_crtc(data->output, data->crtc);

	sw = fb.width;
	sh = fb.height;

	dw = mode->hdisplay;
	dh = mode->vdisplay;

	plane = igt_output_get_plane_type(data->output,
					    DRM_PLANE_TYPE_PRIMARY);
	igt_plane_set_fb(plane, &fb);

	igt_fb_set_size(&fb, plane, sw, sh);

	while (dw >= 8 && dh >= 8) {
		float hscale = (float)sw/dw;
		float vscale = (float)sh/dh;

		igt_plane_set_size(plane, dw, dh);

		igt_reset_fifo_underrun_reporting(data->drm_fd);
		if (!igt_display_try_commit_atomic(&data->display,
					flags | DRM_MODE_ATOMIC_TEST_ONLY, NULL)) {
			igt_display_commit_atomic(&data->display, flags, NULL);

			igt_info("scale %f %f (rate %d)\n", hscale, vscale,
					(int)(mode->clock * hscale * vscale / 2.0f));
		} else
			igt_info("scale %f %f (rate %d) atomic test failed.\n", hscale,
					vscale, (int)(mode->clock * hscale * vscale / 2.0f));

		dw -= 4;
		//dh -= 1;
	}

	igt_plane_set_fb(plane, NULL);
	igt_output_set_crtc(data->output, NULL);
	igt_output_override_mode(data->output, NULL);
	igt_display_commit_atomic(&data->display, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);

	igt_remove_fb(data->drm_fd, &fb);
}

static void run_test(data_t *data)
{
	for_each_crtc_with_single_output(&data->display, data->crtc, data->output) {
		if (output_is_internal_panel(data->output))
			continue;

		test(data);
		break;
	}
}

static data_t data;

int igt_simple_main()
{
	data.drm_fd = drm_open_driver_master(DRIVER_ANY);

	kmstest_set_vt_graphics_mode();

	igt_display_require(&data.display, data.drm_fd);

	run_test(&data);

	igt_display_fini(&data.display);
	drm_close_driver(data.drm_fd);
}

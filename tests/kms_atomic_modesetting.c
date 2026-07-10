#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <xf86drmMode.h>
#include <cairo.h>
#include "drm.h"
#include "ioctl_wrappers.h"
#include "drmtest.h"
#include "igt.h"
#include "igt_aux.h"
#include "sw_sync.h"

IGT_TEST_DESCRIPTION("Concurrently stress page-flips and external modesetting via threads sharing DRM Master.");

/* Global shared context containing the unified state tracking cache */
typedef struct {
	int drm_fd;
	igt_display_t display;
	igt_fb_t base_fb;
} shared_data_t;

/* Mode parameter structure for external display switching */
typedef struct {
	int width;
	int height;
	int vrefresh;
	unsigned int clock;
} mode_param_t;

static uint32_t plane_get_igt_format(igt_plane_t *plane)
{
	drmModePlanePtr plane_kms = plane->drm_plane;
	for (int i = 0; i < plane_kms->count_formats; i++) {
		if (igt_fb_supported_format(plane_kms->formats[i]))
			return plane_kms->formats[i];
	}
	return 0;
}

/* --- Thread 1: Page Flip & Plane Stress Loop --- */
static void *atomic_stress_thread(void *arg)
{
	shared_data_t *shared = (shared_data_t *)arg;

	igt_plane_t *plane;
	struct igt_fb fbs[32][16];
	int num_planes = 0;
	int num_fbs[32] = {0};
	igt_plane_t *planes[32];
	igt_output_t *output;
	int w[32] = {0};
	int h[32] = {0};

	uint64_t modifiers[] = {
		DRM_FORMAT_MOD_LINEAR,
		I915_FORMAT_MOD_X_TILED,
		I915_FORMAT_MOD_Y_TILED,
		I915_FORMAT_MOD_Y_TILED_CCS,
		I915_FORMAT_MOD_Yf_TILED,
		I915_FORMAT_MOD_Yf_TILED_CCS,
		I915_FORMAT_MOD_Y_TILED_GEN12_RC_CCS,
		I915_FORMAT_MOD_4_TILED,
		I915_FORMAT_MOD_4_TILED_MTL_RC_CCS,
		I915_FORMAT_MOD_4_TILED_DG2_RC_CCS,
	};

	for_each_connected_output(&shared->display, output) {
		igt_crtc_t *crtc = igt_output_get_driving_crtc(output);
		if (!crtc) continue;

		drmModeModeInfo *mode = igt_output_get_mode(output);

		for_each_plane_on_crtc(crtc, plane) {
			uint32_t format = plane_get_igt_format(plane);
			if (!format || plane->type == DRM_PLANE_TYPE_CURSOR)
				continue;

			planes[num_planes] = plane;
			w[num_planes] = mode->hdisplay;
			h[num_planes] = mode->vdisplay;

			for (int m = 0; m < ARRAY_SIZE(modifiers); m++) {
				if (igt_plane_has_format_mod(plane, format, modifiers[m])) {
					igt_create_color_pattern_fb(shared->drm_fd,
							w[num_planes] / (2 + (m % 3)), h[num_planes] / (2 + (m % 3)),
							format, modifiers[m],
							0.1 * m, 0.2 * m, 0.3 * m,
							&fbs[num_planes][num_fbs[num_planes]]);
					num_fbs[num_planes]++;
				}
			}

			igt_plane_set_fb(plane, NULL);
			num_planes++;
		}
	}

	if (!igt_display_try_commit_atomic(&shared->display, DRM_MODE_ATOMIC_TEST_ONLY, NULL))
		igt_display_try_commit_atomic(&shared->display, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);

	struct {
		struct igt_fb *fb;
		int x, y, w, h;
	} current_state[32] = {0};

	int successful_commits = 0;
	int failed_commits = 0;

	while (true) {
		if (num_planes == 0) break;

		int p_idx = rand() % num_planes;
		igt_plane_t *p = planes[p_idx];

		struct igt_fb *old_fb = current_state[p_idx].fb;
		int old_x = current_state[p_idx].x;
		int old_y = current_state[p_idx].y;
		int old_w = current_state[p_idx].w;
		int old_h = current_state[p_idx].h;

		int fb_idx = rand() % (num_fbs[p_idx] + 1);
		struct igt_fb *new_fb = NULL;
		int dest_w = 0, dest_h = 0, dest_x = 0, dest_y = 0;

		if (fb_idx < num_fbs[p_idx]) {
			new_fb = &fbs[p_idx][fb_idx];

			if (rand() % 3 == 0) {
				dest_w = new_fb->width;
				dest_h = new_fb->height;
			} else {
				dest_w = (rand() % (w[p_idx] / 2)) + 16;
				dest_h = (rand() % (h[p_idx] / 2)) + 16;
			}

			dest_x = rand() % (w[p_idx] - dest_w + 1);
			dest_y = rand() % (h[p_idx] - dest_h + 1);
		} else if (p->type == DRM_PLANE_TYPE_PRIMARY) {
			new_fb = old_fb ? old_fb : &fbs[p_idx][0];
			dest_w = old_w ? old_w : new_fb->width;
			dest_h = old_h ? old_h : new_fb->height;
			dest_x = old_x;
			dest_y = old_y;
		}

		igt_plane_set_fb(p, new_fb);
		if (new_fb) {
			igt_plane_set_position(p, dest_x, dest_y);
			igt_plane_set_size(p, dest_w, dest_h);
		}

		for (int i = 0; i < num_planes; i++) {
			if (i != p_idx) {
				igt_plane_set_fb(planes[i], current_state[i].fb);
				if (current_state[i].fb) {
					igt_plane_set_position(planes[i], current_state[i].x, current_state[i].y);
					igt_plane_set_size(planes[i], current_state[i].w, current_state[i].h);
				}
			}
		}

		igt_reset_fifo_underrun_reporting(shared->drm_fd);

		if (!igt_display_try_commit_atomic(&shared->display, DRM_MODE_ATOMIC_TEST_ONLY, NULL) &&
			!igt_display_try_commit_atomic(&shared->display, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL))
		{
			current_state[p_idx].fb = new_fb;
			current_state[p_idx].x = dest_x;
			current_state[p_idx].y = dest_y;
			current_state[p_idx].w = dest_w;
			current_state[p_idx].h = dest_h;
			successful_commits++;
		} else {
			/* Revert properties in the shared display cache so they don't pollute Thread 2 */
			igt_plane_set_fb(p, old_fb);
			if (old_fb){
				igt_plane_set_position(p, old_x, old_y);
				igt_plane_set_size(p, old_w, old_h);
			}

			current_state[p_idx].fb = old_fb;
			current_state[p_idx].x = old_x;
			current_state[p_idx].y = old_y;
			current_state[p_idx].w = old_w;
			current_state[p_idx].h = old_h;
			failed_commits++;
		}
	}

	for (int i = 0; i < num_planes; i++) {
		for (int j = 0; j < num_fbs[i]; j++) {
			igt_remove_fb(shared->drm_fd, &fbs[i][j]);
		}
	}

	return NULL;
}

/* --- Thread 2: External Display Modesetting Loop --- */
static void *modeset_external_thread(void *arg)
{
	shared_data_t *shared = (shared_data_t *)arg;
	igt_output_t *output;
	igt_crtc_t *crtc;

	mode_param_t targets[] = {
		{1920, 1080, 120, 296703},
		{3840, 2160, 60, 542260}
	};
	int target_idx = 0;

	while (true) {
		sleep(5); /* 5 seconds loop interval */

		bool external_found = false;

		for_each_crtc_with_single_output(&shared->display, crtc, output) {
			if (output_is_internal_panel(output))
				continue;

			external_found = true;
			mode_param_t active = targets[target_idx];
			drmModeModeInfo *chosen_mode = NULL;

			for (int i = 0; i < output->config.connector->count_modes; i++) {
				drmModeModeInfo *m = &output->config.connector->modes[i];
				if (m->hdisplay == active.width && m->vdisplay == active.height &&
				    m->vrefresh == active.vrefresh && m->clock == active.clock) {
					chosen_mode = m;
					break;
				}
			}

			if (chosen_mode) {
				igt_info("Mode Setting Thread: Committing mode %dx%d@%dHz (clock = %u) on output %s\n",
					 active.width, active.height, active.vrefresh, active.clock, output->name);
				igt_output_override_mode(output, chosen_mode);
				igt_output_set_crtc(output, crtc);
				if (igt_display_try_commit_atomic(&shared->display, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL))
					igt_warn("Mode Setting Thread: Atomic modeset commit with mode %dx%d@%dHz (clock = %u) on output %s rejected by kernel.\n", 
						 active.width, active.height, active.vrefresh, active.clock, output->name);
			} else {
				igt_warn("Mode Setting Thread: Mode %dx%d@%dHz (clock = %u) not found on output %s\n",
					 active.width, active.height, active.vrefresh, active.clock, output->name);
			}
			break; 
		}

		if (!external_found) {
			igt_info("Thread 2: No active external displays found to switch modes on.\n");
		}

		target_idx = (target_idx + 1) % ARRAY_SIZE(targets);
	}

	return NULL;
}

/* --- Main Framework Setup Anchor --- */
int main(int argc, char **argv)
{
	shared_data_t data = { 0 };

	igt_subtest_init(argc, argv);

	data.drm_fd = drm_open_driver_master(DRIVER_ANY);
	kmstest_set_vt_graphics_mode();

	igt_display_require(&data.display, data.drm_fd);
	igt_require(data.display.is_atomic);
	igt_display_require_output(&data.display);

	int valid_outputs = 0;
	igt_display_reset(&data.display);

	/* Initialize mappings on the main shared structure context */
	for (int i = 0; i < data.display.n_crtcs; i++) {
		igt_crtc_t *cur_crtc = &data.display.crtcs[i];
		for (int j = 0; j < data.display.n_outputs; j++) {
			igt_output_t *cur_output = &data.display.outputs[j];
			if (!igt_output_is_connected(cur_output) || igt_output_get_driving_crtc(cur_output))
				continue;

			igt_output_set_crtc(cur_output, cur_crtc);
			if (intel_pipe_output_combo_valid(&data.display)) {
				igt_plane_t *primary = igt_crtc_get_plane_type(cur_crtc, DRM_PLANE_TYPE_PRIMARY);
				drmModeModeInfo *mode = igt_output_get_mode(cur_output);

				igt_create_pattern_fb(data.drm_fd, mode->hdisplay, mode->vdisplay,
								plane_get_igt_format(primary), DRM_FORMAT_MOD_LINEAR, &data.base_fb);
				igt_plane_set_fb(primary, &data.base_fb);
				valid_outputs++;
				break;
			} else {
				igt_output_set_crtc(cur_output, NULL);
			}
		}
	}
	igt_require(valid_outputs > 0);
	igt_display_commit_atomic(&data.display, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);

	igt_describe("Concurrent parallel atomic flipping combined with multi-mode setting stress.");
	pthread_t thread1, thread2;

	/* Fire off the asynchronous test runners sharing the SAME display object context */
	pthread_create(&thread1, NULL, atomic_stress_thread, &data);
	pthread_create(&thread2, NULL, modeset_external_thread, &data);

	/* Await infinite thread execution bounds */
	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);

	/* Clean up structural assets */
	int cleanup_fd = drm_open_driver(DRIVER_ANY);
	igt_remove_fb(cleanup_fd, &data.base_fb);
	igt_display_fini(&data.display);
	drm_close_driver(cleanup_fd);
	drm_close_driver(data.drm_fd);

	igt_exit();
}

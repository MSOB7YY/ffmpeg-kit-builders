/*
 * This file is part of FFmpegKit.
 *
 * FFmpegKit is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FFmpegKit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with FFmpegKit.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * ffplay_lib.h for builds configured with --disable-sdl2.
 *
 * Every FFplay entry point stays exported, so the ffplay_kit_* wrapper API and
 * the bindings built against it keep linking, but nothing can be played:
 * ffplay_init fails and the other calls do nothing.
 */

#include "ffplay_lib.h"

#include "libavformat/avformat.h"
#include "libavutil/log.h"

static void log_unavailable(void) {
    av_log(NULL, AV_LOG_ERROR, "ffplay is not available, this build has no SDL2.\n");
}

FFplayContext *ffplay_init(const char *args_string, const FFplayCallbacks *cb) {
    (void)args_string;
    (void)cb;
    log_unavailable();
    return NULL;
}

FFplayContext *ffplay_init_argv(int argc, const char *const *argv,
                                const FFplayCallbacks *cb) {
    (void)argc;
    (void)argv;
    (void)cb;
    log_unavailable();
    return NULL;
}

int ffplay_start(FFplayContext *ctx) {
    (void)ctx;
    return -1;
}

int ffplay_step(FFplayContext *ctx) {
    (void)ctx;
    return -1;
}

int ffplay_seek(FFplayContext *ctx, double seconds, double rel) {
    (void)ctx;
    (void)seconds;
    (void)rel;
    return -1;
}

int ffplay_pause(FFplayContext *ctx) {
    (void)ctx;
    return -1;
}

int ffplay_resume(FFplayContext *ctx) {
    (void)ctx;
    return -1;
}

int ffplay_stop(FFplayContext *ctx) {
    (void)ctx;
    return -1;
}

double ffplay_get_position(FFplayContext *ctx) {
    (void)ctx;
    return 0;
}

double ffplay_get_duration(FFplayContext *ctx) {
    (void)ctx;
    return 0;
}

int ffplay_is_playing(FFplayContext *ctx) {
    (void)ctx;
    return 0;
}

int ffplay_is_paused(FFplayContext *ctx) {
    (void)ctx;
    return 0;
}

void ffplay_set_volume(FFplayContext *ctx, float volume) {
    (void)ctx;
    (void)volume;
}

float ffplay_get_volume(FFplayContext *ctx) {
    (void)ctx;
    return 0;
}

void ffplay_set_audio_output_device(const char *device_name) {
    (void)device_name;
}

char *ffplay_list_audio_devices(void) {
    return NULL;
}

void ffplay_get_video_size(FFplayContext *ctx, int *width, int *height) {
    (void)ctx;
    if (width) *width = 0;
    if (height) *height = 0;
}

void ffplay_free(FFplayContext *ctx) {
    (void)ctx;
}

void ffplay_close(FFplayContext *ctx) {
    (void)ctx;
}

int ffplay_has_video_stream(const char *path) {
    if (!path) return -1;
    AVFormatContext *fmt_ctx = NULL;
    int ret = avformat_open_input(&fmt_ctx, path, NULL, NULL);
    if (ret < 0) return -1;
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    int has_video = 0;
    if (ret >= 0) {
        for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
            if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                has_video = 1;
                break;
            }
        }
    }
    avformat_close_input(&fmt_ctx);
    return (ret < 0) ? -1 : has_video;
}

#ifdef __ANDROID__
void ffplay_set_android_window(ANativeWindow *window) {
    (void)window;
}
#endif

void ffplay_set_frame_callback(FFplayFrameCallback callback, void *userdata) {
    (void)callback;
    (void)userdata;
}

size_t ffplay_get_frame_buffer_size(void) {
    return 0;
}

int ffplay_copy_frame(uint8_t *destination, size_t destination_size,
                      int *width, int *height, int *linesize,
                      uint64_t *generation) {
    (void)destination;
    (void)destination_size;
    if (width) *width = 0;
    if (height) *height = 0;
    if (linesize) *linesize = 0;
    if (generation) *generation = 0;
    return 0;
}

void ffplay_lib_on_frame(const uint8_t *pixels, int width, int height,
                         int linesize, const char *pixel_format) {
    (void)pixels;
    (void)width;
    (void)height;
    (void)linesize;
    (void)pixel_format;
}

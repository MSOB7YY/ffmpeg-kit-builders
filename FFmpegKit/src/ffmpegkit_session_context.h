#ifndef FFMPEG_KIT_SESSION_CONTEXT_H
#define FFMPEG_KIT_SESSION_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

void ffmpegkit_bind_session_id(long session_id);
void ffmpegkit_unbind_session_id(void);
void ffmpegkit_register_root_context(const void *root, long session_id);
void ffmpegkit_unregister_root_context(const void *root);
/* Returns 1 when the level was stored for the calling thread's session. */
int ffmpegkit_set_session_log_level(int level);

#ifdef __cplusplus
}
#endif

#endif // FFMPEG_KIT_SESSION_CONTEXT_H

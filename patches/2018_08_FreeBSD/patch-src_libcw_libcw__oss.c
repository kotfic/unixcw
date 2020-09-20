--- src/libcw/libcw_oss.c.orig	2018-08-08 04:17:27 UTC
+++ src/libcw/libcw_oss.c
@@ -243,7 +243,7 @@ int cw_oss_open_device_internal(cw_gen_t
 	/* Get fragment size in bytes, may be different than requested
 	   with ioctl(..., SNDCTL_DSP_SETFRAGMENT), and, in particular,
 	   can be different than 2^N. */
-	if ((rv = ioctl(soundcard, (int) SNDCTL_DSP_GETBLKSIZE, &size)) == -1) {
+	if ((rv = ioctl(soundcard, SNDCTL_DSP_GETBLKSIZE, &size)) == -1) {
 		cw_debug_msg ((&cw_debug_object), CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
 			      "cw_oss: ioctl(SNDCTL_DSP_GETBLKSIZE): \"%s\"", strerror(errno));
 		close(soundcard);
@@ -373,7 +373,7 @@ int cw_oss_open_device_ioctls_internal(i
 
 
 	audio_buf_info buff;
-	if (ioctl(*fd, (int) SNDCTL_DSP_GETOSPACE, &buff) == -1) {
+	if (ioctl(*fd, SNDCTL_DSP_GETOSPACE, &buff) == -1) {
 		cw_debug_msg ((&cw_debug_object), CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
 			      "cw_oss: ioctl(SNDCTL_DSP_GETOSPACE): \"%s\"", strerror(errno));
 		return CW_FAILURE;
@@ -411,7 +411,7 @@ int cw_oss_open_device_ioctls_internal(i
 		      "cw_oss: fragment size is 2^%d = %d", parameter & 0x0000ffff, 2 << ((parameter & 0x0000ffff) - 1));
 
 	/* Query fragment size just to get the driver buffers set. */
-	if (ioctl(*fd, (int) SNDCTL_DSP_GETBLKSIZE, &parameter) == -1) {
+	if (ioctl(*fd, SNDCTL_DSP_GETBLKSIZE, &parameter) == -1) {
 		cw_debug_msg ((&cw_debug_object), CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
 			      "cw_oss: ioctl(SNDCTL_DSP_GETBLKSIZE): \"%s\"", strerror(errno));
 		return CW_FAILURE;
@@ -432,7 +432,7 @@ int cw_oss_open_device_ioctls_internal(i
         }
 #endif
 
-	if (ioctl(*fd, (int) SNDCTL_DSP_GETOSPACE, &buff) == -1) {
+	if (ioctl(*fd, SNDCTL_DSP_GETOSPACE, &buff) == -1) {
 		cw_debug_msg ((&cw_debug_object), CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
 			      "cw_oss: ioctl(SNDCTL_GETOSPACE): \"%s\"", strerror(errno));
 		return CW_FAILURE;
@@ -480,7 +480,7 @@ int cw_oss_get_version_internal(int fd, 
 	assert (fd);
 
 	int parameter = 0;
-	if (ioctl(fd, (int) OSS_GETVERSION, &parameter) == -1) {
+	if (ioctl(fd, OSS_GETVERSION, &parameter) == -1) {
 		cw_debug_msg ((&cw_debug_object), CW_DEBUG_SOUND_SYSTEM, CW_DEBUG_ERROR,
 			      "cw_oss: ioctl OSS_GETVERSION");
 		return CW_FAILURE;

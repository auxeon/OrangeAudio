#ifndef CONFIG_H
#define CONFIG_H

/* Default recording device, sample rate. Sound is always recorded in
 * mono. The device can be overriden at runtime using the environment
 * variable SOUND_DEVICE_ENV. */
#define SOUND_DEVICE "default"
#define SOUND_DEVICE_ENV "RTSPECCY_CAPTURE_DEVICE"
#define SOUND_RATE 44100

/* A higher value means larger latency and more memory consumption. On
 * the other hand, a higher value also results in a higher resolution of
 * the spectrogram.
 *
 * Note: This value must be 2^n for some integer n > 1. */
#define SOUND_SAMPLES_PER_TURN 2048

/* Number of lines in the spectrogram history (upper half of the screen).
 *
 * Note: This value must be 2^m for some integer m > 0. */

/* The fourier transformation produced by libfftw3 is not normalized.
 * Hence, you need to provide a scaling factor. This is a sane default.
 * Increase this value if you see too much noise. */
#define FFTW_SCALE (0.0125 * fftw.outlen)

/* "Home audio" microphones tend to be very quiet. Increase this value
 * if the waveform's amplitude is too low (or adjust your mixer). */
#define WAVEFORM_SCALE 4

/* Initial size of the window. */
#define DISPLAY_INITIAL_WIDTH 700
#define DISPLAY_INITIAL_HEIGHT 700

/* Background color. Applies to the background of the "current"
 * spectrogram as well as any "exterior" areas. */
#define DISPLAY_BACKGROUND_COLOR { 0.8984375, 0.4921875, 0.1328125 }
//#define DISPLAY_BACKGROUND_COLOR { 0, 0, 0 }
//#define DISPLAY_BACKGROUND_COLOR { 0.203125, 0.59375, 0.85546875 }

/* Color of the "current" spectrogram (bottom of the screen). */
#define DISPLAY_SPEC_CURRENT_COLOR { 1, 1, 1 }
//#define DISPLAY_SPEC_CURRENT_COLOR { 0.8984375, 0.4921875, 0.1328125 }
//#define DISPLAY_SPEC_CURRENT_COLOR { 0.203125, 0.59375, 0.85546875 }

/* Color of the waveform (if shown). */
#define DISPLAY_WAVEFORM_COLOR { 0.8984375, 0.4921875, 0.1328125 }

/* Most of the interesting stuff (when you're singing or speaking)
 * happens in the first quarter of the spectrogram. We will zoom into
 * that area by default if the following constant is defined. */
#define INTERACTION_ZOOM_STARTUP_FIRST_QUARTER

#endif /* CONFIG_H */

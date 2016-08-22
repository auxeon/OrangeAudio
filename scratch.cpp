#include<iostream>
#include "config.h"
#include<SOIL/SOIL.h>
#include <stdio.h>
#include <stdlib.h>
#include <GL/glut.h>
#include <alsa/asoundlib.h>
#include <fftw3.h>
#include <math.h>
#include <string.h>

#define PCM_DEVICE "default"
using namespace std;

/* Informations about the window, display options. */
int height = 700;
int width = 700;
int dx = 1;
int dy = 50;
int posx = 0;
int posy = 0;
int i;
int R = 230; // 0.8984375 rgb
int G = 126; // 0.4921875 rgb
int B = 34;  // 0.1328125 rgb
float currentmax[2] = {0,-1};
float previousmax[2] = {0,-1};
int count = 0;
int bpm = 0;
float sum = 0;
float oldsum = 0;
float diff = 0;
float olddiff = 0;

float noteFrequency[12];

    char strings[64];


struct interactionInfo
{
    int width;
    int height;
    int update;
    int forceOverview;
    int showWaveform;
    double offsetX, lastOffsetX;
    double scaleX;
} interaction;

/* Global sound info. */
struct soundInfo
{
    snd_pcm_t *handle;

    char *buffer, *bufferLast;
    snd_pcm_uframes_t bufferSizeFrames;
    snd_pcm_uframes_t bufferFill;
    int bufferReady;

    int reprepare;
} sound;

// Global fftw info. //
struct fftwInfo
{
    double *in;
    fftw_complex *out;
    fftw_plan plan;
    int outlen;
    double binWidth;

    double *currentLine;
    unsigned char *textureData;
    GLuint textureHandle;
    int textureWidth, textureHeight;
} fftw;

void printtext(int x, int y, string String)
{
//(x,y) is from the bottom left of the window
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, width, 0, height, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glPushAttrib(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);
    glRasterPos2i(x,y);
        glColor3f(0,1,1);
    for (int i=0; i<String.size(); i++)
    {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, String[i]);
    }
    glPopAttrib();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}



// Get i'th sample from buffer and convert to short int. //
short int getFrame(char *buffer, int i)
{
    return ((buffer[2 * i] & 0xFF)) + ((buffer[2 * i + 1] & 0xFF)<<8);
}

// Return the environment variable "name" or "def" if it's unset. //
char *getenvDefault(char *name, char *def)
{
    char *val = getenv(name);
    if (val == NULL)
        return def;
    else
        return val;
    }

/* Open and init the default recording device. */
void audioInit(void)
{
    int rc;
    int size;
    snd_pcm_hw_params_t *params;
    unsigned int val;
    int dir = 0;

    /* Open PCM device for recording (capture). */
    //rc = snd_pcm_open(&sound.handle, getenvDefault(SOUND_DEVICE_ENV,SOUND_DEVICE),
    rc = snd_pcm_open(&sound.handle, PCM_DEVICE,
                      SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0)
    {
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        exit(EXIT_FAILURE);
    }

    snd_pcm_hw_params_alloca(&params);

    snd_pcm_hw_params_any(sound.handle, params);


    snd_pcm_hw_params_set_access(sound.handle, params,
                                 SND_PCM_ACCESS_RW_INTERLEAVED);

    snd_pcm_hw_params_set_format(sound.handle, params,
                                 SND_PCM_FORMAT_S16_LE);

    /* One channel (mono). */
    snd_pcm_hw_params_set_channels(sound.handle, params, 1);

    /* 44100 bits/second sampling rate (CD quality). */
    val = SOUND_RATE;
    snd_pcm_hw_params_set_rate_near(sound.handle, params, &val, &dir);

    snd_pcm_uframes_t frames = SOUND_SAMPLES_PER_TURN;
    snd_pcm_hw_params_set_period_size_near(sound.handle, params,
                                           &frames, &dir);

    /* Write the parameters to the driver. */
    rc = snd_pcm_hw_params(sound.handle, params);
    if (rc < 0)
    {
        fprintf(stderr, "unable to set hw parameters: %s\n",
                snd_strerror(rc));
        exit(EXIT_FAILURE);
    }

    sound.bufferSizeFrames = SOUND_SAMPLES_PER_TURN;
    size = sound.bufferSizeFrames * 2;  /* 2 bytes/sample, 1 channel */

    sound.buffer = (char *)malloc(size);
    sound.bufferLast = (char *)malloc(size);
    sound.bufferFill = 0;
    sound.bufferReady = 0;

    /* Try to switch to non-blocking mode for reading. If that fails,
     * print a warning and continue in blocking mode. */
    rc = snd_pcm_nonblock(sound.handle, 1);
    if (rc < 0)
    {
        fprintf(stderr, "Could not switch to non-blocking mode: %s\n",
                snd_strerror(rc));
    }

    /* Prepare in audioRead() for the first time. */
    sound.reprepare = 1;
}

int audioRead(void)
{
    if (sound.reprepare)
    {
        int ret;
        ret = snd_pcm_drop(sound.handle);
        if (ret < 0)
        {
            fprintf(stderr, "Error while dropping samples: %s\n",
                    snd_strerror(ret));
        }

        ret = snd_pcm_prepare(sound.handle);
        if (ret < 0)
        {
            fprintf(stderr, "Error while preparing to record: %s\n",
                    snd_strerror(ret));
        }

        sound.reprepare = 0;
    }

    snd_pcm_sframes_t rc;
    rc = snd_pcm_readi(sound.handle, sound.buffer + (sound.bufferFill * 2),
                       sound.bufferSizeFrames - sound.bufferFill);
    if (rc == -EPIPE)
    {
        /* EPIPE means overrun */
        snd_pcm_recover(sound.handle, rc, 0);
    }
    else if (rc == -EAGAIN)
    {
        /* Not ready yet. Come back again later. */
    }
    else if (rc < 0)
    {
        fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
    }
    else
    {
        sound.bufferFill += rc;
        if (sound.bufferFill == sound.bufferSizeFrames)
        {
            /* Buffer full. display() can add this to the history. */
            sound.bufferFill = 0;
            sound.bufferReady = 1;
        }
    }

    return rc;
}

/* Shutdown audio device. */
void audioDeinit(void)
{
    snd_pcm_drop(sound.handle);
    snd_pcm_close(sound.handle);
    free(sound.buffer);
    free(sound.bufferLast);
}

// Create FFTW-plan, allocate buffers. //
void fftwInit(void)
{
    fftw.outlen = sound.bufferSizeFrames / 2;
    fftw.in = (double *)fftw_malloc(sizeof(double) * sound.bufferSizeFrames);
    fftw.out = (fftw_complex *)fftw_malloc(sizeof(fftw_complex)
                                           * (fftw.outlen + 1));
    fftw.plan = fftw_plan_dft_r2c_1d(sound.bufferSizeFrames, fftw.in, fftw.out,
                                     FFTW_ESTIMATE);

    fftw.currentLine = (double *)malloc(sizeof(double) * fftw.outlen);
    memset(fftw.currentLine, 0, sizeof(double) * fftw.outlen);


    // How many hertz does one "bin" comprise //
    fftw.binWidth = (double)SOUND_RATE / (double)SOUND_SAMPLES_PER_TURN;
}

// Free any FFTW resources. //
void fftwDeinit(void)
{
    fftw_destroy_plan(fftw.plan);
    fftw_free(fftw.in);
    fftw_free(fftw.out);
    free(fftw.currentLine);
    free(fftw.textureData);
    fftw_cleanup();
}

// Read from audio device and display current buffer. //
void updateDisplay(void)
{
    int i;

    float bgcolor[3] = DISPLAY_BACKGROUND_COLOR;
    glClearColor(bgcolor[0], bgcolor[1], bgcolor[2], 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	
    printtext(width/2 - 60 ,height/3,strings);


    if (interaction.update)
    {
        /* Try again *now* if it failed. */
        while (audioRead() < 0);
    }

    if (sound.bufferReady)
    {
        memmove(sound.bufferLast, sound.buffer, sound.bufferSizeFrames * 2);

        for (i = 0; i < (int)sound.bufferSizeFrames; i++)
        {
            short int val = getFrame(sound.buffer, i);
            fftw.in[i] = 2 * (double)val / (256 * 256);
        }
        fftw_execute(fftw.plan);

        int ha = 0, ta = 0;
        for (i = 0; i < fftw.outlen; i++)
        {
            double val = sqrt(fftw.out[i][0] * fftw.out[i][0]
                              + fftw.out[i][1] * fftw.out[i][1]) / FFTW_SCALE;
			glRasterPos2d(350,50);
			glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,'C');	
           
            //clipping //
           // val = val > 1.0 ? 1.0 : val;
            
            //finding current max
            if(val>=currentmax[1] ){
                previousmax[0] = currentmax[0];
                previousmax[1] = currentmax[1];
                currentmax[0] = ha;
                currentmax[1] = val;
            }

            /* Save current line for current spectrum. */
            fftw.currentLine[ha++] = val;
                
                sum += val;
                

        }
            diff = sum - oldsum;

            if((olddiff*diff<0) && fabs(olddiff) > 1 && fabs(diff) >1){
           // cout<<"NEWSUM - OLDSUM : "<<diff<<endl;
            count++;
            }

            olddiff = diff;
            oldsum = sum;
            sum = 0;

        if(previousmax[0]!=currentmax[0] && previousmax[0]!=currentmax[0]+1 && previousmax[0]!=currentmax[0]-1){
            //printf("max [%d] - [%f] - freq [%f] Hz\n",(int)currentmax[0],currentmax[1],fftw.binWidth*((int)(currentmax[0]+1)));
		
		if(currentmax[0] == 98 || currentmax[0] == 49 || currentmax[0] == 24){
			sprintf(strings,"NOTE PLAYED - C");
		}
		if(currentmax[0] == 103 || currentmax[0] == 52 || currentmax[0] == 26){
			sprintf(strings,"NOTE PLAYED - C#/Db");
			
		}
		if(currentmax[0] == 110 || currentmax[0] == 55 || currentmax[0] == 27){
			sprintf(strings,"NOTE PLAYED - D");

		}
		if(currentmax[0] == 116 || currentmax[0] == 58 || currentmax[0] == 29){
			sprintf(strings,"NOTE PLAYED - D#/Eb");

		}
		if(currentmax[0] == 123 || currentmax[0] == 61 || currentmax[0] == 31){
			sprintf(strings,"NOTE PLAYED - E");

		}
		if(currentmax[0] == 131 || currentmax[0] == 65 || currentmax[0] == 32){
			sprintf(strings,"NOTE PLAYED - F");

		}
		if(currentmax[0] == 139 || currentmax[0] == 69 || currentmax[0] == 34){
			sprintf(strings,"NOTE PLAYED - F#/Gb");

		}
		if(currentmax[0] == 147 || currentmax[0] == 73 || currentmax[0] == 36){
			sprintf(strings,"NOTE PLAYED - G");

		}
		if(currentmax[0] == 156 || currentmax[0] == 77 || currentmax[0] == 39){
			sprintf(strings,"NOTE PLAYED - G#/Ab");

		}
		if(currentmax[0] == 165 || currentmax[0] == 82 || currentmax[0] == 41){
			sprintf(strings,"NOTE PLAYED - A");

		}
		if(currentmax[0] == 175 || currentmax[0] == 87 || currentmax[0] == 43){
			sprintf(strings,"NOTE PLAYED - A#/Bb");

		}
		if(currentmax[0] == 186 || currentmax[0] == 92 || currentmax[0] == 46){
			sprintf(strings,"NOTE PLAYED - B");

		}
        }

        previousmax[0] = currentmax[0] = 0;
        previousmax[1] = currentmax[1] = -1;

    }

    if (sound.bufferReady)
    {

        /* Reset buffer state. The buffer is no longer ready and we
         * can't update the texture from it until audioRead() re-marked
         * it as ready. */
        sound.bufferReady = 0;
    }

    /* Apply zoom and panning. */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (!interaction.forceOverview)
    {
        glScaled(interaction.scaleX, -1, 1);
        glTranslated(interaction.offsetX, 0, 0);
    }


    /* Show current spectrum. */
    if (!interaction.showWaveform)
    {
        float curcol[3] = DISPLAY_SPEC_CURRENT_COLOR;
        glColor3fv(curcol);
        glBegin(GL_LINES);
        for (i = 0; i < fftw.outlen; i++)
        {
            /* relX will be in [-1, 1], relY will be in [0, 1]. */
            double relX = 2 * ((double)i / fftw.outlen) - 1;
            double relY = fftw.currentLine[i];

            /* Move relY so it'll be shown at the bottom of the screen. */
            relY *= 0.5;
         //   relY -= 1;
            glVertex2f(relX, relY);
            glVertex2f(relX, -relY);
        
        
        }
        glEnd();
    }
    else
    {
        glPushMatrix();
        glLoadIdentity();
        float curcol[3] = DISPLAY_WAVEFORM_COLOR;
        glColor3fv(curcol);
        glBegin(GL_LINES);
        for (i = 0; i < (int)sound.bufferSizeFrames; i++)
        {
            /* relX will be in [-1, 1], relY will be in [-s, s] where s
             * is WAVEFORM_SCALE. */
            short int val = getFrame(sound.bufferLast, i);
            double relX = 2 * ((double)i / sound.bufferSizeFrames) - 1;
            double relY = 2 * WAVEFORM_SCALE * (double)val / (256 * 256);

            /* Clamp relY ... WAVEFORM_SCALE may be too high. */
            relY = relY > 1 ? 1 : relY;
            relY = relY < -1 ? -1 : relY;

            /* Move relY so it'll be shown at the bottom of the screen. */
            relY *= 0.25;
            relY -= 0.75;
            glVertex2f(relX, relY);
        }
        glEnd();
        glPopMatrix();
    }

    float lineYStart = -1;
    if (interaction.showWaveform)
        lineYStart = -0.5;

    glutSwapBuffers();
}

/* Simple orthographic projection. */
void reshape(int w, int h)
{
    interaction.width = w;
    interaction.height = h;

    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -10, 10);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glutPostRedisplay();
}

//glut timer function
void timer_func(int n)           // NEW FUNCTION
{
    //bpm = count*6
    bpm = count*6;
//    cout<<"TEMPO - "<<bpm<<"BPM\n";
    bpm = 0;
    count = 0;
         
    glutTimerFunc(n, timer_func, n); // recursively call timer_func
}




/* Create the window, set up callbacks and interaction parameters. */
void displayInit(int argc, char *argv[])
{
    interaction.width = DISPLAY_INITIAL_WIDTH;
    interaction.height = DISPLAY_INITIAL_HEIGHT;
    interaction.update = 1;
    interaction.forceOverview = 0;
    interaction.showWaveform = 0;
    interaction.scaleX = 1;
    interaction.offsetX = 0;
    interaction.lastOffsetX = 0;

#ifdef INTERACTION_ZOOM_STARTUP_FIRST_QUARTER
    interaction.scaleX = 4;
    interaction.offsetX = 0.75;
    interaction.lastOffsetX = interaction.offsetX;
#endif

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(interaction.width, interaction.height);
    glutCreateWindow("OrangeAudio");    
    glutDisplayFunc(updateDisplay);
    glutReshapeFunc(reshape);
    glutIdleFunc(updateDisplay);
    timer_func(10000);
}




int main(int argc, char *argv[])
{
    displayInit(argc, argv);
    audioInit();
    fftwInit();
    atexit(audioDeinit);
    atexit(fftwDeinit);

    glutMainLoop();
    return 0;  /* Not reached. */
}


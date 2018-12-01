//SDL audio - see below
//or SFML https://www.sfml-dev.org/documentation/2.5.1/
//or Tiny Alsa: https://github.com/tinyalsa/tinyalsa/blob/master/utils/tinyplay.c
//alsa http://www.alsa-project.org/alsa-doc/alsa-lib/
//Arduino audio: https://www.arduino.cc/en/Tutorial/SimpleAudioPlayer
//   https://www.arduino.cc/en/Reference/AudioWrite


//sdlffmovie.h
//sample code from https://www.gamedev.net/forums/topic/602042-manipulating-sounds-c/

//SDL_PauseAudio(0) causes SDL to start playing the audio, and hence calling the callback asking for audio data.


#ifndef INC_SDLFFMOVIE_H
#define INC_SDLFFMOVIE_H

#include "Definitions.h"
#include <SDL.h>
#include <SDL_ffmpeg.h>

#define BUFFER_SIZE 20 // in audio frames

class SDL_FFMovie
{
public:
    SDL_FFMovie()
    {
        audioFrame = NULL;
        videoFrame = NULL;
        movie = NULL;
        mutex = NULL;
        movie_w = 0;
        movie_h = 0;
        timestamp = 0;
        movStart = 0;
    }
    ~SDL_FFMovie() {}
// run a movie
    bool movieDone;
    int Run(SDL_Surface *screen, int x, int y, const char *filename);
private:
// get the current time of the playing movie
    int64_t GetTimestamp()
    {
// return the position that the current movie should be at
        if(SDL_ffmpegValidAudio(movie))
            return timestamp;
        else if(SDL_ffmpegValidVideo(movie))
            return SDL_GetTicks() - movStart;
        return 0;
    }
// create a buffer for audio frames
    SDL_ffmpegAudioFrame **audioFrame;
// the frame used for drawing the current movie
    SDL_ffmpegVideoFrame *videoFrame;
// the movie file
    SDL_ffmpegFile *movie;
/* use a mutex to prevent errors due to multithreading */
    SDL_mutex *mutex;
// the width and height of the current movie
    int movie_w;
    int movie_h;
// start time and position of the current movie
    int64_t timestamp;
    int64_t movStart;
// callback for SDL audio playback
    static void AudioCallback(void *data, Uint8 *stream, int length)
    {
        ((SDL_FFMovie*)data)->ThisAudioCallback(NULL, stream, length);
    }
    void ThisAudioCallback(void *data, Uint8 *stream, int length);

    void Cleanup();
};

#endif /* INC_SDLFFMOVIE_H */


int SDL_FFMovie::Run(SDL_Surface *screen, int x, int y, const char *filename)
{
    movie = SDL_ffmpegOpen(filename);
    if(movie == NULL)
    {
        fprintf(stderr, "could not open %s: %s\n", filename, SDL_ffmpegGetError());
        return -1;
    }
    mutex = SDL_CreateMutex();
// select the first audio and video streams from the mpeg
    SDL_ffmpegSelectAudioStream(movie, 0);
    SDL_ffmpegSelectVideoStream(movie, 0);
// retrieve the audio format of the movie
    SDL_AudioSpec specs = SDL_ffmpegGetAudioSpec(movie, 512, AudioCallback);
    specs.userdata = this; // pass "this" to the audio callback
// retrieve the video sizeo of the movie
    SDL_ffmpegGetVideoSize(movie, &movie_w, &movie_h);
// create a video frame to blit to the screen
    videoFrame = SDL_ffmpegCreateVideoFrame();
    videoFrame->surface = SDL_CreateRGBSurface(0, movie_w, movie_h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0);
// create the source rectangle
    SDL_Rect src_rect = {0, 0 ,movie_w, movie_h};
    SDL_Rect dst_rect = {x, y ,movie_w, movie_h};
    if(SDL_ffmpegValidAudio(movie))
    {
// open the audio device
        if(SDL_OpenAudio(&specs, 0) != 0)
        {
            fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
            Cleanup();
            return -1;
        }
// calculate the audio frame size (2 bytes per sample)
        int frameSize = specs.channels * specs.samples * 2;
// allocate the audio buffer
        audioFrame = new SDL_ffmpegAudioFrame*[BUFFER_SIZE];
//create and fill the audio buffer
        for(int i = 0; i < BUFFER_SIZE; i++)
        {
            audioFrame = SDL_ffmpegCreateAudioFrame(movie, frameSize);
            if(audioFrame == NULL) { Cleanup(); return -1; }
            SDL_ffmpegGetAudioFrame(movie, audioFrame);
        }
// unpause audio so the buffer starts being read.
        SDL_PauseAudio(0);
// store the time at which the movie started playing
        movStart = SDL_GetTicks();
    }
    SDL_Event Event;
    movieDone = false;
    while(movieDone == false)
    {
/*Uint8 *keys = SDL_GetKeyState(NULL);
if(keys[SDLK_ESCAPE])
movieDone = true;*/
// handle keyboard and mouse input
        while (SDL_PollEvent(&Event))
        {
            switch(Event.type)
            {
            case SDL_MOUSEMOTION:
                movieDone = true;
                break;
            case SDL_KEYDOWN:
                movieDone = true;
                switch(Event.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                   movieDone = true;
                }
                break;
            case SDL_QUIT:
                movieDone = true;
                break;
            }
        }
//The two important parts are this one, from the main loop where I use the ffmpeg decoder to decompress audio frames and fill up the buffer:
// fill up the audio buffer if neccessary
        if(SDL_ffmpegValidAudio(movie))
        {
            SDL_LockMutex(mutex);
            for(int i = 0; i < BUFFER_SIZE; i++)
// check if frame is empty
                if(audioFrame->size == 0) SDL_ffmpegGetAudioFrame(movie, audioFrame); // fill frame with new data
            SDL_UnlockMutex(mutex);
        }
// draw the video frame
        if(videoFrame)
        {
// if the current frame has expired, get a new one
            if(videoFrame->pts < GetTimestamp())
                SDL_ffmpegGetVideoFrame(movie, videoFrame);
// draw the current frame to the screen
            if(videoFrame->surface != NULL)
            {
                SDL_FillRect(screen, 0, 0);
                SDL_BlitSurface(videoFrame->surface, &src_rect, screen, &dst_rect);
                SDL_Flip(screen);
            }
// exit if this is the last frame
            if(videoFrame->last) movieDone = true;
        }
    }
    Cleanup();
    return 0;
}


void SDL_FFMovie::ThisAudioCallback(void *data, Uint8 *stream, int length)
{
//and this one, inside the audio callback where I feed them into the audio stream whenever SDL calls the callback because it needs more audio data to continue playing.
// lock mutex, so audioFrame[] will not be changed from another thread
    SDL_LockMutex( mutex );
    if(audioFrame[0]->size == length)
    {
// update timestamp
        timestamp = audioFrame[0]->pts;
// copy one frame from the buffer to the stream
        memcpy(stream, audioFrame[0]->buffer, audioFrame[0]->size);
// mark the frame as used
        audioFrame[0]->size = 0;
// move the empty frame to the end of the buffer
        SDL_ffmpegAudioFrame *f = audioFrame[0];
        for(int i = 1; i < BUFFER_SIZE; i++ ) audioFrame[i - 1] = audioFrame;
        audioFrame[BUFFER_SIZE - 1] = f;
    }
    else memset(stream, 0, length); // no frames available
    SDL_UnlockMutex( mutex );
}


void SDL_FFMovie::Cleanup()
{
// free the movie file
    if(movie != NULL) { SDL_ffmpegFree(movie); movie = NULL; }
// stop any audio playback
    if(SDL_ffmpegValidAudio(movie)) SDL_PauseAudio(1);
    SDL_CloseAudio();
// free all audio frames and delete the buffer
    if(audioFrame != NULL)
    {
        for(int i = 0; i < BUFFER_SIZE; i++)
            if(audioFrame != NULL) SDL_ffmpegFreeAudioFrame(audioFrame);
        delete [] audioFrame;
        audioFrame = NULL;
    }
    if(videoFrame != NULL) { SDL_ffmpegFreeVideoFrame(videoFrame); videoFrame = NULL; }
    if(mutex != NULL) { SDL_DestroyMutex(mutex); mutex = NULL; }
    movie_w = 0;
    movie_h = 0;
    timestamp = 0;
    movStart = 0;
}


//eof
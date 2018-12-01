//SDL audio stream: https://wiki.libsdl.org/SDL_AudioStream
//tutorial: https://wiki.libsdl.org/Tutorials/AudioStream


// You put data at Sint16/mono/22050Hz, you get back data at Float32/stereo/48000Hz
SDL_AudioStream *stream = SDL_NewAudioStream(AUDIO_S16, 1, 22050, AUDIO_F32, 2, 48000);
if (stream == NULL) {
    printf("Uhoh, stream failed to create: %s\n", SDL_GetError());
} else {
    // We are ready to use the stream!
}


Sint16 samples[1024];
int num_samples = read_more_samples_from_disk(samples); // whatever.
// you tell it the number of _bytes_, not samples, you're putting!
int rc = SDL_AudioStreamPut(stream, samples, num_samples * sizeof (Sint16));
if (rc == -1) {
    printf("Uhoh, failed to put samples in stream: %s\n", SDL_GetError());
    return;
}

// Whoops, forgot to add a single sample at the end...!
//  You can put any amount at once, SDL will buffer
//  appropriately, growing the buffer if necessary.
Sint16 onesample = 22;
SDL_AudioStreamPut(stream, &onesample, sizeof (Sint16));


int avail = SDL_AudioStreamAvailable(stream);  // this is in bytes, not samples!
if (avail < 100) {
    printf("I'm still waiting on %d bytes of data!\n", 100 - avail);
}


float converted[100];
// this is in bytes, not samples!
int gotten = SDL_AudioStreamGet(stream, converted, sizeof (converted));
if (gotten == -1) {
    printf("Uhoh, failed to get converted data: %s\n", SDL_GetError());
}
write_more_samples_to_disk(converted, gotten); /* whatever. */


int gotten;
do {
    float converted[100];
    // this is in bytes, not samples!
    gotten = SDL_AudioStreamGet(stream, converted, sizeof (converted));
    if (gotten == -1) {
        printf("Uhoh, failed to get converted data: %s\n", SDL_GetError());
    } else {
        // (gotten) might be less than requested in SDL_AudioStreamGet!
        write_more_samples_to_disk(converted, gotten); /* whatever. */
    }
} while (gotten > 0);


SDL_AudioStreamFlush(stream);

    SDL_AudioStreamClear(stream);

 SDL_FreeAudioStream(stream);
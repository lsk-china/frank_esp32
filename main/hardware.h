//
// Created by lsk on 6/27/25.
//

#ifndef HARDWARE_H
#define HARDWARE_H

#include <stddef.h>

/*
 * ---------- AUDIO ----------
 */

/**
 * Defines metadata of a piece of audio data.
 */
struct audio_metadata
{
    /**
     * Length of the data in memory.
     */
    size_t length;

    /**
     * Channels
     */
    int channels;

    /**
     * Sample rate
     */
    int sample_rate;

    /**
     * Bits per sample (BPP)
     */
    int bits_per_sample;
};

#endif //HARDWARE_H

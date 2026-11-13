/*
 * Minimal stand-in for upstream kissfft's kiss_fft_log.h (vendored subset;
 * errors surface via the return value, not logging).
 */
#pragma once

#include <stdio.h>

#define KISS_FFT_ERROR(x) do { fprintf(stderr, "kissfft error: %s\n", x); } while (0)
#define KISS_FFT_WARNING(x) do { fprintf(stderr, "kissfft warning: " x "\n"); } while (0)

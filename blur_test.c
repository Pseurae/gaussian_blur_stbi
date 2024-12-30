#include <stdio.h>

#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define RADIUS 50
static double kernel[RADIUS];

typedef struct { unsigned char r, g, b, a; } pixel_t;
typedef struct { double r, g, b, a; } pixel_normal_t;

static inline void pixel_normal_div_scalar(pixel_normal_t *px, double i)
{
    px->r /= i;
    px->g /= i;
    px->b /= i;
    px->a /= i;
}

static inline void pixel_normal_add_pixel_with_mult(pixel_normal_t *px1, pixel_t px2, double mult)
{
    px1->r += px2.r * mult;
    px1->g += px2.g * mult;
    px1->b += px2.b * mult;
    px1->a += px2.a * mult;
}

static inline pixel_t pixel_normal_to_pixel(pixel_normal_t accum)
{
    pixel_t px;
    px.r = accum.r;
    px.g = accum.g;
    px.b = accum.b;
    px.a = accum.a;

    return px;
}

static inline pixel_normal_t pixel_to_pixel_normal(pixel_t px)
{
    pixel_normal_t accum;
    accum.r = px.r;
    accum.g = px.g;
    accum.b = px.b;
    accum.a = px.a;

    return accum;
}

static inline int clamp(int a, int min, int max)
{
    if (max < min) return a;
    if (a < min) return min;
    if (a > max) return max;

    return a;
}

static void construct_kernel(void)
{
    for (int i = 1; i <= RADIUS; ++i) 
        kernel[i - 1] = exp(-(i * i) / (2.0 * RADIUS * RADIUS * 0.125));
}

int main(int argc, char *argv[])
{
    // Pre-calculate the gaussian kernel.
    construct_kernel();

    int width, height, num_channels;
    pixel_t *buffer1 = (pixel_t *)stbi_load("image.png", &width, &height, &num_channels, STBI_rgb_alpha);
    pixel_t *buffer2 = (pixel_t *)malloc(sizeof(pixel_t) * width * height);

#define get_pixel(BUF, X, Y) (&BUF[(X) + (Y) * width])
#define safe_get_pixel(BUF, X, Y) get_pixel(BUF, clamp(X, 0, width - 1), clamp(Y, 0, height - 1))

    // 1st Pass: Read from buffer1 -> Write to buffer2
    // 2nd Pass: Read from buffer2 -> Write to buffer1

    // Horizontal Pass
    for (int x = 0; x < width; ++x)
    for (int y = 0; y < height; ++y)
    {
        // A "pixel normal" is just 4 doubles :)
        pixel_normal_t accum = pixel_to_pixel_normal(*get_pixel(buffer1, x, y));
        double sum = 1.0;

        for (int bx = 1; bx <= RADIUS; ++bx)
        {
            double weight = kernel[bx - 1];
            pixel_normal_add_pixel_with_mult(&accum, *safe_get_pixel(buffer1, x - bx, y), weight);
            pixel_normal_add_pixel_with_mult(&accum, *safe_get_pixel(buffer1, x + bx, y), weight);
            sum += weight * 2.0;
        }

        // Get the average of all the added colors
        pixel_normal_div_scalar(&accum, sum);
        *get_pixel(buffer2, x, y) = pixel_normal_to_pixel(accum);
    }

    // Vertical Pass
    for (int x = 0; x < width; ++x)
    for (int y = 0; y < height; ++y)
    {
        pixel_normal_t accum = pixel_to_pixel_normal(*get_pixel(buffer2, x, y));
        double sum = 1.0;

        for (int by = 1; by <= RADIUS; ++by)
        {
            double weight = kernel[by - 1];
            pixel_normal_add_pixel_with_mult(&accum, *safe_get_pixel(buffer2, x, y - by), weight);
            pixel_normal_add_pixel_with_mult(&accum, *safe_get_pixel(buffer2, x, y + by), weight);
            sum += weight * 2.0f;
        }

        pixel_normal_div_scalar(&accum, sum);
        *get_pixel(buffer1, x, y) = pixel_normal_to_pixel(accum);
    }

    // Don't know what the last parameter is supposed to be
    stbi_write_png("post.png", width, height, num_channels, buffer1, width * num_channels);

    // Free the buffers.
    // buffer1 is loaded by stb_image, buffer2 is malloc-ed
    stbi_image_free(buffer1);
    free(buffer2);
    return 0;
}
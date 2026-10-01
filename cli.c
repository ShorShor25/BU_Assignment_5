#include "kernel.h"
#include <string.h>
#include <sys/mman.h>

int generate_pagefault() {
struct image img;
    img.width = 1024;
    img.height = 1024;
    img.pixels = malloc(sizeof(struct pixel) * img.width * img.height);
    if (!img.pixels) return -1;

    memset(img.pixels, 0, sizeof(struct pixel) * img.width * img.height);

    // 2. Use saveimage_mmap to write to a temp file on disk
    char temp_name[] = "/tmp/fault_image.bin";
    saveimage_mmap(temp_name, &img);

    // 3. Free the dummy memory and clean up the file
    free(img.pixels);
    unlink(temp_name);

    return 0;
}

int main(int argc, char** argv) {
    // You can expect argv[1] to be the mode
    // You can expect argv[2] to be the filepath
    // You can expect argv[3] to be the integer width
    // You can expect argv[4] to be the integer height
    // You can expect argv[5] to be the output filepath.

    if (argc != 6) {
        printf("Incorrect number of arguments. Expected: ./cli     \n");
        return -1;
    }

    char* mode = argv[1];
    char* input_path = argv[2];
    int width = atoi(argv[3]);
    int height = atoi(argv[4]);
    char* output_path = argv[5];

    if (strcmp(mode, "fault") == 0) {
        generate_pagefault();
        return 0;
    }

    // Allocate the space needed for one image metadata struct
    struct image img;
    img.width = width;
    img.height = height;
    img.pixels = NULL;

    int kernel[3][3] = {{1, 1, 1}, {1, 1, 1}, {1, 1, 1}};
    float normalize = 1.0f / 9.0f;

    if (strcmp(mode, "kernel") == 0) {
        if (loadimage(input_path, &img) != 0) {
            fprintf(stderr, "Failed to load image %s\n", input_path);
            return -1;
        }

        struct image* out = apply_kernel(&img, (int*)kernel, 3, normalize);
        if (out) {
            saveimage(output_path, out);
            free(out->pixels);
            free(out);
        }
        free(img.pixels);
    }
    else if (strcmp(mode, "fault") == 0) {
        generate_pagefault();
        return 0;
    } 
    else if (strcmp(mode, "mmap") == 0) {
        if (loadimage_mmap(input_path, &img) != 0) {
            fprintf(stderr, "Failed to mmap image %s\n", input_path);
            return -1;
        }

        struct image* out = apply_kernel(&img, (int*)kernel, 3, normalize);
        if (out) {
            saveimage(output_path, out);
            free(out->pixels);
            free(out);
        }

        // Release the read-only mmapped region
        void* map_start = (char*)img.pixels - sizeof(struct image);
        size_t map_size = sizeof(struct image) + (size_t)width * height * sizeof(struct pixel);
        munmap(map_start, map_size);
    } 
    else if (strcmp(mode, "convert") == 0) {
        // Loads BMP and saves to raw binary via mmap
        if (loadimage(input_path, &img) != 0) {
            fprintf(stderr, "Failed to load image %s\n", input_path);
            return -1;
        }
        saveimage_mmap(output_path, &img);
        free(img.pixels);
    } 
    else if (strcmp(mode, "uconvert") == 0) {
        // Loads raw binary via mmap and saves to BMP
        if (loadimage_mmap(input_path, &img) != 0) {
            fprintf(stderr, "Failed to load mmap image %s\n", input_path);
            return -1;
        }
        saveimage(output_path, &img);

        // Release the read-only mmapped region
        void* map_start = (char*)img.pixels - sizeof(struct image);
        size_t map_size = sizeof(struct image) + (size_t)width * height * sizeof(struct pixel);
        munmap(map_start, map_size);
    } 
    else {
        fprintf(stderr, "Unknown mode: %s\n", mode);
        return -1;
    }

    return 0;
}
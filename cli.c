#include "kernel.h"
#include <string.h>

int generate_pagefault() {
    char temp_file[] = "/tmp/pf_test_XXXXXX";
    int fd = mkstemp(temp_file);
    if (fd == -1) return -1;

    size_t size = 16 * 1024 * 1024; // 16 MB
    if (ftruncate(fd, size) == -1) {
        close(fd);
        unlink(temp_file);
        return -1;
    }

    // Write data to ensure blocks exist on disk
    char buf[4096];
    memset(buf, 'X', sizeof(buf));
    for (size_t i = 0; i < size; i += sizeof(buf)) {
        if (write(fd, buf, sizeof(buf)) == -1) {
            close(fd);
            unlink(temp_file);
            return -1;
        }
    }

    // Evict file from page cache
    posix_fadvise(fd, 0, size, POSIX_FADV_DONTNEED);

    void* addr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        unlink(temp_file);
        return -1;
    }

    // Purge memory pages
    madvise(addr, size, MADV_DONTNEED);

    // Reading causes the OS to fetch the page from storage -> Major page fault!
    volatile char* p = (volatile char*)addr;
    volatile char val = 0;
    for (size_t offset = 0; offset < size; offset += 4096 * 64) {
        val += p[offset];
    }
    (void)val;

    munmap(addr, size);
    close(fd);
    unlink(temp_file);
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
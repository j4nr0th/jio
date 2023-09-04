//
// Created by jan on 30.6.2023.
//

#include "../include/jio/iobase.h"
#include "internal.h"


#ifndef _WIN32
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

static void*
file_to_memory(const jio_context* ctx, const char* filename, size_t* p_out_size, int write, int must_create)
{
    static long PG_SIZE = 0;
    void* ptr = NULL;
    if (!PG_SIZE)
    {
        PG_SIZE = sysconf(_SC_PAGESIZE);
        if (PG_SIZE < -1)
        {
            JIO_ERROR(ctx, "sysconf did not find page size, reason: %s", strerror(errno));
            PG_SIZE = 0;
            goto end;
        }
    }

    int o_flags, p_flags;
    if (write)
    {
        o_flags = O_RDWR;
        p_flags = PROT_READ | PROT_WRITE;
    }
    else
    {
        o_flags = O_RDONLY;
        p_flags = PROT_READ;
    }
    if (must_create)
    {
        if (!write)
        {
            JIO_ERROR(ctx, "Create flag was specified for memory file, but write access was not demanded");
            goto end;
        }
        o_flags |= O_CREAT;
    }


    int fd;
    if (!must_create)
    {
        fd = open(filename, o_flags);
    }
    else
    {
        fd = open(filename, o_flags, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    }
    if (fd < 0)
    {
        JIO_ERROR(ctx, "Could not open a file descriptor for file \"%s\" (perms: %s), reason: %s", filename,
                  write ? ("O_RDWR") : ("O_RDONLY"), strerror(errno));
        goto end;
    }
    size_t size;
    if (*p_out_size == 0)
    {

        struct stat fd_stats;
        if (fstat(fd, &fd_stats) < 0)
        {
            JIO_ERROR(ctx, "Could not retrieve stats for fd of file \"%s\", reason: %s", filename, strerror(errno));
            close(fd);
            goto end;
        }
        size = fd_stats.st_size + 1;  //  That extra one for the sick null terminator
    }
    else
    {
        size = *p_out_size;
        if (ftruncate(fd, (off_t)size) != 0)
        {
            JIO_ERROR(ctx, "Failed truncating FD to %zu bytes, reason: %s", (size_t)size, strerror(errno));
            close(fd);
            goto end;
        }

    }

    //  Round size up to the closest larger multiple of page size
    size_t extra = size % PG_SIZE;
    if (extra)
    {
        size += (PG_SIZE - extra);
    }
    assert(size % PG_SIZE == 0);
    ptr = mmap(NULL, size, p_flags, write ? MAP_SHARED : MAP_PRIVATE, fd, 0);
    close(fd);
    if (ptr == MAP_FAILED)
    {
        JIO_ERROR(ctx, "Failed mapping file \"%s\" to memory (prot: %s), reason: %s", filename,
                  write ? ("PROT_READ|PROT_WRITE") : ("PROT_READ"), strerror(errno));
        ptr = NULL;
        goto end;
    }
    *p_out_size = size;

end:

    return ptr;
}

static void file_from_memory(const jio_context* ctx, void* ptr, size_t size)
{
    int r = munmap(ptr, size);
    if (r < 0)
    {
        JIO_ERROR(ctx, "Failed unmapping pointer %p from memory, reason: %s", ptr, strerror(errno));
    }
}



jio_result jio_memory_file_create(
        const jio_context* ctx,
        const char* filename, jio_memory_file** p_file_out, int write, int can_create, size_t size)
{
    jio_result res = JIO_RESULT_SUCCESS;
    jio_memory_file* const this = jio_alloc(ctx, sizeof(*this));
    if (!this)
    {
        return JIO_RESULT_BAD_ALLOC;
    }

    int should_create = 0;
    if (!realpath(filename, this->name))
    {
        if (!can_create)
        {
            JIO_ERROR(ctx, "Could not find full path of file \"%s\", reason: %s", filename, strerror(errno));
            res = JIO_RESULT_BAD_PATH;
            jio_free(ctx, this);
            goto end;
        }
        should_create = 1;
    }

    size_t real_size = size;
    void* ptr = file_to_memory(NULL, filename, &real_size, write, should_create);
    if (!ptr)
    {
        JIO_ERROR(ctx, "Failed mapping file to memory");
        res = JIO_RESULT_BAD_MAP;
        jio_free(ctx, this);
        goto end;
    }
    this->can_write = (write != 0);
    this->ptr = ptr;
    this->file_size = real_size;
    this->ctx = ctx;
    *p_file_out = this;
end:
    return res;
}


#else

void* file_to_memory(const char* filename, size_t* p_out_size)
{
    RMOD_ENTER_FUNCTION;
    void* ptr = NULL;
    FILE* fd = fopen(filename, "r");
    if (!fd)
    {
        RMOD_ERROR_CRIT("Could not open file \"%s\", reason: %s", filename, RMOD_ERRNO_MESSAGE);
        goto end;
    }
    fseek(fd, 0, SEEK_END);
    size_t size = ftell(fd) + 1;
    ptr = jalloc(size);
    fseek(fd, 0, SEEK_SET);
    if (ptr)
    {
        size_t v = fread(ptr, 1, size, fd);
        if (ferror(fd))
        {
            RMOD_ERROR("Failed calling fread on file \"%s\", reason: %s", filename, RMOD_ERRNO_MESSAGE);
            fclose(fd);
            goto end;
        }
        *p_out_size = v;
    }
    fclose(fd);

end:
    RMOD_LEAVE_FUNCTION;
    return ptr;
}

void unmap_file(void* ptr, size_t size)
{
    RMOD_ENTER_FUNCTION;
    (void)size;
    jfree(ptr);
    RMOD_LEAVE_FUNCTION;
}
static void file_from_memory(void* ptr, size_t size)
{
    RMOD_ENTER_FUNCTION;
    jfree(ptr);
    (void)size;
    RMOD_LEAVE_FUNCTION;
}


bool map_file_is_named(const jio_memory_file* f1, const char* filename)
{
    RMOD_ENTER_FUNCTION;
    const bool res = strcmp(f1->name, filename) != 0;
    RMOD_LEAVE_FUNCTION;
    return res;
}



rmod_result rmod_map_file_to_memory(const char* filename, jio_memory_file* p_file_out)
{
    RMOD_ENTER_FUNCTION;
    rmod_result res = RMOD_RESULT_SUCCESS;
    if (!GetFullPathNameA(filename, sizeof(p_file_out->name), p_file_out->name, NULL))
    {
        RMOD_ERROR("Could not find full path of file \"%s\", reason: %s", filename, RMOD_ERRNO_MESSAGE);
        res = RMOD_RESULT_BAD_PATH;
        goto end;
    }

    size_t size;
    void* ptr = file_to_memory(filename, &size);
    if (!ptr)
    {
        RMOD_ERROR("Failed mapping file to memory");
        res = RMOD_RESULT_BAD_FILE_MAP;
        goto end;
    }

    p_file_out->ptr = ptr;
    p_file_out->file_size = size;
    end:
    RMOD_LEAVE_FUNCTION;
    return res;
}


#endif


void jio_memory_file_destroy(jio_memory_file* mem_file)
{
    file_from_memory(mem_file->ctx, mem_file->ptr, mem_file->file_size);
    jio_free(mem_file->ctx, mem_file);
}

jio_result jio_memory_file_sync(const jio_memory_file* file, int sync)
{
    const jio_context* ctx = file->ctx;
    jio_result res = JIO_RESULT_SUCCESS;

    if (msync(file->ptr, file->file_size, sync ? MS_SYNC : MS_ASYNC) != 0)
    {
        JIO_ERROR(ctx, "Could not sync memory mapping of file \"%s\" (write allowed: %u), reason: %s", file->name, file->can_write,
                  strerror(errno));
        res = JIO_RESULT_BAD_ACCESS;
    }

    return res;
}

unsigned jio_memory_file_count_lines(const jio_memory_file* file)
{
    unsigned count = 1;
    for (const char* ptr = file->ptr; ptr; ptr = memchr(ptr, '\n', file->file_size - (ptr - (const char*)file->ptr)))
    {
        count += 1;
        ptr += 1;
    }

    return count;
}

unsigned jio_memory_file_count_non_empty_lines(const jio_memory_file* file)
{
    unsigned count = 1;
    const char* ptr = file->ptr;
    const char* const end = (const char*)file->ptr + file->file_size;
    for (;;)
    {
        const char* next = memchr(ptr, '\n', end - ptr);
        const char* end_of_line;
        if (!next)
        {
            end_of_line = end;
        }
        else
        {
            end_of_line = next;
        }

        while (ptr < end_of_line)
        {
            if (*ptr && !jio_iswhitespace(*ptr))
            {
                count += 1;
                break;
            }
            ++ptr;
        }

        ptr = next + 1;

        if (!next)
        {
            break;
        }
    }

    return count;
}

jio_result jio_context_create(const jio_context_create_info* create_info, jio_context** p_context)
{
    jio_context_create_info info = *create_info;
    create_info = NULL;
    if (!info.error_callbacks)
    {
        info.error_callbacks = &DEFAULT_ERROR_CALLBACKS;
    }
    if (!info.allocator_callbacks)
    {
        info.allocator_callbacks = &DEFAULT_ALLOCATOR_CALLBACKS;
    }
    if (!info.stack_allocator_callbacks)
    {
        info.stack_allocator_callbacks = info.allocator_callbacks;
    }

    jio_context* const this = info.allocator_callbacks->alloc(info.allocator_callbacks->param, sizeof(*this));
    if (!this)
    {
        return JIO_RESULT_BAD_ALLOC;
    }

    this->allocator_callbacks = *info.allocator_callbacks;
    this->stack_allocator_callbacks = *info.stack_allocator_callbacks;
    this->error_callbacks = *info.error_callbacks;
    *p_context = this;

    return JIO_RESULT_SUCCESS;
}

void jio_context_destroy(jio_context* ctx)
{
    jio_free(ctx, ctx);
}

jio_memory_file_info jio_memory_file_get_info(const jio_memory_file* file)
{
    const jio_memory_file_info result =
            {
            .size = file->file_size,
            .memory = file->ptr,
            .can_write = file->can_write,
            .full_name = file->name,
            };
    return result;
}


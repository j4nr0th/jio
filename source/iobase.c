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
#include <stdio.h>

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

#else

#include <stdio.h>
#include <string.h>

bool map_file_is_named(const jio_memory_file* f1, const char* filename)
{
    const bool res = strcmp(f1->name, filename) != 0;
    return res;
}


jio_result jio_memory_file_create(
        const jio_context* ctx, const char* filename, jio_memory_file** p_file_out, int write, int can_create,
        size_t size)
{
    HANDLE file_handle = CreateFileA(
            filename,
            GENERIC_READ | (write ? GENERIC_WRITE : 0),
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            can_create ? OPEN_ALWAYS : OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
                                    );
    if (file_handle == INVALID_HANDLE_VALUE)
    {
        JIO_ERROR(ctx, "Could not get Win32 handle to file \"%s\"", filename);
        return JIO_RESULT_BAD_PATH;
    }
    const BOOL was_not_created = !can_create || GetLastError() == ERROR_ALREADY_EXISTS;
    DWORD map_size_lo = 0, map_size_hi = 0;

    BY_HANDLE_FILE_INFORMATION file_info;

    ULARGE_INTEGER li;
    if (was_not_created)
    {
        if (!GetFileInformationByHandle(file_handle, &file_info))
        {
            JIO_ERROR(ctx, "Could not retrieve Win32 file information by GetFileInformationByHandle");
            CloseHandle(file_handle);
            return JIO_RESULT_BAD_WINDOWS;
        }
        if (size == 0)
        {
            map_size_lo = file_info.nFileSizeLow;
            map_size_hi = file_info.nFileSizeHigh;
            li.LowPart = map_size_lo;
            li.HighPart = map_size_hi;
        }
        else
        {
            li.QuadPart = size;
            map_size_lo = li.LowPart;
            map_size_hi = li.HighPart;
        }
    }
    else
    {
        li.QuadPart = size;
        map_size_lo = li.LowPart;
        map_size_hi = li.HighPart;
    }


    HANDLE view_handle = CreateFileMappingA(
            file_handle,
            NULL,
            write ? PAGE_READWRITE : PAGE_READONLY,
            map_size_hi,
            map_size_lo,
            NULL);
    if (view_handle == INVALID_HANDLE_VALUE)
    {
        JIO_ERROR(ctx, "Could not create Win32 file mapping for file \"%s\"", filename);
        CloseHandle(file_handle);
        return JIO_RESULT_BAD_MAP;
    }

    LPVOID mapping_ptr = MapViewOfFile(view_handle, write ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ, 0, 0, 0);
    if (!mapping_ptr)
    {
        JIO_ERROR(ctx, "Could not map Win32 file view to memory for file \"%s\"", filename);
        CloseHandle(view_handle);
        CloseHandle(file_handle);
        return JIO_RESULT_BAD_MAP;
    }

    jio_memory_file* const this = jio_alloc(ctx, sizeof(*this));
    if (!this)
    {
        JIO_ERROR(ctx, "Could not allocate memory for the memory file");
        CloseHandle(view_handle);
        CloseHandle(file_handle);
        return JIO_RESULT_BAD_ALLOC;
    }

    this->ptr = mapping_ptr;
    this->file_size = size ? size : li.QuadPart;
    this->ctx = ctx;
    if (!GetFinalPathNameByHandleA(file_handle, this->name, sizeof(this->name), VOLUME_NAME_DOS))
    {
        this->name[0] = 0;
        JIO_ERROR(ctx, "Could not get full file path from GetFinalPathNameByHandleA");
    }
    this->can_write = write;
    this->view_handle = view_handle;
    this->file_handle = file_handle;

    *p_file_out = this;

    return JIO_RESULT_SUCCESS;
}


jio_result jio_memory_file_sync(const jio_memory_file* file, int sync)
{
    (void) sync;
    if (!FlushViewOfFile(file->ptr, file->file_size))
    {
        JIO_ERROR(file->ctx, "Could not flush %zu bytes of file \"%s\"", file->file_size, file->name);
    }
    return JIO_RESULT_SUCCESS;
}

void jio_memory_file_destroy(jio_memory_file* mem_file)
{
    const jio_context* ctx = mem_file->ctx;

    (void) UnmapViewOfFile(mem_file->ptr);
    (void) CloseHandle(mem_file->view_handle);
    (void) CloseHandle(mem_file->file_handle);

    jio_free(ctx, mem_file);
}

#endif


unsigned jio_memory_file_count_lines(const jio_memory_file* file)
{
    unsigned count = 1;
    for (const char* ptr = file->ptr; ptr; ptr = memchr(ptr, '\n', file->file_size - (ptr - (const char*) file->ptr)))
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
    const char* const end = (const char*) file->ptr + file->file_size;
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
static void* default_alloc(void* state, size_t size)
{
    (void) state;
    assert(state == (void*)0xBadBabe);
    return malloc(size);
}

static void* default_realloc(void* state, void* ptr, size_t new_size)
{
    (void) state;
    assert(state == (void*)0xBadBabe);
    return realloc(ptr, new_size);
}

static void default_free(void* state, void* ptr)
{
    (void) state;
    assert(state == (void*)0xBadBabe);
    free(ptr);
}

static const jio_allocator_callbacks DEFAULT_ALLOCATOR_CALLBACKS =
        {
                .alloc = default_alloc,
                .realloc = default_realloc,
                .free = default_free,
                .param = (void*)0xBadBabe,
        };

static void default_report(void* state, const char* msg, const char* file, int line, const char* function)
{
    (void) state;
    assert(state == (void*)0xC001Cafe);
    (void)fprintf(stderr, "JIO error (%s:%d - %s): \"%s\"\n", file, line, function, msg);
}

static const jio_error_callbacks DEFAULT_ERROR_CALLBACKS =
        {
                .report = default_report,
                .state = (void*) 0xC001Cafe
        };

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


/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <jsanchez@monoinfinito.net> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Jorge S�nchez
 * ----------------------------------------------------------------------------
 */

#pragma once

#pragma warning(disable: 4189)
#pragma warning(disable: 4201)
#pragma warning(disable: 4100)

#include <stdint.h>
typedef uint8_t   u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef float    f32;
typedef double   f64;
typedef i32      b32;

#define global static
#define internal_variable static

#define INLINE __forceinline
#include <assert.h>
#define Assert(Expr) assert(Expr)

#define Kilobytes(Expr) ((Expr) * 1024)
#define Megabytes(Expr) (Kilobytes(Expr) * 1024)
#define Gigabytes(Expr) (Megabytes(Expr) * 1024)

#define Malloc(Expr) Malloc__((Expr))
#define Free(Expr) Free__((Expr))
global u32 AllocationCount = 0;
global size_t AllocationSize = 0;
void *Malloc__(size_t Size)
{
    AllocationCount += 1;
    return calloc(1, Size);
}

void Free__(void *Ptr)
{
    AllocationCount -= 1;
    free(Ptr);
}


char *ReadTextFile(char *Filename)
{
    // IMPORTANT(Jorge): The caller of this function needs to free the allocated pointer!
    Assert(Filename);

    SDL_RWops *RWops = SDL_RWFromFile(Filename, "rb");
    if (RWops == NULL)
    {
        return NULL;
    }

    size_t FileSize = SDL_RWsize(RWops);
    char* Result = (char*)Malloc(FileSize + 1);
    char* Buffer = Result;

    size_t BytesReadTotal = 0, BytesRead = 1;
    while (BytesReadTotal < FileSize && BytesRead != 0)
    {
        BytesRead = SDL_RWread(RWops, Buffer, 1, (FileSize - BytesReadTotal));
        BytesReadTotal += BytesRead;
        Buffer += BytesRead;
    }

    SDL_RWclose(RWops);
    if (BytesReadTotal != FileSize)
    {
        Free(Result);
        return NULL;
    }

    Result[BytesReadTotal] = '\0';

    return Result;
}


//
// Error Checking Macro's
//

#define NULL_CHECK(Expr)                                                                                    \
    if(!Expr)                                                                                               \
    {                                                                                                       \
        DWORD Err = GetLastError();                                                                         \
        printf("NULL_CHECK TRIGGERED, MSDN ErrorCode: %ld, File: %s Line: %d \n", Err, __FILE__, __LINE__); \
        return NULL;                                                                                        \
    }

#define GLERR                                                                                               \
    do {                                                                                                    \
        GLuint glerr;                                                                                       \
        while((glerr = glGetError()) != GL_NO_ERROR)                                                        \
            fprintf(stderr, "%s:%d glGetError() = 0x%04x\n", __FILE__, __LINE__, glerr);                    \
    } while(0)
/*
Copyright (C) 2025 Tripp Robins

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software
without restriction, including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "../svtl.h"
#include "cthreads.h"
#include "hashmap.h"
#include <math.h>
#include <assert.h>
#define SVTL_DEFAULT_THREAD_COUNT 2u

typedef	uint8_t u8;
typedef int8_t i8;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;
typedef int64_t i64;
typedef float f32;
typedef double f64;


struct SVTL_Instance {
    struct cthreads_thread* threads;
    u16 threadCount;
};

static u64 svtlUsageCount = 0u;
struct SVTL_Instance instance = {NULL, 0};


static void DBG_VALIDATE_INSTANCE_USAGE() {
    #ifndef NDEBUG
        if (0u==instance.threadCount) {
            assert(00&&"SVTL_Init() must be called before usage of any other SVTL functions.");
        }
    #endif
}

static errno_t SVTL_createInstance(struct SVTL_Instance* _instance)
{
    if (_instance->threads)
        return 0;
    _instance->threadCount = SVTL_DEFAULT_THREAD_COUNT;
    _instance->threads = malloc(_instance->threadCount * sizeof(_instance->threads[0]));
    if (!_instance->threads)
        return -1;
    
    return 0u;
}

static void SVTL_destroyInstance(struct SVTL_Instance* _instance)
{
    if (_instance->threads)
        free(_instance->threads);

    _instance->threads = NULL;
    _instance->threadCount = 0u;
}

SVTL_API errno_t SVTL_Register(void)
{
    svtlUsageCount++;
    if (svtlUsageCount==1) {
        return SVTL_createInstance(&instance);
    }
    return 0;
}

SVTL_API void SVTL_Unregister(void)
{
    if (svtlUsageCount>0)
        svtlUsageCount--;
    if (svtlUsageCount==0) {
        SVTL_destroyInstance(&instance);
    }
}

struct SVTL_translate2D_Args
{
    const struct SVTL_VertexInfo* vi; u32 firstVertexIndex; u32 vertexCount; struct SVTL_F64Vec2 displacement;
};
static void* SVTL_translate2D_ThreadSegment(void *v)
{
    struct SVTL_translate2D_Args* args = (struct SVTL_translate2D_Args*)v;
    const struct SVTL_VertexInfo* vi = args->vi;
    u32 firstVertexIndex = args->firstVertexIndex;
    u32 vertexCount = args->vertexCount;
    struct SVTL_F64Vec2 displacement = args->displacement;
    void* vertices = vi->vertices;

    u32 i = firstVertexIndex;
    const u32 end = firstVertexIndex + vertexCount;
    if (vi->positionType == SVTL_POS_TYPE_VEC2_F32) {
        for (; i < end; ++i)
        {
            struct SVTL_F32Vec2* pos = (struct SVTL_F32Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            pos->x += (f32)displacement.x;
            pos->y += (f32)displacement.y;
        }
    }
    else if (vi->positionType == SVTL_POS_TYPE_VEC2_F64)
    {
        for (; i < end; ++i)
        {
            struct SVTL_F64Vec2* pos = (struct SVTL_F64Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            pos->x += displacement.x;
            pos->y += displacement.y;
        }
    }
    return NULL;
}


static u32 getSegmentSize(u32 count, u32 divisions, u32 divisionIdx)
{

    i32 i = divisionIdx;
    i32 j = (count + 1) / divisions;
    i32 k = count - (i + 1) * j;
    if (i == (i32)divisions - 1)
        j += k;
    return j;
}

SVTL_API errno_t SVTL_translate2D(const struct SVTL_VertexInfo* vi, struct SVTL_F64Vec2 displacement)
{
    DBG_VALIDATE_INSTANCE_USAGE();

    struct SVTL_translate2D_Args* dataList = malloc(sizeof(struct SVTL_translate2D_Args) * instance.threadCount);
    struct cthreads_args* argList = malloc(sizeof(struct cthreads_args) * instance.threadCount);
    if (!dataList || !argList) {
        return -1;
    }

    u8 i;
    for (i=0; i < instance.threadCount; ++i)
    {
        struct cthreads_args* args = argList + i;
        struct SVTL_translate2D_Args* fData = &dataList[i];
        fData->vi = vi;
        fData->firstVertexIndex = i * (vi->count + 1) / instance.threadCount;
        fData->vertexCount = getSegmentSize(vi->count, instance.threadCount, i);
        fData->displacement = displacement;
        args->data = fData;
        args->func = SVTL_translate2D_ThreadSegment;
        cthreads_thread_create(instance.threads + i, NULL, args->func, args->data, args);
    }

    for (i = 0; i < instance.threadCount; ++i) 
    {
        int exitCode=0;
        cthreads_thread_join(instance.threads[i], &exitCode);
        if (exitCode != 0) {
            free(dataList);
            free(argList);
            return -1;
        }
    }

    free(dataList);
    free(argList);
    return 0;
}

struct SVTL_rotate2D_Args
{
    const struct SVTL_VertexInfo* vi;  u32 firstVertexIndex; u32 vertexCount; f64 radians; struct SVTL_F64Vec2 origin;
};

static void* SVTL_rotate2D_ThreadSegment(void* v)
{
    struct SVTL_rotate2D_Args* args  = (struct SVTL_rotate2D_Args*)v;

    const struct SVTL_VertexInfo* vi = args->vi;
    u32 firstVertexIndex = args->firstVertexIndex;
    u32 vertexCount = args->vertexCount;
    struct SVTL_F64Vec2 origin = args->origin;
    f64 radians = args->radians;
    void* vertices = vi->vertices;

    f64 c = cos(radians);
    f64 s = sin(radians);

   
    u32 i = firstVertexIndex;
    const u32 end = firstVertexIndex + vertexCount;
    if (vi->positionType == SVTL_POS_TYPE_VEC2_F32) {
        for (; i < end; ++i)
        {
            struct SVTL_F32Vec2* pos = (struct SVTL_F32Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            pos->x -= (f32)origin.x;
            pos->y -= (f32)origin.y;
            f64 rx = pos->x * (f32)c - pos->y * (f32)s;
            f64 ry = pos->x * (f32)s + pos->y * (f32)c;
            pos->x = (f32)(rx + origin.x);
            pos->y = (f32)(ry + origin.y);
        }
    }
    else if (vi->positionType == SVTL_POS_TYPE_VEC2_F64)
    {
        for (; i < end; ++i)
        {
            struct SVTL_F64Vec2* pos = (struct SVTL_F64Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            pos->x -= origin.x;
            pos->y -= origin.y;
            f64 rx = pos->x * c - pos->y * s;
            f64 ry = pos->x * s + pos->y * c;
            pos->x = rx + origin.x;
            pos->y = ry + origin.y;
        }
    }
    return NULL;
}

SVTL_API errno_t SVTL_rotate2D(const struct SVTL_VertexInfo* vi, f64 radians, struct SVTL_F64Vec2 origin)
{
    DBG_VALIDATE_INSTANCE_USAGE();

    struct SVTL_rotate2D_Args* dataList = malloc(sizeof(struct SVTL_rotate2D_Args) * instance.threadCount);
    struct cthreads_args* argList = malloc(sizeof(struct cthreads_args) * instance.threadCount);
    if (!dataList || !argList) {
        return -1;
    }

    u8 i;
    for (i = 0; i < instance.threadCount; ++i)
    {
        struct cthreads_args* args = argList + i;
        struct SVTL_rotate2D_Args* fData = &dataList[i];
        fData->vi = vi;
        fData->firstVertexIndex = i * (vi->count + 1) / instance.threadCount;
        fData->vertexCount = getSegmentSize(vi->count, instance.threadCount, i);
        fData->radians = radians;
        fData->origin = origin;
        args->data = fData;
        args->func = SVTL_rotate2D_ThreadSegment;
        cthreads_thread_create(instance.threads + i, NULL, args->func, args->data, args);
    }

    for (i = 0; i < instance.threadCount; ++i)
    {
        int exitCode = 0;
        cthreads_thread_join(instance.threads[i], &exitCode);
        if (exitCode != 0) {
            free(dataList);
            free(argList);
            return -1;
        }
    }

    free(dataList);
    free(argList);
    return 0;
}


struct SVTL_scale2D_Args
{
    const struct SVTL_VertexInfo* vi;  u32 firstVertexIndex; u32 vertexCount; struct SVTL_F64Vec2 scaleFactor; struct SVTL_F64Vec2 origin;
};
static void* SVTL_scale2D_ThreadSegment(void* v)
{
    struct SVTL_scale2D_Args* args  = (struct SVTL_scale2D_Args*)v;
    const struct SVTL_VertexInfo* vi = args->vi;
    u32 firstVertexIndex = args->firstVertexIndex;
    u32 vertexCount = args->vertexCount;
    struct SVTL_F64Vec2 origin = args->origin;
    struct SVTL_F64Vec2 scaleFactor = args->scaleFactor;
    void* vertices = vi->vertices;

    u32 i = firstVertexIndex;
    const u32 end = firstVertexIndex + vertexCount;
    if (vi->positionType == SVTL_POS_TYPE_VEC2_F32) {
        for (; i < end; ++i)
        {
            struct SVTL_F32Vec2* pos = (struct SVTL_F32Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            pos->x -= (f32)origin.x;
            pos->y -= (f32)origin.y;
            pos->x *= (f32)scaleFactor.x;
            pos->y *= (f32)scaleFactor.y;
            pos->x += (f32)origin.x;
            pos->y += (f32)origin.x;
        }
    }
    else if (vi->positionType == SVTL_POS_TYPE_VEC2_F64)
    {
        for (; i < end; ++i)
        {
            struct SVTL_F64Vec2* pos = (struct SVTL_F64Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            pos->x -= origin.x;
            pos->y -= origin.y;
            pos->x *= scaleFactor.x;
            pos->y *= scaleFactor.y;
            pos->x += origin.x;
            pos->y += origin.x;
        }
    }
    return NULL;
}

SVTL_API errno_t SVTL_scale2D(const struct SVTL_VertexInfo* vi, struct SVTL_F64Vec2 scaleFactor, struct SVTL_F64Vec2 origin)
{
    struct SVTL_scale2D_Args* dataList = malloc(sizeof(struct SVTL_scale2D_Args) * instance.threadCount);
    struct cthreads_args* argList = malloc(sizeof(struct cthreads_args) * instance.threadCount);
    if (!dataList || !argList) {
        return -1;
    }

    u8 i;
    for (i = 0; i < instance.threadCount; ++i)
    {
        struct cthreads_args* args = argList + i;
        struct SVTL_scale2D_Args* fData = &dataList[i];
        fData->vi = vi;
        fData->firstVertexIndex = i * (vi->count + 1) / instance.threadCount;
        fData->vertexCount = getSegmentSize(vi->count, instance.threadCount, i);
        fData->scaleFactor = scaleFactor;
        fData->origin = origin;
        args->data = fData;
        args->func = SVTL_scale2D_ThreadSegment;
        cthreads_thread_create(instance.threads + i, NULL, args->func, args->data, args);
    }

    for (i = 0; i < instance.threadCount; ++i)
    {
        int exitCode = 0;
        cthreads_thread_join(instance.threads[i], &exitCode);
        if (exitCode != 0) {
            free(dataList);
            free(argList);
            return -1;
        }
    }

    free(dataList);
    free(argList);
    return 0;
}
struct SVTL_skew2D_Args
{
    const struct SVTL_VertexInfo* vi;  u32 firstVertexIndex; u32 vertexCount; struct SVTL_F64Vec2 skewFactor; struct SVTL_F64Vec2 origin;
};


static void* SVTL_skew2D_ThreadSegment(void *v)
{
    DBG_VALIDATE_INSTANCE_USAGE();

    struct SVTL_skew2D_Args* args  = (struct SVTL_skew2D_Args*)v;

    const struct SVTL_VertexInfo* vi = args->vi;
    u32 firstVertexIndex = args->firstVertexIndex;
    u32 vertexCount = args->vertexCount;
    struct SVTL_F64Vec2 origin = args->origin;
    struct SVTL_F64Vec2 skewFactor = args->skewFactor;
    void* vertices = vi->vertices;

    u32 i = firstVertexIndex;
    const u32 end = firstVertexIndex + vertexCount;
    if (vi->positionType == SVTL_POS_TYPE_VEC2_F32) {
        for (; i < end; ++i)
        {
            struct SVTL_F32Vec2* pos = (struct SVTL_F32Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            pos->x -= (f32)origin.x;
            pos->y -= (f32)origin.y;
            pos->x = pos->x + (f32)skewFactor.x * pos->y;
            pos->y = pos->y + (f32)skewFactor.y * pos->x;
            pos->x += (f32)origin.x;
            pos->y += (f32)origin.y;
        }
    }
    else if (vi->positionType == SVTL_POS_TYPE_VEC2_F64)
    {
        for (; i < end; ++i)
        {
            struct SVTL_F64Vec2* pos = (struct SVTL_F64Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            pos->x -= origin.x;
            pos->y -= origin.y;
            pos->x = pos->x + skewFactor.x * pos->y;
            pos->y = pos->y + skewFactor.x * pos->x;
            pos->x += origin.x;
            pos->y += origin.y;
        }
    }
    return NULL;
}

SVTL_API errno_t SVTL_skew2D(const struct SVTL_VertexInfo* vi, struct SVTL_F64Vec2 skewFactor, struct SVTL_F64Vec2 origin)
{
    DBG_VALIDATE_INSTANCE_USAGE();

    struct SVTL_skew2D_Args* dataList = malloc(sizeof(struct SVTL_skew2D_Args) * instance.threadCount);
    struct cthreads_args* argList = malloc(sizeof(struct cthreads_args) * instance.threadCount);
    if (!dataList || !argList) {
        return -1;
    }

    u8 i;
    for (i = 0; i < instance.threadCount; ++i)
    {
        struct cthreads_args* args = argList + i;
        struct SVTL_skew2D_Args* fData = &dataList[i];
        fData->vi = vi;
        fData->firstVertexIndex = i * (vi->count + 1) / instance.threadCount;
        fData->vertexCount = getSegmentSize(vi->count, instance.threadCount, i);
        fData->skewFactor = skewFactor;
        fData->origin = origin;
        args->data = fData;
        args->func = SVTL_skew2D_ThreadSegment;
        cthreads_thread_create(instance.threads + i, NULL, args->func, args->data, args);
    }

    for (i = 0; i < instance.threadCount; ++i)
    {
        int exitCode = 0;
        cthreads_thread_join(instance.threads[i], &exitCode);
        if (exitCode != 0) {
            free(dataList);
            free(argList);
            return -1;
        }
    }

    free(dataList);
    free(argList);
    return 0;
}


struct SVTL_mirror2D_Args
{
    const struct SVTL_VertexInfo* vi; u32 firstVertexIndex; u32 vertexCount; struct SVTL_F64Line2 mirrorLine;
};
static void* SVTL_mirror2D_ThreadSegment(void*__args)
{
    struct SVTL_mirror2D_Args* args = __args;
    const struct SVTL_VertexInfo* vi = args->vi;
    const struct SVTL_F64Line2 mirrorLine = args->mirrorLine;
    void* vertices = vi->vertices;
    u32 firstVertexIndex = args->firstVertexIndex;
    u32 vertexCount = args->vertexCount;


    f64 c = cos(mirrorLine.dir);
    f64 s = sin(mirrorLine.dir);
    u32 i = firstVertexIndex;
    const u32 end = firstVertexIndex + vertexCount;
    if (vi->positionType == SVTL_POS_TYPE_VEC2_F32) {
        for (; i < end; ++i)
        {
            struct SVTL_F32Vec2* pos = (struct SVTL_F32Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            pos->x -= mirrorLine.center.x;
            pos->y-= mirrorLine.center.y;

            f32 rx = pos->x * c + pos->y * s;
            f32 ry = pos->x * -s - pos->y * -c;

            pos->x = c * rx - s * ry + mirrorLine.center.x;
            pos->y = s * rx + c * ry + mirrorLine.center.y;
        }
    }
    else if (vi->positionType == SVTL_POS_TYPE_VEC2_F64)
    {
        for (; i < end; ++i)
        {
            struct SVTL_F64Vec2* pos = (struct SVTL_F64Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            pos->x -= mirrorLine.center.x;
            pos->y-= mirrorLine.center.y;

            f64 rx = pos->x * c + pos->y * s;
            f64 ry = pos->x * -s - pos->y * -c;

            pos->x = c * rx - s * ry + mirrorLine.center.x;
            pos->y = s * rx + c * ry + mirrorLine.center.y;
        }
    }
}

SVTL_API errno_t SVTL_mirror2D(const struct SVTL_VertexInfo* vi, struct SVTL_F64Line2 mirrorLine)
{
    DBG_VALIDATE_INSTANCE_USAGE();

    struct SVTL_mirror2D_Args* dataList = malloc(sizeof(struct SVTL_mirror2D_Args) * instance.threadCount);
    struct cthreads_args* argList = malloc(sizeof(struct cthreads_args) * instance.threadCount);
    if (!dataList || !argList) {
        return -1;
    }

    u8 i;
    for (i = 0; i < instance.threadCount; ++i)
    {
        struct cthreads_args* args = argList + i;
        struct SVTL_mirror2D_Args* fData = &dataList[i];
        fData->vi = vi;
        fData->firstVertexIndex = i * (vi->count + 1) / instance.threadCount;
        fData->vertexCount = getSegmentSize(vi->count, instance.threadCount, i);
        fData->mirrorLine = mirrorLine;
        args->data = fData;
        args->func = SVTL_mirror2D_ThreadSegment;
        cthreads_thread_create(instance.threads + i, NULL, args->func, args->data, args);
    }

    for (i = 0; i < instance.threadCount; ++i)
    {
        int exitCode = 0;
        cthreads_thread_join(instance.threads[i], &exitCode);
        if (exitCode != 0) {
            free(dataList);
            free(argList);
            return -1;
        }
    }

    free(dataList);
    free(argList);
    return 0;
}

struct vertexHashmapPair
{
    struct SVTL_F64Vec2 pos;
    void* vertex;
    u32 vIndex;
};


uint64_t user_hash(const void *item, uint64_t seed0, uint64_t seed1) 
{
    const struct vertexHashmapPair *pair = item;
    return hashmap_sip(&pair->pos, sizeof(pair->pos), seed0, seed1);
}


int user_compare(const void *a, const void *b, void *udata) {
    const struct vertexHashmapPair *pairA = a;
    const struct vertexHashmapPair *pairB = b;

    return memcmp(&pairA->pos, &pairB->pos, sizeof(pairA->pos));
}

SVTL_API void SVTL_unindexedToIndexed2D(const struct SVTL_VertexInfoReadOnly* vi, void* verticesOut, uint32_t* vertexCountOut, uint32_t* indicesOut, uint32_t* indexCountOut)
{
    struct hashmap* map = hashmap_new(sizeof(struct vertexHashmapPair), 0, 0, 0, user_hash, user_compare, NULL, NULL);

    u32 i; 
    for (i = 0; i < vi->count; ++i)
    {
        const u8* vertex = (u8*)vi->vertices + vi->stride * i;
        if (vi->positionType == SVTL_POS_TYPE_VEC2_F32) 
        {
            struct SVTL_F32Vec2* pos = vertex + vi->positionOffset;
            hashmap_set(map, &(struct vertexHashmapPair){.pos={pos->x,pos->y}, .vertex = vertex, .vIndex = 0u});
        }
        else if (vi->positionType == SVTL_POS_TYPE_VEC2_F64)
        {
            struct SVTL_F64Vec2* pos = vertex + vi->positionOffset;
            hashmap_set(map, &(struct vertexHashmapPair){.pos={pos->x,pos->y}, .vertex = vertex, .vIndex = 0u});
        }
    }

    size_t iter = 0;
    void *item;
    while (hashmap_iter(map, &iter, &item)) {
        struct vertexHashmapPair *pair = item;
        pair->vIndex = iter;
        
        if (verticesOut) {
            memcpy(verticesOut + iter, pair->vertex, vi->stride);
        }
    }
    if (vertexCountOut)
        *vertexCountOut = iter+1;

    if (indexCountOut)
        *indexCountOut = vi->count;
    
    /*update indices*/
    u32 i; 
    for (i = 0; i < vi->count; ++i)
    {
        const u8* vertex = (u8*)vi->vertices + vi->stride * i;
          if (vi->positionType == SVTL_POS_TYPE_VEC2_F32) 
        {
            struct SVTL_F32Vec2* pos = vertex + vi->positionOffset;
            struct vertexHashmapPair *pair = hashmap_get(map, &(struct vertexHashmapPair){.pos = {pos->x,pos->y}});
            if (indicesOut) {
                *(indicesOut + i) = pair->vIndex;
            }
        }
        else if (vi->positionType == SVTL_POS_TYPE_VEC2_F64)
        {
            struct SVTL_F64Vec2* pos = vertex + vi->positionOffset;
            struct vertexHashmapPair *pair = hashmap_get(map, &(struct vertexHashmapPair){.pos = {pos->x,pos->y}});
             if (indicesOut) {
                *(indicesOut + i) = pair->vIndex;
            }
        }
    }
    return 0;
}

SVTL_API f64 SVTL_findSignedArea(const struct SVTL_VertexInfo* vi)
{
    /*uses the shoelace formula*/
    DBG_VALIDATE_INSTANCE_USAGE();

    const void* vertices = vi->vertices;

    f64 area = 0.0;

    u32 i = 0;
    if (vi->positionType == SVTL_POS_TYPE_VEC2_F32) {
        for (; i < vi->count; ++i)
        {
            struct SVTL_F32Vec2* pos = (struct SVTL_F32Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            struct SVTL_F32Vec2* posNext = (struct SVTL_F32Vec2*)((u8*)vertices + (vi->stride * ((i + 1) % vi->count)) + vi->positionOffset);
            area +=  (pos->x * posNext->y - posNext->x * pos->y);
        }
    }
    else if (vi->positionType == SVTL_POS_TYPE_VEC2_F64)
    {
         for (; i < vi->count; ++i)
        {
            struct SVTL_F64Vec2* pos = (struct SVTL_F64Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            struct SVTL_F64Vec2* posNext = (struct SVTL_F64Vec2*)((u8*)vertices + (vi->stride * ((i + 1) % vi->count)) + vi->positionOffset);
            area +=  (pos->x * posNext->y - posNext->x * pos->y);
        }
    }

    area *= 0.5;
    return area;
}

SVTL_API struct SVTL_F64Vec2 SVTL_findCentroid2D(const struct SVTL_VertexInfo* vi)
{
    DBG_VALIDATE_INSTANCE_USAGE();

    /* https://en.wikipedia.org/wiki/Centroid#Of_a_polygon */

    const void* vertices = vi->vertices;

    const f64 a = SVTL_findSignedArea(vi);

    f64 sumX=0.0;
    f64 sumY=0.0;
    u32 i = 0;
    if (vi->positionType == SVTL_POS_TYPE_VEC2_F32)
     {
        for (; i < vi->count; ++i)
        {
            struct SVTL_F32Vec2* pos = (struct SVTL_F32Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            struct SVTL_F32Vec2* posNext = (struct SVTL_F32Vec2*)((u8*)vertices + (vi->stride * ((i+1) % vi->count)) + vi->positionOffset);
            sumX += (pos->x + posNext->x) * (pos->x*posNext->y - posNext->x*pos->y);
            sumY += (pos->y + posNext->y) * (pos->x * posNext->y - posNext->x * pos->y);
        }
    }
    else if (vi->positionType == SVTL_POS_TYPE_VEC2_F64)
    {
        for (; i < vi->count; ++i)
        {
            struct SVTL_F64Vec2* pos = (struct SVTL_F64Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            struct SVTL_F64Vec2* posNext = (struct SVTL_F64Vec2*)((u8*)vertices + (vi->stride * ((i+1) % vi->count)) + vi->positionOffset);
            sumX += (pos->x + posNext->x) * (pos->x*posNext->y - posNext->x*pos->y);
            sumY += (pos->y + posNext->y) * (pos->x * posNext->y - posNext->x * pos->y);
        }
    }
        
  
    struct SVTL_F64Vec2 retV;
    retV.x = (1.0 / (6.0*a)) * sumX;
    retV.y = (1.0 / (6.0*a)) * sumY;
    return retV;
}

struct SVTL_ExtractVertexPositions2D_Args {
    const struct SVTL_VertexInfo* vi; struct SVTL_F64Vec2* positionsOut; u32 firstVertexIndex; u32 vertexCount;
};

SVTL_API void* SVTL_ExtractVertexPositions2D_ThreadSegment(void* args__) 
{
    DBG_VALIDATE_INSTANCE_USAGE();

    struct SVTL_ExtractVertexPositions2D_Args* args = args__;
    const struct SVTL_VertexInfo* vi = args->vi;
    void* vertices = vi->vertices;
    struct SVTL_F64Vec2* positionsOut = args->positionsOut;

    u32 i = 0;
    if (vi->positionType == SVTL_POS_TYPE_VEC2_F32) 
    {
        for (; i < vi->count; ++i)
        {
            struct SVTL_F32Vec2* pos = (struct SVTL_F32Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            positionsOut[i].x = pos->x;
            positionsOut[i].x = pos->y;
        }
    }
    else if (vi->positionType == SVTL_POS_TYPE_VEC2_F64)
    {
        for (; i < vi->count; ++i)
        {
            struct SVTL_F64Vec2* pos = (struct SVTL_F64Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            positionsOut[i].x = pos->x;
            positionsOut[i].x = pos->y;
        }
    }

    return NULL;
}

SVTL_API errno_t SVTL_extractVertexPositions2D(const struct SVTL_VertexInfo* vi, struct SVTL_F64Vec2* positionsOut)
{
    DBG_VALIDATE_INSTANCE_USAGE();

    struct SVTL_ExtractVertexPositions2D_Args* dataList = malloc(sizeof(struct SVTL_ExtractVertexPositions2D_Args) * instance.threadCount);
    struct cthreads_args* argList = malloc(sizeof(struct cthreads_args) * instance.threadCount);
    if (!dataList || !argList) {
        return -1;
    }

    u8 i;
    for (i = 0; i < instance.threadCount; ++i)
    {
        struct cthreads_args* args = argList + i;
        struct SVTL_ExtractVertexPositions2D_Args* fData = &dataList[i];
        fData->vi = vi;
        fData->positionsOut = positionsOut;
        fData->firstVertexIndex = i * (vi->count + 1) / instance.threadCount;
        fData->vertexCount = getSegmentSize(vi->count, instance.threadCount, i);
        args->data = fData;
        args->func = SVTL_skew2D_ThreadSegment;
        cthreads_thread_create(instance.threads + i, NULL, args->func, args->data, args);
    }

    for (i = 0; i < instance.threadCount; ++i)
    {
        int exitCode = 0;
        cthreads_thread_join(instance.threads[i], &exitCode);
        if (exitCode != 0) {
            free(dataList);
            free(argList);
            return -1;
        }
    }

    free(dataList);
    free(argList);
    return 0;
}

SVTL_API errno_t SVTL_extractVertexPositions2D_s(const struct SVTL_VertexInfo* vi, struct SVTL_F64Vec2* positionsOut, uint64_t posBuffSize)
{
    DBG_VALIDATE_INSTANCE_USAGE();

    if (posBuffSize < vi->count*sizeof(struct SVTL_F64Vec2))
        return -1;
    return SVTL_ExtractVertexPositions2D(vi, positionsOut);
}
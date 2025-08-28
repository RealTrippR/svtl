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
#include "threadpool.h"
#include "hashmap.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#define TASK_COUNT 2u
#define THREAD_TIMEOUT_MS 60000 // 20 seconds

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


static u64 svtlUsageCount = 0u;
ThreadPoolHandle threadPool={0};
bool threadPoolExists=false;

static u16 taskHandleSize=sizeof(ThreadPoolTaskHandle);
static errno_t(*launchTask)(SVTL_Task, SVTL_TaskHandle)=NULL;
static errno_t(*joinTask)(SVTL_TaskHandle)=NULL;

static errno_t defaultLaunchTask(SVTL_Task task, void* tphdl) {
    ThreadPoolTask t = {task.args, task.func};
    return ThreadPool_LaunchTask(threadPool, t,  tphdl);
}
static errno_t defaultJoinTask(void* tphdl) {
    ThreadPool_JoinTask(tphdl);
    return 0;
}
/*
/// sets the callback to launch a task/thread.*/
SVTL_API void SVTL_setTaskLaunchCallback(SVTL_LaunchTask_T cb) {
    launchTask = cb;
    if (launchTask!=defaultLaunchTask) {
        if (threadPoolExists) {
            ThreadPool_Destroy(&threadPool);
            threadPoolExists=false;
        }
    } else {
        if (!threadPoolExists) {
            taskHandleSize=sizeof(ThreadPoolTaskHandle);
            threadPool.threadCount = TASK_COUNT;
            ThreadPool_New(&threadPool, THREAD_TIMEOUT_MS);
            threadPoolExists=true;
        }
    }
}

/*
/// sets the callback to join a task/thread. The argument of the function pointer should be a task handle.*/
SVTL_API void SVTL_setTaskJoinCallback(SVTL_JoinTask_T cb) {
    joinTask = cb;
}

SVTL_API void setTaskHandleSize(u16 size) {
    taskHandleSize = size;
}

static void DBG_VALIDATE_INSTANCE_USAGE() {
    #ifndef NDEBUG
       if (launchTask==NULL || joinTask==NULL) {
            assert(00&&"launchTask & joinTask must be valid function pointers");
       }
    #endif
}

SVTL_API errno_t SVTL_register(void)
{
    svtlUsageCount++;
    if (svtlUsageCount==1) {
        if (launchTask==NULL) {
            SVTL_setTaskLaunchCallback(defaultLaunchTask);
            SVTL_setTaskJoinCallback(defaultJoinTask);
        }
    }
    return 0;
}

SVTL_API void SVTL_unregister(void)
{
    if (svtlUsageCount>0)
        svtlUsageCount--;
    if (svtlUsageCount==0) {
        if (threadPoolExists==true) {
            ThreadPool_Destroy(&threadPool);
            launchTask=NULL;
            joinTask=NULL;
            threadPoolExists=false;
        }
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

static u32 getSegmentSizeGrouped(u32 count, u32 groupSize, u32 divisions, u32 divisionIdx)
{
    u32 groupCount =  (count + groupSize - 1) / groupSize; /*ceil (count/groupSize)*/
    u32 groupsInDiv = groupCount / divisions;
    u32 groupRemainder = groupCount % divisions;
    
    if (divisionIdx < groupRemainder)
        groupsInDiv += 1;
    
    
    u32 max_ = groupsInDiv * groupSize * (divisionIdx+1);
    return max_ > count ? groupSize-(max_-count) : groupsInDiv * groupSize;
}

SVTL_API errno_t SVTL_translate2D(const struct SVTL_VertexInfo* vi, struct SVTL_F64Vec2 displacement)
{
    DBG_VALIDATE_INSTANCE_USAGE();

    struct SVTL_translate2D_Args* dataList = malloc(sizeof(struct SVTL_translate2D_Args) * TASK_COUNT);
    SVTL_TaskHandle* taskHandles = malloc(taskHandleSize * TASK_COUNT);
    if (!dataList || !taskHandles) {
        return -1;
    }

    u8 i;
    for (i=0; i < TASK_COUNT; ++i) {
        SVTL_Task task;
        struct SVTL_translate2D_Args* fData = &dataList[i];
        fData->vi = vi;
        fData->firstVertexIndex = i * (vi->count + 1) / TASK_COUNT;
        fData->vertexCount = getSegmentSize(vi->count, TASK_COUNT, i);
        fData->displacement = displacement;
        task.args = fData;
        task.func = SVTL_translate2D_ThreadSegment;
        launchTask(task,(u8*)taskHandles+(i*taskHandleSize));
    }

    for (i = 0; i < TASK_COUNT; ++i) {
        joinTask((u8*)taskHandles+(i*taskHandleSize));
    }

    free(dataList);
    free(taskHandles);
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

    struct SVTL_rotate2D_Args* argList = malloc(sizeof(struct SVTL_rotate2D_Args) * TASK_COUNT);
    SVTL_TaskHandle* taskHandles = malloc(taskHandleSize * TASK_COUNT);
    if (!argList || !taskHandles) {
        return -1;
    }

    u8 i;
    for (i = 0; i < TASK_COUNT; ++i)
    {
        SVTL_Task task;
        struct SVTL_rotate2D_Args* fData = &argList[i];
        fData->vi = vi;
        fData->firstVertexIndex = i * (vi->count + 1) / TASK_COUNT;
        fData->vertexCount = getSegmentSize(vi->count, TASK_COUNT, i);
        fData->radians = radians;
        fData->origin = origin;
        task.args = fData;
        task.func = SVTL_rotate2D_ThreadSegment;
        launchTask(task,(u8*)taskHandles+(i*taskHandleSize));
    }

    for (i = 0; i < TASK_COUNT; ++i)
    {
        joinTask((u8*)taskHandles+(i*taskHandleSize));
    }

    free(argList);
    free(taskHandles);
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
    struct SVTL_scale2D_Args* argList = malloc(sizeof(struct SVTL_scale2D_Args) * TASK_COUNT);
    SVTL_TaskHandle* taskHandles = malloc(taskHandleSize * TASK_COUNT);
    if (!argList || !taskHandles) {
        return -1;
    }

    u8 i;
    for (i = 0; i < TASK_COUNT; ++i)
    {
        SVTL_Task task;
        struct SVTL_scale2D_Args* fData = &argList[i];
        fData->vi = vi;
        fData->firstVertexIndex = i * (vi->count + 1) / TASK_COUNT;
        fData->vertexCount = getSegmentSize(vi->count, TASK_COUNT, i);
        fData->scaleFactor = scaleFactor;
        fData->origin = origin;
        task.args = fData;
        task.func = SVTL_scale2D_ThreadSegment;
        launchTask(task,(u8*)taskHandles+(i*taskHandleSize));
    }

    for (i = 0; i < TASK_COUNT; ++i)
    {
        joinTask((u8*)taskHandles+(i*taskHandleSize));
    }

    free(argList);
    free(taskHandles);
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

    struct SVTL_skew2D_Args* argList = malloc(sizeof(struct SVTL_skew2D_Args) * TASK_COUNT);
    SVTL_TaskHandle* taskHandles = malloc(taskHandleSize * TASK_COUNT);
    if (!argList || !taskHandles) {
        return -1;
    }

    u8 i;
    for (i = 0; i < TASK_COUNT; ++i)
    {
        SVTL_Task task;
        struct SVTL_skew2D_Args* fData = &argList[i];
        fData->vi = vi;
        fData->firstVertexIndex = i * (vi->count + 1) / TASK_COUNT;
        fData->vertexCount = getSegmentSize(vi->count, TASK_COUNT, i);
        fData->skewFactor = skewFactor;
        fData->origin = origin;
        task.args = fData;
        task.func = SVTL_skew2D_ThreadSegment;
        launchTask(task,(u8*)taskHandles+(i*taskHandleSize));
    }

    for (i = 0; i < TASK_COUNT; ++i)
    {
        joinTask((u8*)taskHandles+(i*taskHandleSize));
    }

    free(taskHandles);
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
            pos->x -= (f32)mirrorLine.center.x;
            pos->y -= (f32)mirrorLine.center.y;

            f32 rx = (f32)(pos->x * c + pos->y * s);
            f32 ry = (f32)(pos->x * -s - pos->y * -c);

            pos->x = (f32)(c * rx - s * ry + mirrorLine.center.x);
            pos->y = (f32)(s * rx + c * ry + mirrorLine.center.y);
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
    return NULL;
}

SVTL_API errno_t SVTL_mirror2D(const struct SVTL_VertexInfo* vi, struct SVTL_F64Line2 mirrorLine)
{
    DBG_VALIDATE_INSTANCE_USAGE();

    struct SVTL_mirror2D_Args* argList = malloc(sizeof(struct SVTL_mirror2D_Args) * TASK_COUNT);
    SVTL_TaskHandle* taskHandles = malloc(taskHandleSize * TASK_COUNT);
    if (!argList || !taskHandles) {
        return -1;
    }

    u8 i;
    for (i = 0; i < TASK_COUNT; ++i)
    {
        SVTL_Task task;
        struct SVTL_mirror2D_Args* fData = &argList[i];
        fData->vi = vi;
        fData->firstVertexIndex = i * (vi->count + 1) / TASK_COUNT;
        fData->vertexCount = getSegmentSize(vi->count, TASK_COUNT, i);
        fData->mirrorLine = mirrorLine;
        task.args = fData;
        task.func = SVTL_mirror2D_ThreadSegment;
        launchTask(task,(u8*)taskHandles+(i*taskHandleSize));
    }

    for (i = 0; i < TASK_COUNT; ++i) {
        joinTask((u8*)taskHandles+(i*taskHandleSize));
    }


    free(taskHandles);
    free(argList);
    return 0;
}

struct vertexHashmapPair
{
    struct SVTL_F64Vec2 pos;
    const void* vertex;
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

SVTL_API errno_t SVTL_unindexedToIndexed2D(const struct SVTL_VertexInfoReadOnly* vi, void* verticesOut, uint32_t* vertexCountOut, uint32_t* indicesOut, uint32_t* indexCountOut)
{
    struct hashmap* map = hashmap_new(sizeof(struct vertexHashmapPair), 0, 0, 0, user_hash, user_compare, NULL, NULL);
    if (!map)
        return -1;

    u32 i; 
    for (i = 0; i < vi->count; ++i)
    {
        const u8* vertex = (u8*)vi->vertices + vi->stride * i;
        if (vi->positionType == SVTL_POS_TYPE_VEC2_F32) 
        {
            const struct SVTL_F32Vec2* pos = (const struct SVTL_F32Vec2*)vertex + vi->positionOffset;
            hashmap_set(map, &(struct vertexHashmapPair){.pos={pos->x,pos->y}, .vertex = vertex, .vIndex = 0u});
        }
        else if (vi->positionType == SVTL_POS_TYPE_VEC2_F64)
        {
            const struct SVTL_F64Vec2* pos =  (const struct SVTL_F64Vec2*)vertex + vi->positionOffset;
            hashmap_set(map, &(struct vertexHashmapPair){.pos={pos->x,pos->y}, .vertex = vertex, .vIndex = 0u});
        }
    }

    if (hashmap_count(map) >UINT32_MAX-64) 
    {
        hashmap_free(map);
        return -1;
    }

    size_t iter = 0;
    void *item;
    while (hashmap_iter(map, &iter, &item)) {
        struct vertexHashmapPair *pair = item;
        pair->vIndex = (u32)iter;
        
        if (verticesOut) {
            memcpy((u8*)verticesOut + iter, pair->vertex, vi->stride);
        }
    }

    if (vertexCountOut)
        *vertexCountOut = (u32)(iter+1);

    if (indexCountOut)
        *indexCountOut = vi->count;
    
    /*update indices*/
    for (i = 0; i < vi->count; ++i)
    {
        const u8* vertex = (u8*)vi->vertices + vi->stride * i;
          if (vi->positionType == SVTL_POS_TYPE_VEC2_F32) 
        {
            const struct SVTL_F32Vec2* pos = (const struct SVTL_F32Vec2*)((u8*)vertex + vi->positionOffset);
            const struct vertexHashmapPair *pair = hashmap_get(map, &(struct vertexHashmapPair){.pos = {pos->x,pos->y}});
            if (indicesOut) {
                *(indicesOut + i) = pair->vIndex;
            }
        }
        else if (vi->positionType == SVTL_POS_TYPE_VEC2_F64)
        {
            const struct SVTL_F64Vec2* pos = (const struct SVTL_F64Vec2*)((u8*)vertex + vi->positionOffset);
            const struct vertexHashmapPair *pair = hashmap_get(map, &(struct vertexHashmapPair){.pos = {pos->x,pos->y}});
            if (indicesOut) {
                *(indicesOut + i) = pair->vIndex;
            }
        }
    }

    hashmap_free(map);
    return 0;
}

struct SVTL_findSignedArea_Args
{
    const struct SVTL_VertexInfoReadOnly* vi;
    u32 firstIndex; u32 count;
    f64* areaOut;
};

SVTL_API void* SVTL_findSignedArea_ThreadSegment(void* __args)
{
    struct SVTL_findSignedArea_Args* args = __args;
    const struct SVTL_VertexInfoReadOnly* vi = args->vi;
    u32 firstIndex = args->firstIndex;
    u32 count = args->count;
    u32 i;
    const u32 end = firstIndex + count;

    if (vi->topologyType==SVTL_TOPOLOGY_TYPE_POINT_LIST)
    {
        f64 area = 0.0;
        for (i = firstIndex; i < end; ++i)
        {
            if (vi->positionType == SVTL_POS_TYPE_VEC2_F32)
            {
                struct SVTL_F32Vec2* posA = (struct SVTL_F32Vec2*)((u8*)vi->vertices + (vi->stride * i) + vi->positionOffset);
                struct SVTL_F32Vec2* posB = (struct SVTL_F32Vec2*)((u8*)vi->vertices + (vi->stride * ((i+1)%vi->count) ) + vi->positionOffset);
                area += (posA->x * posB->y - posB->x * posA->y);
            }
            else if (vi->positionType == SVTL_POS_TYPE_VEC2_F64) {
                struct SVTL_F64Vec2* posA = (struct SVTL_F64Vec2*)((u8*)vi->vertices + (vi->stride * i) + vi->positionOffset);
                struct SVTL_F64Vec2* posB = (struct SVTL_F64Vec2*)((u8*)vi->vertices + (vi->stride * ((i+1)%vi->count) ) + vi->positionOffset);
                area += (posA->x * posB->y - posB->x * posA->y);
            }
        }
        *args->areaOut = area;
        return NULL;
    }

 
    u32 idxA=0u;
    u32 idxB=0u;
    u32 idxC=0u;
    if (vi->topologyType == SVTL_TOPOLOGY_TYPE_TRIANGLE_FAN)
    {
        if (vi->indices != NULL) 
        {
            if (vi->indexType == SVTL_INDEX_TYPE_U16)
            {
                idxA = *(u16*)vi->indices;
            }
             else {
                idxA = *(u32*)vi->indices; 
            }
        }
    } else if (vi->topologyType == SVTL_TOPOLOGY_TYPE_TRIANGLE_STRIP)
    {
        if (firstIndex < 2) {
            firstIndex = 2;
        }
    }

    for (i = firstIndex; i < end;)
    {
        if (vi->topologyType == SVTL_TOPOLOGY_TYPE_TRIANGLE_LIST)
        { 
           
            if (vi->indices != NULL) 
            {
                if (vi->indexType == SVTL_INDEX_TYPE_U16) {
                    idxA = *((u16*)vi->indices + i);
                    idxB = *((u16*)vi->indices + (i + 1));
                    idxC = *((u16*)vi->indices + (i + 2));

                    if (vi->primitiveRestartEnabled) {
                        if (idxA == 0xFFFF) {
                            i++;
                            continue;
                        }
                    }
                } else {
                    idxA = *((u32*)vi->indices + i);
                    idxB = *((u32*)vi->indices + (i + 1));
                    idxC = *((u32*)vi->indices + (i + 2));

                    if (vi->primitiveRestartEnabled) {
                        if (idxA == 0xFFFFFFFF) {
                            i++;
                            continue;
                        }
                    }
                }

               
            } else {
                idxA = i;
                idxB = i+1;
                idxC = i+2;
            }
        }
        else if (vi->topologyType == SVTL_TOPOLOGY_TYPE_TRIANGLE_STRIP)
        {
            if (vi->indices !=NULL)
            {
                if (vi->indexType==SVTL_INDEX_TYPE_U16)
                {
                    idxC = *((u16*)vi->indices + i - 2);
                    idxB = *((u16*)vi->indices + i - 1);
                    idxA = *((u16*)vi->indices + i);
                } else {
                    idxC = *((u32*)vi->indices + i - 2);
                    idxB = *((u32*)vi->indices + i - 1);
                    idxA = *((u32*)vi->indices + i);
                }
            } else {
                idxC = i-2;
                idxB = i-1;
                idxA = i;
            }
        } 
        else if (vi->topologyType == SVTL_TOPOLOGY_TYPE_TRIANGLE_FAN)
        {
            if (vi->indices != NULL) 
            {
                if (vi->indexType == SVTL_INDEX_TYPE_U16)
                {
                    idxB = *((u16*)vi->indices + i);
                    idxC = *((u16*)vi->indices + i + 1);

                    if (vi->primitiveRestartEnabled) {
                        if (idxB == 0xFFFF) {
                            i++;
                            continue;
                        }
                    }
                } 
                else {
                    idxB = *((u32*)vi->indices + i);
                    idxC = *((u32*)vi->indices + i + 1);

                    if (vi->primitiveRestartEnabled) {
                        if (idxC == 0xFFFFFFFF) {
                            i++;
                            continue;
                        }
                    }  
                }
            }
            else {
                idxB = i;
                idxC = i+1;
            }
        }

        if (vi->positionType == SVTL_POS_TYPE_VEC2_F32)
        {
            struct SVTL_F32Vec2* posA = (struct SVTL_F32Vec2*)((u8*)vi->vertices + (vi->stride * idxA) + vi->positionOffset);
            struct SVTL_F32Vec2* posB = (struct SVTL_F32Vec2*)((u8*)vi->vertices + (vi->stride * idxB) + vi->positionOffset);
            struct SVTL_F32Vec2* posC = (struct SVTL_F32Vec2*)((u8*)vi->vertices + (vi->stride * idxC) + vi->positionOffset);

            /*www.omnicalculator.com/math/area-triangle-coordinates*/
            f64 a = 0.5 * (posA->x * (posB->y - posC->y) + posB->x * (posC->y - posA->y) + posC->x * (posA->y - posB->y));
            *args->areaOut += a;
        }
        else if (vi->positionType == SVTL_POS_TYPE_VEC2_F64)
        {
            struct SVTL_F64Vec2* posA = (struct SVTL_F64Vec2*)((u8*)vi->vertices + (vi->stride * idxA) + vi->positionOffset);
            struct SVTL_F64Vec2* posB = (struct SVTL_F64Vec2*)((u8*)vi->vertices + (vi->stride * idxB) + vi->positionOffset);
            struct SVTL_F64Vec2* posC = (struct SVTL_F64Vec2*)((u8*)vi->vertices + (vi->stride * idxC) + vi->positionOffset);

            /*www.omnicalculator.com/math/area-triangle-coordinates*/
            f64 a = 0.5 * (posA->x * (posB->y - posC->y) + posB->x * (posC->y - posA->y) + posC->x * (posA->y - posB->y));
            *args->areaOut +=a;
        }

        if (vi->topologyType == SVTL_TOPOLOGY_TYPE_TRIANGLE_LIST)
        {
            i+=3;
        } else if (vi->topologyType == SVTL_TOPOLOGY_TYPE_TRIANGLE_STRIP)
        {
            i++;
        } else if (vi->topologyType == SVTL_TOPOLOGY_TYPE_TRIANGLE_FAN)
        {
            i++;
        } else {
            break;
        }
    }

    return NULL;
}

SVTL_API f64 SVTL_findSignedArea(const struct SVTL_VertexInfoReadOnly* vi, errno_t* err)
{
    DBG_VALIDATE_INSTANCE_USAGE();

    struct SVTL_findSignedArea_Args* argList = malloc(sizeof(argList[0]) * TASK_COUNT);
    SVTL_TaskHandle* taskHandles = malloc(taskHandleSize * TASK_COUNT);
    f64* areaList = calloc(TASK_COUNT,sizeof(f64));

    if (!taskHandles || !argList || !areaList) {
        if (err)
            *err = -1;
        return 0.0;
    }

    if (vi->count<3) {
        if (err)
            *err = -2;
        return 0;
    }
    u8 i;
    for (i = 0; i < TASK_COUNT; ++i)
    {
        SVTL_Task task;
        struct SVTL_findSignedArea_Args* fData = &argList[i];
        fData->vi = vi;
        fData->firstIndex = i * (vi->count + 1) / TASK_COUNT;
        fData->count = getSegmentSizeGrouped(vi->count, 3, TASK_COUNT, i);
        fData->areaOut = areaList + i;
        task.args = fData;
        task.func = SVTL_findSignedArea_ThreadSegment;
        launchTask(task,(u8*)taskHandles+(i*taskHandleSize));
    }

    for (i = 0; i < TASK_COUNT; ++i) {
        u64 offset = (i*taskHandleSize);
        joinTask((u8*)taskHandles+offset);
    }

    free(taskHandles);
    free(argList);

    if (err)
        *err=0;

    f64 areaSum = 0.0;
    for (i = 0; i < TASK_COUNT; ++i)
    {
        if (vi->topologyType==SVTL_TOPOLOGY_TYPE_POINT_LIST) {
            areaSum += areaList[i]/6.0;
        } else {
            areaSum += areaList[i];
        }
    }

    free(areaList);

    return areaSum;
}

struct SVTL_findCentroid2D_Args
{
    const struct SVTL_VertexInfoReadOnly* vi;
    u32 firstIndex; u32 count;
    f64* areaOut;
    struct SVTL_F64Vec2* centroidSumOut;
};

SVTL_API void* SVTL_findCentroid2D_ThreadSegment(void* __args)
{
    struct SVTL_findCentroid2D_Args* args = __args;
    const struct SVTL_VertexInfoReadOnly* vi = args->vi;
    u32 firstIndex = args->firstIndex;
    u32 count = args->count;
    u32 i;
    const u32 end = firstIndex + count;

    if (vi->topologyType==SVTL_TOPOLOGY_TYPE_POINT_LIST)
    {
        f64 area = 0.0;
        struct SVTL_F64Vec2 c = {0,0};
        for (i = firstIndex; i < end; ++i)
        {
            if (vi->positionType == SVTL_POS_TYPE_VEC2_F32)
            {
                struct SVTL_F32Vec2* posA = (struct SVTL_F32Vec2*)((u8*)vi->vertices + (vi->stride * i) + vi->positionOffset);
                struct SVTL_F32Vec2* posB = (struct SVTL_F32Vec2*)((u8*)vi->vertices + (vi->stride * ((i+1)%vi->count) ) + vi->positionOffset);
                area += (posA->x * posB->y - posB->x * posA->y);
                c.x += (posA->x + posB->x) * (posA->x * posB->y - posB->x * posA->y);
                c.y += (posA->y + posB->y) * (posA->x * posB->y - posB->x * posA->y);
            }
            else if (vi->positionType == SVTL_POS_TYPE_VEC2_F64) {
                struct SVTL_F64Vec2* posA = (struct SVTL_F64Vec2*)((u8*)vi->vertices + (vi->stride * i) + vi->positionOffset);
                struct SVTL_F64Vec2* posB = (struct SVTL_F64Vec2*)((u8*)vi->vertices + (vi->stride * ((i+1)%vi->count) ) + vi->positionOffset);
                area += (posA->x * posB->y - posB->x * posA->y);
                c.x += (posA->x + posB->x) * (posA->x * posB->y - posB->x * posA->y);
                c.y += (posA->y + posB->y) * (posA->x * posB->y - posB->x * posA->y);
            }
        }
        *args->areaOut = area * 0.5;
        *args->centroidSumOut = c;
        return NULL;
    }
 
    u32 idxA=0u;
    u32 idxB=0u;
    u32 idxC=0u;
    if (vi->topologyType == SVTL_TOPOLOGY_TYPE_TRIANGLE_FAN)
    {
        if (vi->indices != NULL) 
        {
            if (vi->indexType == SVTL_INDEX_TYPE_U16)
            {
                idxA = *(u16*)vi->indices;
            }
             else {
                idxA = *(u32*)vi->indices; 
            }
        }
    } else if (vi->topologyType == SVTL_TOPOLOGY_TYPE_TRIANGLE_STRIP)
    {
        if (firstIndex < 2) {
            firstIndex = 2;
        }
    }

    for (i = firstIndex; i < end;)
    {
        if (vi->topologyType == SVTL_TOPOLOGY_TYPE_TRIANGLE_LIST)
        { 
           
            if (vi->indices != NULL) 
            {
                if (vi->indexType == SVTL_INDEX_TYPE_U16) {
                    idxA = *((u16*)vi->indices + i);
                    idxB = *((u16*)vi->indices + (i + 1));
                    idxC = *((u16*)vi->indices + (i + 2));

                    if (vi->primitiveRestartEnabled) {
                        if (idxA == 0xFFFF) {
                            i++;
                            continue;
                        }
                    }
                } else {
                    idxA = *((u32*)vi->indices + i);
                    idxB = *((u32*)vi->indices + (i + 1));
                    idxC = *((u32*)vi->indices + (i + 2));

                    if (vi->primitiveRestartEnabled) {
                        if (idxA == 0xFFFFFFFF) {
                            i++;
                            continue;
                        }
                    }
                }

               
            } else {
                idxA = i;
                idxB = i+1;
                idxC = i+2;
            }
        }
        else if (vi->topologyType == SVTL_TOPOLOGY_TYPE_TRIANGLE_STRIP)
        {
            if (vi->indices !=NULL)
            {
                if (vi->indexType==SVTL_INDEX_TYPE_U16)
                {
                    idxC = *((u16*)vi->indices + i - 2);
                    idxB = *((u16*)vi->indices + i - 1);
                    idxA = *((u16*)vi->indices + i);
                } else {
                    idxC = *((u32*)vi->indices + i - 2);
                    idxB = *((u32*)vi->indices + i - 1);
                    idxA = *((u32*)vi->indices + i);
                }
            } else {
                idxC = i-2;
                idxB = i-1;
                idxA = i;
            }
        } 
        else if (vi->topologyType == SVTL_TOPOLOGY_TYPE_TRIANGLE_FAN)
        {
            if (vi->indices != NULL) 
            {
                if (vi->indexType == SVTL_INDEX_TYPE_U16)
                {
                    idxB = *((u16*)vi->indices + i);
                    idxC = *((u16*)vi->indices + i + 1);

                    if (vi->primitiveRestartEnabled) {
                        if (idxB == 0xFFFF) {
                            i++;
                            continue;
                        }
                    }
                } 
                else {
                    idxB = *((u32*)vi->indices + i);
                    idxC = *((u32*)vi->indices + i + 1);

                    if (vi->primitiveRestartEnabled) {
                        if (idxC == 0xFFFFFFFF) {
                            i++;
                            continue;
                        }
                    }  
                }
            }
            else {
                idxB = i;
                idxC = i+1;
            }
        }

        if (vi->positionType == SVTL_POS_TYPE_VEC2_F32)
        {
            struct SVTL_F32Vec2* posA = (struct SVTL_F32Vec2*)((u8*)vi->vertices + (vi->stride * idxA) + vi->positionOffset);
            struct SVTL_F32Vec2* posB = (struct SVTL_F32Vec2*)((u8*)vi->vertices + (vi->stride * idxB) + vi->positionOffset);
            struct SVTL_F32Vec2* posC = (struct SVTL_F32Vec2*)((u8*)vi->vertices + (vi->stride * idxC) + vi->positionOffset);

            /*www.omnicalculator.com/math/area-triangle-coordinates*/
            f64 a = fabs(0.5 * (posA->x * (posB->y - posC->y) + posB->x * (posC->y - posA->y) + posC->x * (posA->y - posB->y)));
            struct SVTL_F64Vec3 c={0,0};
            c.x = a * (posA->x+posB->x+posC->x)/3.0;
            c.y = a * (posA->y+posB->y+posC->y)/3.0;

            *(args->areaOut) += a;
            args->centroidSumOut->x += c.x;
            args->centroidSumOut->y += c.y;
        }
        else if (vi->positionType == SVTL_POS_TYPE_VEC2_F64)
        {
            struct SVTL_F64Vec2* posA = (struct SVTL_F64Vec2*)((u8*)vi->vertices + (vi->stride * idxA) + vi->positionOffset);
            struct SVTL_F64Vec2* posB = (struct SVTL_F64Vec2*)((u8*)vi->vertices + (vi->stride * idxB) + vi->positionOffset);
            struct SVTL_F64Vec2* posC = (struct SVTL_F64Vec2*)((u8*)vi->vertices + (vi->stride * idxC) + vi->positionOffset);

            /*www.omnicalculator.com/math/area-triangle-coordinates*/
            f64 a = fabs(0.5 * (posA->x * (posB->y - posC->y) + posB->x * (posC->y - posA->y) + posC->x * (posA->y - posB->y)));
            struct SVTL_F64Vec3 c={0,0};
            c.x = a * (posA->x+posB->x+posC->x)/3.0;
            c.y = a * (posA->y+posB->y+posC->y)/3.0;

            *(args->areaOut) += a;
            args->centroidSumOut->x += c.x;
            args->centroidSumOut->y += c.y;
        }

        if (vi->topologyType == SVTL_TOPOLOGY_TYPE_TRIANGLE_LIST)
        {
            i+=3;
        } else if (vi->topologyType == SVTL_TOPOLOGY_TYPE_TRIANGLE_STRIP)
        {
            i++;
        } else if (vi->topologyType == SVTL_TOPOLOGY_TYPE_TRIANGLE_FAN)
        {
            i++;
        } else {
            break;
        }
    }

    return NULL;
}

SVTL_API struct SVTL_F64Vec2 SVTL_findCentroid2D(const struct SVTL_VertexInfoReadOnly* vi, errno_t* err)
{
    struct SVTL_F64Vec2 retV = {0,0};
    
    DBG_VALIDATE_INSTANCE_USAGE();

    struct SVTL_findCentroid2D_Args* dataList = malloc(sizeof(dataList[0]) * TASK_COUNT);
    SVTL_TaskHandle* taskHandles = malloc(taskHandleSize * TASK_COUNT);
    f64* areaList = calloc(TASK_COUNT,sizeof(areaList[0]));
    struct SVTL_F64Vec2* centroidSumList = calloc(TASK_COUNT,sizeof(centroidSumList[0]));

    if (!dataList || !taskHandles) {
        if (err)
            *err = -1;
        return retV;
    }

    u8 i;
    for (i = 0; i < TASK_COUNT; ++i)
    {
        SVTL_Task task;
        struct SVTL_findCentroid2D_Args* fData = &dataList[i];
        fData->vi = vi;
        fData->firstIndex =  getSegmentSizeGrouped(vi->count, 3, TASK_COUNT, 0)*i;
        fData->count = getSegmentSizeGrouped(vi->count, 3, TASK_COUNT, i);
        fData->areaOut = areaList + i;
        fData->centroidSumOut = centroidSumList + i;
        task.args = fData;
        task.func = SVTL_findCentroid2D_ThreadSegment;
        if (launchTask(task, taskHandles+i)) {
            if (err) {
                *err=-1;
            }
        }
    }

    for (i = 0; i < TASK_COUNT; ++i) {
       joinTask(taskHandles+i);
    }

    f64 area=0;
    for (i = 0; i < TASK_COUNT; ++i) {
        retV.x+=centroidSumList[i].x;
        retV.y+=centroidSumList[i].y;
        area+=areaList[i];
    }

    if (vi->topologyType==SVTL_TOPOLOGY_TYPE_POINT_LIST) {
        retV.x = retV.x/(area*6.0);
        retV.y = retV.y/(area*6.0);
    } else {
        retV.x = retV.x/area;
        retV.y = retV.y/area;
    }

    free(dataList);
    free(taskHandles);
    free(centroidSumList);
    free(areaList);

    if (err)
        *err=0;

    return retV;
}

SVTL_API errno_t SVTL_extractVertexPositions2D(const struct SVTL_VertexInfoReadOnly* vi, struct SVTL_F64Vec2* positionsOut)
{
    DBG_VALIDATE_INSTANCE_USAGE();

    const void* vertices = vi->vertices;

    u32 i = 0;
    if (vi->positionType == SVTL_POS_TYPE_VEC2_F32) 
    {
        for (; i < vi->count; ++i)
        {
            struct SVTL_F32Vec2* pos = (struct SVTL_F32Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            positionsOut[i].x = pos->x;
            positionsOut[i].y = pos->y;
        }
    }
    else if (vi->positionType == SVTL_POS_TYPE_VEC2_F64)
    {
        for (; i < vi->count; ++i)
        {
            struct SVTL_F64Vec2* pos = (struct SVTL_F64Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
            positionsOut[i].x = pos->x;
            positionsOut[i].y = pos->y;
        }
    }

    return 0;
}

SVTL_API errno_t SVTL_extractVertexPositions2D_s(const struct SVTL_VertexInfoReadOnly* vi, struct SVTL_F64Vec2* positionsOut, uint64_t posBuffSize)
{
    DBG_VALIDATE_INSTANCE_USAGE();

    if (posBuffSize < vi->count*sizeof(struct SVTL_F64Vec2))
        return -1;
    return SVTL_extractVertexPositions2D(vi, positionsOut);
}
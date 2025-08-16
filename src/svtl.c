#include <svtl.h>
#include <math.h>
#include <src/cthreads.h>

#define SVTL_DEFAULT_THREAD_COUNT 4u

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

struct SVTL_Instance instance;



errno_t SVTL_createInstance(struct SVTL_Instance* instance)
{
    instance->threadCount = SVTL_DEFAULT_THREAD_COUNT;
    instance->threads = malloc(instance->threadCount * sizeof(instance->threads[0]));
    if (!instance->threads)
        return -1;
    
    return 0u;
}

void SVTL_destroyInstance(struct SVTL_Instance* instance)
{
    if (instance->threads)
        free(instance->threads);

    instance->threads = NULL;
    instance->threadCount = 0u;
}

errno_t SVTL_Init(void)
{
    return SVTL_createInstance(&instance);
}

void SVTL_Terminate(void)
{
    SVTL_destroyInstance(&instance);
}

struct SVTL_translate2D_Args
{
    const struct SVTL_VertexInfo* vi; u32 firstVertexIndex; u32 vertexCount; struct SVTL_F64Vec2 displacement;
};
void* SVTL_translate2D_ThreadSegment(void *v)
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

    long i = divisionIdx;
    long j = (count + 1) / divisions;
    long k = count - (i + 1) * j;
    if (i == divisions - 1)
        j += k;
    return j;
}

errno_t SVTL_translate2D(const struct SVTL_VertexInfo* vi, struct SVTL_F64Vec2 displacement)
{
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

void* SVTL_rotate2D_ThreadSegment(void* v)
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

errno_t SVTL_rotate2D(const struct SVTL_VertexInfo* vi, f64 radians, struct SVTL_F64Vec2 origin)
{
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

void* SVTL_scale2D_ThreadSegment(void* v)
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

errno_t SVTL_scale2D(const struct SVTL_VertexInfo* vi, struct SVTL_F64Vec2 scaleFactor, struct SVTL_F64Vec2 origin)
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


void* SVTL_skew2D_ThreadSegment(void *v)
{
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

errno_t SVTL_skew2D(const struct SVTL_VertexInfo* vi, struct SVTL_F64Vec2 skewFactor, struct SVTL_F64Vec2 origin)
{
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
    return 0;
}

f64 SVTL_findSignedArea(const struct SVTL_VertexInfo* vi)
{
    const void* vertices = vi->vertices;

    f64 area = 0.0;

    u32 i = 0;
    for (; i < vi->count; ++i)
    {
        struct SVTL_F32Vec2* pos = (struct SVTL_F32Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
        struct SVTL_F32Vec2* posNext = (struct SVTL_F32Vec2*)((u8*)vertices + (vi->stride * ((i + 1) % vi->count)) + vi->positionOffset);
        area +=  (pos->x * posNext->y - posNext->x * pos->y);
    }

    area *= 0.5;
    return area;
}

struct SVTL_F64Vec2 SVTL_findCentroid2D(const struct SVTL_VertexInfo* vi)
{
    /* https://en.wikipedia.org/wiki/Centroid#Of_a_polygon */

    const void* vertices = vi->vertices;

    const f64 a = SVTL_findSignedArea(vi);

    f64 sumX=0.0;
    f64 sumY=0.0;
    u32 i = 0;
    for (; i < vi->count; ++i)
    {
        struct SVTL_F32Vec2* pos = (struct SVTL_F32Vec2*)((u8*)vertices + (vi->stride * i + vi->positionOffset));
        struct SVTL_F32Vec2* posNext = (struct SVTL_F32Vec2*)((u8*)vertices + (vi->stride * ((i+1) % vi->count)) + vi->positionOffset);
        sumX += (pos->x + posNext->x) * (pos->x*posNext->y - posNext->x*pos->y);
        sumY += (pos->y + posNext->y) * (pos->x * posNext->y - posNext->x * pos->y);
    }
    
  
    struct SVTL_F64Vec2 retV;
    retV.x = (1.0 / (6.0*a)) * sumX;
    retV.y = (1.0 / (6.0*a)) * sumY;
    return retV;
}
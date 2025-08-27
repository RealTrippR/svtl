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


#ifndef SVTL_H
#define SVTL_H

#if !defined(SVTL_API)
    #if defined(SVTL_DYNAMIC)

        #if defined(SVTL_SHARED_EXPORTS)
            #define SVTL_API __declspec(dllexport)
        #else
            #define SVTL_API __declspec(dllimport)
        #endif

    #else
        #define SVTL_API
    #endif /* !SVTL_DYNAMIC */

    #ifdef __cplusplus 
        #define SVTL_API extern "C"

        #if defined(SVTL_DYNAMIC)
            #if defined(SVTL_SHARED_EXPORTS)
                #define SVTL_API extern "C" __declspec(dllexport)
            #else
                #define SVTL_API extern "C" __declspec(dllimport)
            #endif
        #endif
    #endif
#endif

#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

struct SVTL_F32Vec2
{
    float x;
    float y;
};

struct SVTL_F32Vec3
{
    float x;
    float y;
    float z;
};

struct SVTL_F64Vec2
{
    double x;
    double y;
};

struct SVTL_F64Vec3
{
    double x;
    double y;
    double z;
};

struct SVTL_F64Line2
{
    double dir; /*direction in radians*/
    struct SVTL_F64Vec2 center;
};

enum SVTL_PositionType
{
    SVTL_POS_TYPE_VEC2_F32,
    SVTL_POS_TYPE_VEC2_F64,
};

enum SVTL_IndexType
{
    SVTL_INDEX_TYPE_U16,
    SVTL_INDEX_TYPE_U32,
};

enum SVTL_TopologyType
{
    SVTL_TOPOLOGY_TYPE_TRIANGLE_LIST,
    SVTL_TOPOLOGY_TYPE_TRIANGLE_STRIP,
    SVTL_TOPOLOGY_TYPE_TRIANGLE_FAN
};

struct SVTL_VertexInfo
{
    uint32_t stride;
    uint32_t count;
    void* vertices;
    void* indices;
    enum SVTL_PositionType positionType;
    enum SVTL_IndexType indexType;
    enum SVTL_TopologyType topologyType;
    uint32_t positionOffset;
    bool primitiveRestartEnabled;
};

struct SVTL_VertexInfoReadOnly
{
    uint32_t stride;
    uint32_t count;
    const void* vertices;
    const void* indices;
    enum SVTL_PositionType positionType;
    enum SVTL_IndexType indexType;
    enum SVTL_TopologyType topologyType;
    uint32_t positionOffset;
    bool primitiveRestartEnabled;
};


typedef struct 
{
    void* args;
    void*(*func)(void*);
} SVTL_Task;

typedef void* SVTL_TaskHandle;

typedef errno_t (*SVTL_LaunchTask_T)(SVTL_Task, SVTL_TaskHandle);

typedef errno_t (*SVTL_JoinTask_T)(SVTL_TaskHandle);

/*
/// sets the callback to launch a task/thread.*/
SVTL_API void SVTL_setTaskLaunchCallback(SVTL_LaunchTask_T cb);

/*
/// sets the callback to join a task/thread. The argument of the function pointer should be a task handle.*/
SVTL_API void SVTL_setTaskJoinCallback(SVTL_JoinTask_T cb);

SVTL_API void setTaskHandleSize(uint16_t bytes);

/*
/// Registers a usage of the Simple Vertex Transformation Library.
/// @return errno_t - error code: 0 on success, -1 upon failure */
SVTL_API errno_t SVTL_register(void);

/*
/// Unregisters a usage of the Simple Vertex Transformation Library.*/
SVTL_API void SVTL_unregister(void);

/*
/// Translates the positions of the given vertices by displacement units.
/// @param SVTL_VertexInfo* vi - vertex info
/// @param SVTL_F64Vec2 displacement - 2D displacement
/// @return errno_t - error code: 0 on success, -1 upon failure */
SVTL_API errno_t SVTL_translate2D(const struct SVTL_VertexInfo* vi, struct SVTL_F64Vec2 displacement);

/*
/// Rotates the positions of the given vertices around the origin by radians.
/// @param SVTL_VertexInfo* vi - vertex info
/// @param double radians - rotation in radians
/// @param SVTL_F64Vec2 origin - the origin of the rotation
/// @return errno_t - error code: 0 on success, -1 upon failure */
SVTL_API errno_t SVTL_rotate2D(const struct SVTL_VertexInfo* vi, double radians, struct SVTL_F64Vec2 origin);

/*
/// Dilates the positions of the given vertices around the origin by the scaleFactor.
/// @param SVTL_VertexInfo* vi - vertex info
/// @param SVTL_F64Vec2 scaleFactor - scale factor.
/// @param SVTL_F64Vec2 origin - the origin of the dilation.
/// @return errno_t - error code: 0 on success, -1 upon failure */
SVTL_API errno_t SVTL_scale2D(const struct SVTL_VertexInfo* vi, struct SVTL_F64Vec2 scaleFactor, struct SVTL_F64Vec2 origin);

/*
/// Skews the positions of the given vertices around the origin by the skewFactor.
/// @param SVTL_VertexInfo* vi - vertex info
/// @param SVTL_F64Vec2 skewFactor - skew factor.
/// @param SVTL_F64Vec2 origin - the origin of the skew.
/// @return errno_t - error code: 0 on success, -1 upon failure */
SVTL_API errno_t SVTL_skew2D(const struct SVTL_VertexInfo* vi, struct SVTL_F64Vec2 skewFactor, struct SVTL_F64Vec2 origin);

/*
/// Mirrors the positions of the given vertices around the mirror line.
/// @param SVTL_VertexInfo* vi - vertex info
/// @param SVTL_F64Line2 mirrorLine - the line around which the mirror is performed
/// @return errno_t - error code: 0 on success, -1 upon failure */
SVTL_API errno_t SVTL_mirror2D(const struct SVTL_VertexInfo* vi, struct SVTL_F64Line2 mirrorLine);


/*
/// Converts a list of unindexed vertices to indexed vertices
/// @param SVTL_VertexInfo* vi - vertex info
/// @param void* verticesOut - a buffer to hold the new list of vertices. It must have a size of vertexCountOut * vi.stride
/// @param void* indicesOut - a buffer to hold the list of indices. It must have a size of indexCountOut * sizeof(u32)
/// @param uint32_t* vertexCountOut - the count of the new list of vertices*/
SVTL_API errno_t SVTL_unindexedToIndexed2D(const struct SVTL_VertexInfoReadOnly* vi, void* verticesOut, uint32_t* vertexCountOut, uint32_t* indicesOut, uint32_t* indexCountOut);

/*
/// Returns the signed area of a simple closed polygon.
/// @param SVTL_VertexInfo* vi - vertex info
/// @return double - the signed area of the polygon */
SVTL_API double SVTL_findSignedArea(const struct SVTL_VertexInfoReadOnly* vi, errno_t* err);

/*
/// Returns the centroid of a simple closed polygon.
/// @param SVTL_VertexInfo* vi - vertex info
/// @return SVTL_F64Vec2 - the centroid of the polygon */
SVTL_API struct SVTL_F64Vec2 SVTL_findCentroid2D(const struct SVTL_VertexInfoReadOnly* vi, errno_t* err);

/*
/// Extracts the positions of the given vertices and stores them in an array with a size of (vi.count * sizeof(SVTL_F64Vec2))
/// @param SVTL_VertexInfo* vi - vertex info
/// @param SVTL_F64Vec2* positionsOut - the buffer to store the positions
/// @return errno_t - error code: 0 on success, -1 upon failure */
SVTL_API errno_t SVTL_extractVertexPositions2D(const struct SVTL_VertexInfoReadOnly* vi, struct SVTL_F64Vec2* positionsOut);

/*
/// Extracts the positions of the given vertices and stores them in an array with a size of (vi.count * sizeof(SVTL_F64Vec2))
/// Identical to SVTL_ExtractVertexPositions2D, except it checks to ensure that the buffer length is of the correct size.
/// @param SVTL_VertexInfo* vi - vertex info
/// @param SVTL_F64Vec2* positionsOut - the buffer to store the positions
/// @param uint64_t posBuffSize - the size of the buffer to store the positions in bytes
/// @return errno_t - error code: 0 on success, -1 upon failure */
SVTL_API errno_t SVTL_extractVertexPositions2D_s(const struct SVTL_VertexInfoReadOnly* vi, struct SVTL_F64Vec2* positionsOut, uint64_t posBuffSize);

#endif /*!SVTL_H*/
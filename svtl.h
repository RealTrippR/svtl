#ifndef SVTL_H
#define SVTL_H

#ifdef SVTL_DYNAMIC
#ifdef SVTL_EXPORTS
#define SVTL_API __declspec(dllexport)
#else
#define SVTL_API __declspec(dllimport)
#endif
#else
#define SVTL_API
#endif // !SVTL_DYNAMIC


#include <errno.h>
#include <stdint.h>


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

enum SVTL_PositionType
{
    SVTL_POS_TYPE_VEC2_F32,
    SVTL_POS_TYPE_VEC3_F32,
    SVTL_POS_TYPE_VEC2_F64,
    SVTL_POS_TYPE_VEC3_F64
};

struct SVTL_VertexInfo
{
    uint32_t stride;
    uint32_t count;
    void* vertices;
    enum SVTL_PositionType positionType;
    uint32_t positionOffset;
};

SVTL_API errno_t SVTL_Init(void);

SVTL_API void SVTL_Terminate(void);

SVTL_API errno_t SVTL_translate2D(const struct SVTL_VertexInfo* vi, struct SVTL_F64Vec2 displacement);

SVTL_API errno_t SVTL_rotate2D(const struct SVTL_VertexInfo* vi, double radians, struct SVTL_F64Vec2 origin);

SVTL_API errno_t SVTL_scale2D(const struct SVTL_VertexInfo* vi, struct SVTL_F64Vec2 scaleFactor, struct SVTL_F64Vec2 origin);

SVTL_API errno_t SVTL_skew2D(const struct SVTL_VertexInfo* vi, struct SVTL_F64Vec2 skewFactor, struct SVTL_F64Vec2 origin);

SVTL_API double SVTL_findSignedArea(const struct SVTL_VertexInfo* vi);

SVTL_API struct SVTL_F64Vec2 SVTL_findCentroid2D(const struct SVTL_VertexInfo* vi);

#endif /*SVTL_H*/
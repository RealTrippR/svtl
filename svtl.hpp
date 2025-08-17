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

namespace svtl
{
    #include <svtl.h>


    struct F32Vec2
    {
        float x;
        float y;
    };

    struct F32Vec3
    {
        float x;
        float y;
        float z;
    };

    struct F64Vec2
    {
        double x;
        double y;
    };

    struct F64Vec3
    {
        double x;
        double y;
        double z;
    };

    enum PositionType
    {
        SVTL_POS_TYPE_VEC2_F32,
        SVTL_POS_TYPE_VEC3_F32,
        SVTL_POS_TYPE_VEC2_F64,
        SVTL_POS_TYPE_VEC3_F64
    };

    struct VertexInfo
    {
        uint32_t stride;
        uint32_t count;
        void* vertices;
        enum SVTL_PositionType positionType;
        uint32_t positionOffset;
    };
    /*
    /// Initializes the Simple Vertex Transformation Library.
    /// @return errno_t - error code: 0 on success, -1 upon failure */
    inline errno_t Init(void) {
        return SVTL_Init();
    }

    /*
    /// Terminates the Simple Vertex Transformation Library. */
    inline void Terminate(void) {
        SVTL_Terminate();
    }

    /*
    /// Translates the positions of the given vertices by displacement units.
    /// @param SVTL_VertexInfo* vi - vertex info
    /// @param SVTL_F64Vec2 displacement - 2D displacement
    /// @return errno_t - error code: 0 on success, -1 upon failure */
    inline errno_t translate2D(const struct VertexInfo* vi, struct F64Vec2 displacement)
    {
        return SVTL_translate2D((SVTL_VertexInfo*)vi, *(SVTL_F64Vec2*)&displacement);
    }

    /*
    /// Rotates the positions of the given vertices around the origin by radians.
    /// @param SVTL_VertexInfo* vi - vertex info
    /// @param double radians - rotation in radians
    /// @param SVTL_F64Vec2 origin - the origin of the rotation
    /// @return errno_t - error code: 0 on success, -1 upon failure */
    inline errno_t rotate2D(const struct VertexInfo* vi, double radians, struct F64Vec2 origin) 
    {
        return SVTL_rotate2D((SVTL_VertexInfo*)vi, radians, *(SVTL_F64Vec2*)&origin);
    }

    /*
    /// Dilates the positions of the given vertices around the origin by the scaleFactor.
    /// @param SVTL_VertexInfo* vi - vertex info
    /// @param SVTL_F64Vec2 scaleFactor - scale factor.
    /// @param SVTL_F64Vec2 origin - the origin of the dilation.
    /// @return errno_t - error code: 0 on success, -1 upon failure */
    inline errno_t scale2D(const struct VertexInfo* vi, struct F64Vec2 scaleFactor, struct F64Vec2 origin)
    {
        return SVTL_scale2D((SVTL_VertexInfo*)vi, *(SVTL_F64Vec2*)&scaleFactor, *(SVTL_F64Vec2*)&origin);
    }

    /*
    /// Skews the positions of the given vertices around the origin by the skewFactor.
    /// @param SVTL_VertexInfo* vi - vertex info
    /// @param SVTL_F64Vec2 skewFactor - skew factor.
    /// @param SVTL_F64Vec2 origin - the origin of the skew.
    /// @return errno_t - error code: 0 on success, -1 upon failure */
    inline errno_t skew2D(const struct VertexInfo* vi, struct F64Vec2 skewFactor, struct F64Vec2 origin)
    {
        return SVTL_skew2D((SVTL_VertexInfo*)vi, *(SVTL_F64Vec2*)&skewFactor, *(SVTL_F64Vec2*)&origin);
    }

    /*
    /// Returns the signed area of a simple closed polygon.
    /// @param SVTL_VertexInfo* vi - vertex info
    /// @return double - the signed area of the polygon. */
    inline double findSignedArea(const struct VertexInfo* vi)
    {
        return SVTL_findSignedArea((SVTL_VertexInfo*)vi);
    }

    
    /*
    /// Extracts the positions of the given vertices and stores them in an array with a size of (vi.count * sizeof(SVTL_F64Vec2))
    /// @param SVTL_VertexInfo* vi - vertex info
    /// @param SVTL_F64Vec2* positionsOut - the buffer to store the positions*/
    inline errno_t ExtractVertexPositions2D(const struct VertexInfo* vi, F64Vec2* positionsOut)
    {
        return SVTL_ExtractVertexPositions2D((SVTL_VertexInfo*)vi, (SVTL_F64Vec2*)positionsOut);
    }

        
    
    /*
    /// Extracts the positions of the given vertices and stores them in an array with a size of (vi.count * sizeof(SVTL_F64Vec2))
    /// Identical to SVTL_ExtractVertexPositions2D, except it checks to ensure that the buffer length is of the correct size.
    /// @param SVTL_VertexInfo* vi - vertex info
    /// @param SVTL_F64Vec2* positionsOut - the buffer to store the positions*/
    inline errno_t ExtractVertexPositions2D_s(const struct VertexInfo* vi, F64Vec2* positionsOut, uint64_t buffSize)
    {
        return SVTL_ExtractVertexPositions2D_s((SVTL_VertexInfo*)vi, (SVTL_F64Vec2*)positionsOut, buffSize);
    }
}
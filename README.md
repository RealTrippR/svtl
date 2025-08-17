## Simple Vertex Transformation Library ##
<ins> Brief </ins>

SVTL is a basic vertex transformation library written in C89. It provides a handful of functions to perform basic geometric transformations on vertices.

<ins> Functions </ins>

```SVTL_Init``` - Initializes SVTL
```SVTL_Terminate``` - Terminates SVTL

```SVTL_translate2D``` - translates by a given displacement

```SVTL_rotate2D``` - rotates around the origin

```SVTL_scale2D``` - scales relative to the origin

```SVTL_skew2D``` - skews relative to the origin

```SVTL_findSignedArea``` - returns the signed area of a simple closed polygon

```SVTL_findCentroid2D``` - returns the centroid of a simple closed polygon

```SVTL_ExtractVertexPositions2D``` - writes vertex positions to a buffer

```SVTL_ExtractVertexPositions2D_s``` - writes vertex positions to a bounds checked buffer

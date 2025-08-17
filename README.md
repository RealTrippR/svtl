## Simple Vertex Transformation Library ##
<ins> Brief </ins>

SVTL is a basic vertex transformation library written in C89. It provides a handful of functions to perform basic geometric transformations on vertices.

<ins> **Functions** </ins>

```SVTL_Init``` - Initializes SVTL </br>
```SVTL_Terminate``` - Terminates SVTL </br>
```SVTL_translate2D``` - translates by a given displacement </br>
```SVTL_rotate2D``` - rotates around the origin </br>
```SVTL_scale2D``` - scales relative to the origin </br>
```SVTL_skew2D``` - skews relative to the origin </br>
```SVTL_findSignedArea``` - returns the signed area of a simple closed polygon </br>
```SVTL_findCentroid2D``` - returns the centroid of a simple closed polygon </br>
```SVTL_ExtractVertexPositions2D``` - writes vertex positions to a buffer </br>
```SVTL_ExtractVertexPositions2D_s``` - writes vertex positions to a bounds checked buffer </br>

<ins> **Performance** </ins>

<ins> **Naming Conventions** </ins>
- Preprocessor Macros: UPPER_SNAKE_CASE
- Function Names: CamelCase
- Variable names: pascalCase
- Enum Types: CamelCase
- Enum Values: UPPER_SNAKE_CASE

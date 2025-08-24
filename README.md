## Simple Vertex Transformation Library ##
<ins> Brief </ins>

SVTL is a basic vertex transformation library written in C89. It provides a handful of functions to perform basic geometric transformations on vertices.

<ins> **Functions** </ins>

```SVTL_register``` - registers a usage of SVTL </br>
```SVTL_unregister``` - unregisters a usage of SVTL </br>
```SVTL_translate2D``` - translates by a given displacement </br>
```SVTL_rotate2D``` - rotates around the origin </br>
```SVTL_scale2D``` - scales relative to the origin </br>
```SVTL_skew2D``` - skews relative to the origin </br>
```SVTL_mirror2D``` - mirrors around a given line </br>
```SVTL_unindexedToIndexed2D``` - converts a list of unindexed vertices to indexed vertices </br>
```SVTL_findSignedArea``` - returns the signed area of a simple closed polygon </br>
```SVTL_findCentroid2D``` - returns the centroid of a simple closed polygon </br>
```SVTL_extractVertexPositions2D``` - writes vertex positions to a buffer </br>
```SVTL_extractVertexPositions2D_s``` - writes vertex positions to a bounds checked buffer </br>

<ins> **Example** </ins>
```
#include <svtl.h>

struct Vertex2D {
	float x;
	float y;

	float r;
	float g;
	float b;
};

int main()
{
	SVTL_Init();

	struct Vertex2D vertices[4] = { {0,0}, {1,0}, {1,1}, {0,1} };

	struct SVTL_VertexInfo vi;
	vi.count = sizeof(vertices)/sizeof(vertices[0]);
	vi.stride = sizeof(struct Vertex2D);
	vi.positionOffset = 0u;
	vi.positionType = SVTL_POS_TYPE_VEC2_F32;
	vi.vertices = vertices;

	struct SVTL_F64Vec2 v = { 0, 1 };
	struct SVTL_F64Vec2 v2 = { 0, 1 };

	SVTL_translate2D(&vi, v);

	v.x = 0.f;
	v.y = 0.f;
	v2.x = 2.f;
	v2.y = 2.f;
	SVTL_scale2D(&vi, v2, v);

	v.x = 0;
	v.y = 0;

	SVTL_rotate2D(&vi, 3.141592653589, v);
	SVTL_rotate2D(&vi, -3.141592653589, v);



	v.x = 1.f;
	v.y = 0.f;
	v2.x = 1.f;
	v2.y = 3.f;
	SVTL_skew2D(&vi, v, v2);
	
	struct SVTL_F64Vec2 vc = SVTL_findCentroid2D(&vi);

	SVTL_Terminate();
}
```

<ins> **Naming Conventions** </ins>
- Preprocessor Macros: UPPER_SNAKE_CASE
- Function Names: CamelCase
- Variable names: pascalCase
- Enum Types: CamelCase
- Enum Values: UPPER_SNAKE_CASE

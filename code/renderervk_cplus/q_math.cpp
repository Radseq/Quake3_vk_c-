// todo this file should not be there i think

#include "q_math.hpp"

void ClearBounds_plus(vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}

int BoxOnPlaneSide_plus(vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	float dist[2];
	int sides, b, i;

	// fast axial cases
	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}

	// general case
	dist[0] = dist[1] = 0;
	if (p->signbits < 8) // >= 8: default case is original code (dist[0]=dist[1]=0)
	{
		for (i = 0; i < 3; i++)
		{
			b = (p->signbits >> i) & 1;
			dist[b] += p->normal[i] * emaxs[i];
			dist[!b] += p->normal[i] * emins[i];
		}
	}

	sides = 0;
	if (dist[0] >= p->dist)
		sides = 1;
	if (dist[1] < p->dist)
		sides |= 2;

	return sides;
}

void SetPlaneSignbits_plus(cplane_t *out)
{
	int bits, j;

	// for fast box on planeside test
	bits = 0;
	for (j = 0; j < 3; j++)
	{
		if (out->normal[j] < 0)
		{
			bits |= 1 << j;
		}
	}
	out->signbits = bits;
}

vec_t VectorNormalize_plus(vec3_t v)
{
	// NOTE: TTimo - Apple G4 altivec source uses double?
	float length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];

	if (length)
	{
		/* writing it this way allows gcc to recognize that rsqrt can be used */
		ilength = 1 / (float)sqrt(length);
		/* sqrt(length) = length * (1 / sqrt(length)) */
		length *= ilength;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}

/*
=====================
PlaneFromPoints

Returns false if the triangle is degenerate.
The normal will point out of the clock for clockwise ordered points
=====================
*/
bool PlaneFromPoints_plus(vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c)
{
	vec3_t d1, d2;

	VectorSubtract(b, a, d1);
	VectorSubtract(c, a, d2);
	CrossProduct(d2, d1, plane);
	if (VectorNormalize_plus(plane) == 0)
	{
		return false;
	}

	plane[3] = DotProduct(a, plane);
	return true;
}

/*
===============
RotatePointAroundVector

This is not implemented very well...
===============
*/
void RotatePointAroundVector_plus(vec3_t dst, const vec3_t dir, const vec3_t point,
								  float degrees)
{
	float m[3][3];
	float im[3][3];
	float zrot[3][3];
	float tmpmat[3][3];
	float rot[3][3];
	int i;
	vec3_t vr, vup, vf;
	float rad;

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

	PerpendicularVector_plus(vr, dir);
	CrossProduct(vr, vf, vup);

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	memcpy(im, m, sizeof(im));

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	memset(zrot, 0, sizeof(zrot));
	zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

	rad = DEG2RAD(degrees);
	zrot[0][0] = cos(rad);
	zrot[0][1] = sin(rad);
	zrot[1][0] = -sin(rad);
	zrot[1][1] = cos(rad);

	MatrixMultiply_plus(m, zrot, tmpmat);
	MatrixMultiply_plus(tmpmat, im, rot);

	for (i = 0; i < 3; i++)
	{
		dst[i] = rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
	}
}

/*
** assumes "src" is normalized
*/
void PerpendicularVector_plus(vec3_t dst, const vec3_t src)
{
	int pos;
	int i;
	float minelem = 1.0F;
	vec3_t tempvec;

	/*
	** find the smallest magnitude axially aligned vector
	*/
	for (pos = 0, i = 0; i < 3; i++)
	{
		if (fabs(src[i]) < minelem)
		{
			pos = i;
			minelem = fabs(src[i]);
		}
	}
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	/*
	** project the point onto the plane defined by src
	*/
	ProjectPointOnPlane_plus(dst, tempvec, src);

	/*
	** normalize the result
	*/
	VectorNormalize_plus(dst);
}

void MatrixMultiply_plus(float in1[3][3], float in2[3][3], float out[3][3])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
}

void ProjectPointOnPlane_plus(vec3_t dst, const vec3_t p, const vec3_t normal)
{
	float d;
	vec3_t n;
	float inv_denom;

	inv_denom = DotProduct(normal, normal);
#ifndef Q3_VM
	assert(Q_fabs_plus(inv_denom) != 0.0f); // zero vectors get here
#endif
	inv_denom = 1.0f / inv_denom;

	d = DotProduct(normal, p) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}

void AxisCopy_plus(vec3_t in[3], vec3_t out[3])
{
	VectorCopy(in[0], out[0]);
	VectorCopy(in[1], out[1]);
	VectorCopy(in[2], out[2]);
}

float Q_fabs_plus(float f)
{
	floatint_t fi;
	fi.f = f;
	fi.i &= 0x7FFFFFFF;
	return fi.f;
}

float Q_rsqrt_plus(float number)
{
#if defined(_MSC_SSE2)
	float ret;
	_mm_store_ss(&ret, _mm_rsqrt_ss(_mm_load_ss(&number)));
	return ret;
#elif defined(_GCC_SSE2)
	/* writing it this way allows gcc to recognize that rsqrt can be used with -ffast-math */
	return 1.0f / sqrtf(number);
#else
	floatint_t t;
	float x2, y;
	const float threehalfs = 1.5F;

	x2 = number * 0.5F;
	t.f = number;
	t.i = 0x5f3759df - (t.i >> 1); // what the fuck?
	y = t.f;
	y = y * (threehalfs - (x2 * y * y)); // 1st iteration
										 //	y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

	return y;
#endif
}

vec_t VectorNormalize2_plus(const vec3_t v, vec3_t out)
{
	float length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];

	if (length)
	{
		/* writing it this way allows gcc to recognize that rsqrt can be used */
		ilength = 1 / (float)sqrt(length);
		/* sqrt(length) = length * (1 / sqrt(length)) */
		length *= ilength;
		out[0] = v[0] * ilength;
		out[1] = v[1] * ilength;
		out[2] = v[2] * ilength;
	}
	else
	{
		VectorClear(out);
	}

	return length;
}

float RadiusFromBounds_plus(const vec3_t mins, const vec3_t maxs)
{
	int i;
	vec3_t corner;
	float a, b;

	for (i = 0; i < 3; i++)
	{
		a = fabs(mins[i]);
		b = fabs(maxs[i]);
		corner[i] = a > b ? a : b;
	}

	return VectorLength(corner);
}

void AddPointToBounds_plus(const vec3_t v, vec3_t mins, vec3_t maxs)
{
	if (v[0] < mins[0])
	{
		mins[0] = v[0];
	}
	if (v[0] > maxs[0])
	{
		maxs[0] = v[0];
	}

	if (v[1] < mins[1])
	{
		mins[1] = v[1];
	}
	if (v[1] > maxs[1])
	{
		maxs[1] = v[1];
	}

	if (v[2] < mins[2])
	{
		mins[2] = v[2];
	}
	if (v[2] > maxs[2])
	{
		maxs[2] = v[2];
	}
}

#ifndef Q3_VM
/*
=====================
Q_acos

the msvc acos doesn't always return a value between -PI and PI:

int i;
i = 1065353246;
acos(*(float*) &i) == -1.#IND0

=====================
*/
float Q_acos_plus(float c)
{
	float angle;

	angle = acos(c);

	if (angle > M_PI)
	{
		return (float)M_PI;
	}
	if (angle < -M_PI)
	{
		return (float)M_PI;
	}
	return angle;
}
#endif

int Q_log2_plus(int val)
{
	int answer;

	answer = 0;
	while ((val >>= 1) != 0)
	{
		answer++;
	}
	return answer;
}

void AxisClear_plus(vec3_t axis[3])
{
	axis[0][0] = 1;
	axis[0][1] = 0;
	axis[0][2] = 0;
	axis[1][0] = 0;
	axis[1][1] = 1;
	axis[1][2] = 0;
	axis[2][0] = 0;
	axis[2][1] = 0;
	axis[2][2] = 1;
}
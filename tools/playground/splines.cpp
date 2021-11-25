#include <rw.h>
#include <skeleton.h>

#include <vector>

using namespace rw;
using namespace RWDEVICE;

static Im3DVertex im3dVerts[1024];
static int numImVerts;
static rw::PrimitiveType imPrim;
static Im3DVertex imVert;

void
BeginIm3D(rw::PrimitiveType prim)
{
	numImVerts = 0;
	imPrim = prim;
}

void
EndIm3D(void)
{
	rw::im3d::Transform(im3dVerts, numImVerts, nil, rw::im3d::EVERYTHING);
	rw::im3d::RenderPrimitive(imPrim);
	rw::im3d::End();
}

void
AddVertex(const rw::V3d &vert)
{
	if(numImVerts >= 1024){
		EndIm3D();
		switch(imPrim){
		case PRIMTYPEPOLYLINE:
			im3dVerts[0] = im3dVerts[numImVerts-1];
			numImVerts = 1;
			break;
		case PRIMTYPETRISTRIP:
			// TODO: winding?
			im3dVerts[0] = im3dVerts[numImVerts-2];
			im3dVerts[1] = im3dVerts[numImVerts-1];
			numImVerts = 2;
			break;
		case PRIMTYPETRIFAN:
			im3dVerts[1] = im3dVerts[numImVerts-1];
			numImVerts = 2;
			break;
		default:
			numImVerts = 0;
		}
	}

	imVert.setX(vert.x);
	imVert.setY(vert.y);
	imVert.setZ(vert.z);
	im3dVerts[numImVerts++] = imVert;
}

float epsilon = 0.000001;

class BezierSurf
{
public:
	// hardcoded 4x4 for now
	V3d verts[16];

	void mirrorX(void);
	void mirrorY(void);

	V3d eval(float u, float v);
	void drawHull(void);
	void drawShaded(void);
};

class RBCurve
{
public:
	int degree;
	std::vector<V3d> verts;
	std::vector<float> weights;	// for rational
	std::vector<float> knots;

	V3d eval(float u);
	void drawHull(void);
	void drawSpline(void);
};

class RBSurf
{
public:
	int degreeU, degreeV;
	int numU, numV;
	std::vector<V3d> verts;
	std::vector<float> weights;
	std::vector<float> knotsU, knotsV;

	void update(void);
	V3d eval(float u, float v);
	void drawHull(void);
	void drawIsoparms(void);
	void drawShaded(void);
};

float div0(float a, float b) { return b == 0.0f ? a : a/b; }

// naive algorithm
float
evalBasis(int i, int d, float u, float knots[])
{
	if(d == 0){
		if(knots[i] <= u && u < knots[i+1])
			return 1.0f;
		return 0.0f;
	}

	float b0 = evalBasis(i, d-1, u, knots);
	float b1 = evalBasis(i+1, d-1, u, knots);
	return b0*div0(u-knots[i], knots[i+d] - knots[i]) + b1*div0(knots[i+d+1]-u, knots[i+d+1] - knots[i+1]);
}

float
evalBasisFast(int i, int d, float u, float knots[])
{
	int r, j;
	float tmp[10];

	// degree 0 values
	for(j = 0; j < d+1; j++)
		tmp[j] = knots[i+j] <= u && u < knots[i+j+1] ? 1.0f : 0.0f;

	// build up from degree zero
	for(r = d, d = 1; r > 0; r--, d++){
		for(j = 0; j < r; j++){
			float t1 = div0(u-knots[i+j], knots[i+j + d] - knots[i+j]);
			float t2 = div0(knots[i+j + d+1]-u, knots[i+j + d+1] - knots[i+j + 1]);
			tmp[j] = tmp[j]*t1 + tmp[j+1]*t2;
		}
	}
	return tmp[0];
}

void
BezierSurf::mirrorX(void)
{
	int i;
	for(i = 0; i < 16; i++)
		verts[i].x = -verts[i].x;
}

void
BezierSurf::mirrorY(void)
{
	int i;
	for(i = 0; i < 16; i++)
		verts[i].y = -verts[i].y;
}

V3d
BezierSurf::eval(float u, float v)
{
	int i, j;
	V3d out = { 0.0f, 0.0f, 0.0f };
	float us[4], vs[4];
	float iu = 1.0f-u;
	float iv = 1.0f-v;
	us[0] = iu*iu*iu;
	us[1] = 3.0f*u*iu*iu;
	us[2] = 3.0f*u*u*iu;
	us[3] = u*u*u;
	vs[0] = iv*iv*iv;
	vs[1] = 3.0f*v*iv*iv;
	vs[2] = 3.0f*v*v*iv;
	vs[3] = v*v*v;
	for(i = 0; i < 4; i++)
		for(j = 0; j < 4; j++)
			out = add(out, scale(verts[j+i*4],us[j]*vs[i]));
	return out;
}

void
BezierSurf::drawHull(void)
{
	rw::SetRenderStatePtr(rw::TEXTURERASTER, nil);
	rw::SetRenderState(rw::FOGENABLE, 0);

	imVert.setU(0.0f);
	imVert.setV(0.0f);
	imVert.setColor(138, 72, 51, 255);
//	imVert.setColor(228, 172, 121, 255);

	int iu, iv;
	for(iv = 0; iv < 4; iv++){
		BeginIm3D(rw::PRIMTYPEPOLYLINE);
		for(iu = 0; iu < 4; iu++)
			AddVertex(verts[iu + iv*4]);
		EndIm3D();
	}

	for(iu = 0; iu < 4; iu++){
		BeginIm3D(rw::PRIMTYPEPOLYLINE);
		for(iv = 0; iv < 4; iv++)
			AddVertex(verts[iu + iv*4]);
		EndIm3D();
	}
}

void
BezierSurf::drawShaded(void)
{
	V3d vert;
	int iu, iv;
	float u, v;

	rw::SetRenderStatePtr(rw::TEXTURERASTER, nil);
	rw::SetRenderState(rw::FOGENABLE, 0);

	imVert.setU(0.0f);
	imVert.setV(0.0f);
	imVert.setColor(0, 128, 240, 255);

	float endu = 1.0f - epsilon;
	float endv = 1.0f - epsilon;
	float uinc = (endu-0.0f)/40.0f;
	float vinc = (endv-0.0f)/40.0f;

	float vnext;
	for(v = 0.0f;; v = vnext){
		if(v > endv)
			v = endv;
		vnext = v + vinc;

		BeginIm3D(rw::PRIMTYPETRISTRIP);
		for(u = 0.0f;; u += uinc){
			if(u > endu)
				u = endu;

			AddVertex(eval(u, v));
			AddVertex(eval(u, vnext));

			if(u >= endu)
				break;
		}
		EndIm3D();
		if(vnext >= endv)
			break;
	}
}

V3d
RBCurve::eval(float u)
{
	int i;
	V3d vert = { 0.0f, 0.0f, 0.0f };
	float w = 0.0f;

	// Find knots we're interested in
	for(i = 0; i < knots.size(); i++)
		if(knots[i] <= u && u < knots[i+1])
			break;
	int startI = i-degree;
	int endI = i+1;

	for(i = startI; i < endI; i++){
		float r = evalBasisFast(i, degree, u, &knots[0]);
		w += weights[i]*r;
		vert = add(vert, scale(verts[i], weights[i]*r));
	}
	return scale(vert, div0(1.0f,w));
}

void
RBCurve::drawHull(void)
{
	int i;

	rw::SetRenderStatePtr(rw::TEXTURERASTER, nil);
	rw::SetRenderState(rw::FOGENABLE, 0);

	BeginIm3D(rw::PRIMTYPEPOLYLINE);
	imVert.setU(0.0f);
	imVert.setV(0.0f);
	imVert.setColor(138, 72, 51, 255);
//	imVert.setColor(228, 172, 121, 255);
	for(i = 0; i < verts.size(); i++)
		AddVertex(verts[i]);
	EndIm3D();
}

void
RBCurve::drawSpline(void)
{
	int i;
	float u, endu;
	V3d vert;

	rw::SetRenderStatePtr(rw::TEXTURERASTER, nil);
	rw::SetRenderState(rw::FOGENABLE, 0);
	BeginIm3D(rw::PRIMTYPEPOLYLINE);
	imVert.setU(0.0f);
	imVert.setV(0.0f);
	imVert.setColor(0, 4, 96, 255);

	u = knots[0];
	endu = knots[knots.size()-1] - epsilon;
	float uinc = (endu-knots[0])/40.0f;
	for(;; u += uinc){
		if(u > endu)
			u = endu;
		AddVertex(eval(u));
		if(u >= endu)
			break;
	}

	EndIm3D();
}

void
RBSurf::update(void)
{
	numU = knotsU.size() - degreeU - 1;
	numV = knotsV.size() - degreeV - 1;
}

V3d
RBSurf::eval(float u, float v)
{
	int i, j;
	V3d vert = { 0.0f, 0.0f, 0.0f };
	float w = 0.0f;

	float basisU[10], basisV[10];

	// Find knots we're interested in
	int k;
	for(k = 0; k < knotsU.size(); k++)
		if(knotsU[k] <= u && u < knotsU[k+1])
			break;
	int startI = k-degreeU;
	int endI = k+1;
	for(k = 0; k < knotsV.size(); k++)
		if(knotsV[k] <= v && v < knotsV[k+1])
			break;
	int startJ = k-degreeV;
	int endJ = k+1;

	for(i = startI; i < endI; i++) basisU[i-startI] = evalBasisFast(i, degreeU, u, &knotsU[0]);
	for(j = startJ; j < endJ; j++) basisV[j-startJ] = evalBasisFast(j, degreeV, v, &knotsV[0]);

	for(j = startJ; j < endJ; j++)
		for(i = startI; i < endI; i++){
			float r = basisV[j-startJ]*basisU[i-startI];
			w += weights[j*numU + i]*r;
			vert = add(vert, scale(verts[j*numU + i], weights[j*numU + i]*r));
		}
	return scale(vert, div0(1.0f,w));
}

void
RBSurf::drawHull(void)
{
	rw::SetRenderStatePtr(rw::TEXTURERASTER, nil);
	rw::SetRenderState(rw::FOGENABLE, 0);

	imVert.setU(0.0f);
	imVert.setV(0.0f);
	imVert.setColor(138, 72, 51, 255);
//	imVert.setColor(228, 172, 121, 255);

	int iu, iv;
	for(iv = 0; iv < numV; iv++){
		BeginIm3D(rw::PRIMTYPEPOLYLINE);
		for(iu = 0; iu < numU; iu++)
			AddVertex(verts[iu + iv*numU]);
		EndIm3D();
	}

	for(iu = 0; iu < numU; iu++){
		BeginIm3D(rw::PRIMTYPEPOLYLINE);
		for(iv = 0; iv < numV; iv++)
			AddVertex(verts[iu + iv*numU]);
		EndIm3D();
	}
}

void
RBSurf::drawIsoparms(void)
{
	V3d vert;
	int iu, iv;
	float u, v;

	rw::SetRenderStatePtr(rw::TEXTURERASTER, nil);
	rw::SetRenderState(rw::FOGENABLE, 0);

	imVert.setU(0.0f);
	imVert.setV(0.0f);
	imVert.setColor(0, 4, 96, 255);

	float endu = knotsU[knotsU.size()-1] - epsilon;
	float endv = knotsU[knotsV.size()-1] - epsilon;
	float uinc = (endu-knotsU[0])/40.0f;
	float vinc = (endv-knotsV[0])/40.0f;

	v = -100000.0f;
	for(iv = 0; iv < knotsV.size(); iv++){
		if(knotsV[iv] <= v) continue;
		v = knotsV[iv];
		if(v > endv) v = endv;

		BeginIm3D(rw::PRIMTYPEPOLYLINE);
		for(u = knotsU[0];; u += uinc){
			if(u > endu)
				u = endu;
			AddVertex(eval(u, v));
			if(u >= endu)
				break;
		}
		EndIm3D();
	}

	u = -100000.0f;
	for(iu = 0; iu < knotsU.size(); iu++){
		if(knotsU[iu] <= u) continue;
		u = knotsU[iu];
		if(u > endu) u = endu;

		BeginIm3D(rw::PRIMTYPEPOLYLINE);
		for(v = knotsV[0];; v += vinc){
			if(v > endv)
				v = endv;
			AddVertex(eval(u, v));
			if(v >= endv)
				break;
		}
		EndIm3D();
	}
}

void
RBSurf::drawShaded(void)
{
	V3d vert;
	int iu, iv;
	float u, v;

	rw::SetRenderStatePtr(rw::TEXTURERASTER, nil);
	rw::SetRenderState(rw::FOGENABLE, 0);

	imVert.setU(0.0f);
	imVert.setV(0.0f);
	imVert.setColor(0, 128, 240, 255);

	float endu = knotsU[knotsU.size()-1] - epsilon;
	float endv = knotsU[knotsV.size()-1] - epsilon;
	float uinc = (endu-knotsU[0])/40.0f;
	float vinc = (endv-knotsV[0])/40.0f;

	float vnext;
	for(v = knotsV[0];; v = vnext){
		if(v > endv)
			v = endv;
		vnext = v + vinc;

		BeginIm3D(rw::PRIMTYPETRISTRIP);
		for(u = knotsU[0];; u += uinc){
			if(u > endu)
				u = endu;

			AddVertex(eval(u, v));
			AddVertex(eval(u, vnext));

			if(u >= endu)
				break;
		}
		EndIm3D();
		if(vnext >= endv)
			break;
	}
}


RBCurve testspline1, testspline2;
RBSurf testsurf;

V3d teapotVerts[] = {
	{  0.2000,  0.0000, 2.70000 }, {  0.2000, -0.1120, 2.70000 },
	{  0.1120, -0.2000, 2.70000 }, {  0.0000, -0.2000, 2.70000 },
	{  1.3375,  0.0000, 2.53125 }, {  1.3375, -0.7490, 2.53125 },
	{  0.7490, -1.3375, 2.53125 }, {  0.0000, -1.3375, 2.53125 },
	{  1.4375,  0.0000, 2.53125 }, {  1.4375, -0.8050, 2.53125 },
	{  0.8050, -1.4375, 2.53125 }, {  0.0000, -1.4375, 2.53125 },
	{  1.5000,  0.0000, 2.40000 }, {  1.5000, -0.8400, 2.40000 },
	{  0.8400, -1.5000, 2.40000 }, {  0.0000, -1.5000, 2.40000 },
	{  1.7500,  0.0000, 1.87500 }, {  1.7500, -0.9800, 1.87500 },
	{  0.9800, -1.7500, 1.87500 }, {  0.0000, -1.7500, 1.87500 },
	{  2.0000,  0.0000, 1.35000 }, {  2.0000, -1.1200, 1.35000 },
	{  1.1200, -2.0000, 1.35000 }, {  0.0000, -2.0000, 1.35000 },
	{  2.0000,  0.0000, 0.90000 }, {  2.0000, -1.1200, 0.90000 },
	{  1.1200, -2.0000, 0.90000 }, {  0.0000, -2.0000, 0.90000 },
	{ -2.0000,  0.0000, 0.90000 }, {  2.0000,  0.0000, 0.45000 },
	{  2.0000, -1.1200, 0.45000 }, {  1.1200, -2.0000, 0.45000 },
	{  0.0000, -2.0000, 0.45000 }, {  1.5000,  0.0000, 0.22500 },
	{  1.5000, -0.8400, 0.22500 }, {  0.8400, -1.5000, 0.22500 },
	{  0.0000, -1.5000, 0.22500 }, {  1.5000,  0.0000, 0.15000 },
	{  1.5000, -0.8400, 0.15000 }, {  0.8400, -1.5000, 0.15000 },
	{  0.0000, -1.5000, 0.15000 }, { -1.6000,  0.0000, 2.02500 },
	{ -1.6000, -0.3000, 2.02500 }, { -1.5000, -0.3000, 2.25000 },
	{ -1.5000,  0.0000, 2.25000 }, { -2.3000,  0.0000, 2.02500 },
	{ -2.3000, -0.3000, 2.02500 }, { -2.5000, -0.3000, 2.25000 },
	{ -2.5000,  0.0000, 2.25000 }, { -2.7000,  0.0000, 2.02500 },
	{ -2.7000, -0.3000, 2.02500 }, { -3.0000, -0.3000, 2.25000 },
	{ -3.0000,  0.0000, 2.25000 }, { -2.7000,  0.0000, 1.80000 },
	{ -2.7000, -0.3000, 1.80000 }, { -3.0000, -0.3000, 1.80000 },
	{ -3.0000,  0.0000, 1.80000 }, { -2.7000,  0.0000, 1.57500 },
	{ -2.7000, -0.3000, 1.57500 }, { -3.0000, -0.3000, 1.35000 },
	{ -3.0000,  0.0000, 1.35000 }, { -2.5000,  0.0000, 1.12500 },
	{ -2.5000, -0.3000, 1.12500 }, { -2.6500, -0.3000, 0.93750 },
	{ -2.6500,  0.0000, 0.93750 }, { -2.0000, -0.3000, 0.90000 },
	{ -1.9000, -0.3000, 0.60000 }, { -1.9000,  0.0000, 0.60000 },
	{  1.7000,  0.0000, 1.42500 }, {  1.7000, -0.6600, 1.42500 },
	{  1.7000, -0.6600, 0.60000 }, {  1.7000,  0.0000, 0.60000 },
	{  2.6000,  0.0000, 1.42500 }, {  2.6000, -0.6600, 1.42500 },
	{  3.1000, -0.6600, 0.82500 }, {  3.1000,  0.0000, 0.82500 },
	{  2.3000,  0.0000, 2.10000 }, {  2.3000, -0.2500, 2.10000 },
	{  2.4000, -0.2500, 2.02500 }, {  2.4000,  0.0000, 2.02500 },
	{  2.7000,  0.0000, 2.40000 }, {  2.7000, -0.2500, 2.40000 },
	{  3.3000, -0.2500, 2.40000 }, {  3.3000,  0.0000, 2.40000 },
	{  2.8000,  0.0000, 2.47500 }, {  2.8000, -0.2500, 2.47500 },
	{  3.5250, -0.2500, 2.49375 }, {  3.5250,  0.0000, 2.49375 },
	{  2.9000,  0.0000, 2.47500 }, {  2.9000, -0.1500, 2.47500 },
	{  3.4500, -0.1500, 2.51250 }, {  3.4500,  0.0000, 2.51250 },
	{  2.8000,  0.0000, 2.40000 }, {  2.8000, -0.1500, 2.40000 },
	{  3.2000, -0.1500, 2.40000 }, {  3.2000,  0.0000, 2.40000 },
	{  0.0000,  0.0000, 3.15000 }, {  0.8000,  0.0000, 3.15000 },
	{  0.8000, -0.4500, 3.15000 }, {  0.4500, -0.8000, 3.15000 },
	{  0.0000, -0.8000, 3.15000 }, {  0.0000,  0.0000, 2.85000 },
	{  1.4000,  0.0000, 2.40000 }, {  1.4000, -0.7840, 2.40000 },
	{  0.7840, -1.4000, 2.40000 }, {  0.0000, -1.4000, 2.40000 },
	{  0.4000,  0.0000, 2.55000 }, {  0.4000, -0.2240, 2.55000 },
	{  0.2240, -0.4000, 2.55000 }, {  0.0000, -0.4000, 2.55000 },
	{  1.3000,  0.0000, 2.55000 }, {  1.3000, -0.7280, 2.55000 },
	{  0.7280, -1.3000, 2.55000 }, {  0.0000, -1.3000, 2.55000 },
	{  1.3000,  0.0000, 2.40000 }, {  1.3000, -0.7280, 2.40000 },
	{  0.7280, -1.3000, 2.40000 }, {  0.0000, -1.3000, 2.40000 }
};
int teapotPatches[10][16] = {
	{ 118, 118, 118, 118, 124, 122, 119, 121,
	  123, 126, 125, 120,  40,  39,  38,  37 },
	{ 102, 103, 104, 105,   4,   5,   6,   7,
	    8,   9,  10,  11,  12,  13,  14,  15 },
	{  12,  13,  14,  15,  16,  17,  18,  19,
	   20,  21,  22,  23,  24,  25,  26,  27 },
	{  24,  25,  26,  27,  29,  30,  31,  32,
	   33,  34,  35,  36,  37,  38,  39,  40 },
	{  96,  96,  96,  96,  97,  98,  99, 100,
	  101, 101, 101, 101,   0,   1,   2,   3 },
	{   0,   1,   2,   3, 106, 107, 108, 109,
	  110, 111, 112, 113, 114, 115, 116, 117 },
	{  41,  42,  43,  44,  45,  46,  47,  48,
	   49,  50,  51,  52,  53,  54,  55,  56 },
	{  53,  54,  55,  56,  57,  58,  59,  60,
	   61,  62,  63,  64,  28,  65,  66,  67 },
	{  68,  69,  70,  71,  72,  73,  74,  75,
	   76,  77,  78,  79,  80,  81,  82,  83 },
	{  80,  81,  82,  83,  84,  85,  86,  87,
	   88,  89,  90,  91,  92,  93,  94,  95 },
};

BezierSurf teapotSurfs[32];

void
initsplines(void)
{
	V3d vert;

	testspline1.degree = 3;
	testspline1.verts.clear();
	testspline1.weights.clear();
	vert.set(-30.63383, 22.65459, 0);
	vert = scale(vert, 1.0f/20.0f);
	testspline1.verts.push_back(vert);
	testspline1.weights.push_back(1.0f);
	vert.set(13.50783, 33.01786, 15.06403);
	vert = scale(vert, 1.0f/20.0f);
	testspline1.verts.push_back(vert);
	testspline1.weights.push_back(1.0f);
	vert.set(34.252, -10.36327, 15.06403);
	vert = scale(vert, 1.0f/20.0f);
	testspline1.verts.push_back(vert);
	testspline1.weights.push_back(1.0f);
	vert.set(-7.959972, -1.205032, 0);
	vert = scale(vert, 1.0f/20.0f);
	testspline1.verts.push_back(vert);
	testspline1.weights.push_back(1.0f);
	vert.set(6.995127, -41.32158, -18.19684);
	vert = scale(vert, 1.0f/20.0f);
	testspline1.verts.push_back(vert);
	testspline1.weights.push_back(1.0f);

	testspline1.knots.clear();
	testspline1.knots.push_back(0);
	testspline1.knots.push_back(0);
	testspline1.knots.push_back(0);
	testspline1.knots.push_back(0);
	testspline1.knots.push_back(1);
	testspline1.knots.push_back(2);
	testspline1.knots.push_back(2);
	testspline1.knots.push_back(2);
	testspline1.knots.push_back(2);


	testspline2.degree = 2;
	testspline2.verts.clear();
#define V(x, y, z) \
	vert.set(x, y, z); \
	testspline2.verts.push_back(scale(vert, 1.0f/20.0f)); \
	testspline2.weights.push_back(1.0f);
	V(-61.9913, 9.158239, 0);
	V(-32.32231, 27.23371, 0);
	V(25.80961, -4.820126, 0);
#undef V
	testspline2.knots.clear();
	testspline2.knots.push_back(0);
	testspline2.knots.push_back(0);
	testspline2.knots.push_back(0);
	testspline2.knots.push_back(1);
	testspline2.knots.push_back(1);
	testspline2.knots.push_back(1);

/*
	testspline2 = testspline1;
//	testspline2.knots.clear();
//	testspline2.knots.push_back(0);
//	testspline2.knots.push_back(0);
//	testspline2.knots.push_back(0);
//	testspline2.knots.push_back(0);
//	testspline2.knots.push_back(3);
//	testspline2.knots.push_back(4);
//	testspline2.knots.push_back(4);
//	testspline2.knots.push_back(4);
//	testspline2.knots.push_back(4);
	testspline1.weights[2] = 5.0f;
*/

#define V(x, y, z) \
	vert.set(x, y, z); \
	testsurf.verts.push_back(scale(vert, 1.0f/20.0f)); \
	testsurf.weights.push_back(1.0f);

	testsurf.degreeU = 3;
	testsurf.degreeV = 3;
	testsurf.verts.clear();
	testsurf.weights.clear();
	V(-69.22764, -0, 12.77366);
	V(-48.72468, 0, 29.16251);
	V(-24.84476, 0, 39.52605);
	V(22.43265, -0, 45.79238);
	V(36.4229, 0, 28.9215);
	V(61.02645, 0, 6.74835);
	V(86.35364, -0, 20.96809);
	V(-69.22764, 9.286676, 12.77366);
	V(-48.72468, 9.286676, 29.16251);
	V(-24.84476, 9.286676, 39.52605);
	V(22.43265, 9.286676, 45.79238);
	V(36.4229, 9.286676, 28.9215);
	V(61.02645, 9.286676, 6.74835);
	V(86.35364, 9.286676, 20.96809);
	V(-68.13416, 23.51821, 6.114491);
	V(-48.19925, 23.51821, 22.27994);
	V(-26.91943, 23.51821, 33.20763);
	V(18.69658, 23.51821, 39.0488);
	V(27.88844, 23.51821, 30.80604);
	V(57.49908, 23.51821, -0.6488895);
	V(86.35364, 23.51821, 14.21974);
	V(-67.00163, 52.46369, -0.7825046);
	V(-47.65504, 52.46369, 15.15157);
	V(-29.06818, 52.46369, 26.66356);
	V(14.82709, 52.46369, 32.06438);
	V(19.04919, 52.46369, 32.75788);
	V(53.84574, 52.46369, -8.310311);
	V(86.35364, 52.46369, 7.230374);
	V(-67.86079, 70.07219, 4.449699);
	V(-48.06789, 70.07219, 20.5593);
	V(-27.43809, 70.07219, 31.62803);
	V(17.76257, 70.07219, 37.36291);
	V(25.75483, 70.07219, 31.27717);
	V(56.61724, 70.07219, -2.498198);
	V(86.35364, 70.07219, 12.53265);
#undef V
	testsurf.knotsU.clear();
	testsurf.knotsU.push_back(0);
	testsurf.knotsU.push_back(0);
	testsurf.knotsU.push_back(0);
	testsurf.knotsU.push_back(0);
	testsurf.knotsU.push_back(0.25);
	testsurf.knotsU.push_back(0.5);
	testsurf.knotsU.push_back(0.75);
	testsurf.knotsU.push_back(1);
	testsurf.knotsU.push_back(1);
	testsurf.knotsU.push_back(1);
	testsurf.knotsU.push_back(1);
	testsurf.knotsV.clear();
	testsurf.knotsV.push_back(0);
	testsurf.knotsV.push_back(0);
	testsurf.knotsV.push_back(0);
	testsurf.knotsV.push_back(0);
	testsurf.knotsV.push_back(0.5);
	testsurf.knotsV.push_back(1);
	testsurf.knotsV.push_back(1);
	testsurf.knotsV.push_back(1);
	testsurf.knotsV.push_back(1);

	testsurf.update();

	int i, j;
	for(i = 0; i < 10; i++)
		for(j = 0; j < 16; j++)
			teapotSurfs[i].verts[j] = teapotVerts[teapotPatches[i][j]];
	for(i = 0; i < 10; i++){
		teapotSurfs[i+10] = teapotSurfs[i];
		teapotSurfs[i+10].mirrorY();
	}
	for(i = 0; i < 6; i++){
		teapotSurfs[i+20] = teapotSurfs[i];
		teapotSurfs[i+20].mirrorX();
		teapotSurfs[i+20+6] = teapotSurfs[i+10];
		teapotSurfs[i+20+6].mirrorX();
	}
}

void
rendersplines(void)
{
//	testspline1.drawHull();
//	testspline1.drawSpline();

//	testspline2.drawHull();
//	testspline2.drawSpline();

//	testsurf.drawHull();
//	testsurf.drawShaded();
//	testsurf.drawIsoparms();

	int i;
	for(i = 0; i < 32; i++){
		teapotSurfs[i].drawHull();
		teapotSurfs[i].drawShaded();
	}
}

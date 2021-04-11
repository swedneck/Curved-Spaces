//  CurvedSpacesGPUFunctions.metal
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#include <metal_stdlib>
using namespace metal;
#include "CurvedSpacesGPUDefinitions.h"	//	Include this *after* "using namespace metal"
										//		if you might need to use uint32_t in "…GPUDefinitions.h"


//	Note on data types:  The talk WWDC 2016 session #606, whose transcript is available at
//
//		http://asciiwwdc.com/2016/sessions/606
//
//	says
//
//		the most important thing to remember when you're choosing data types
//		is that A8 and later GPUs have 16-bit register units, which means that
//		for example if you're using a 32-bit data type, that's twice the register space,
//		twice the bandwidth, potentially twice the power and so-forth,
//		it's just twice as much stuff.
//
//		So, accordingly you will save registers, you will get faster performance,
//		you'll get lower power by using smaller data types.
//
//		Use half and short for arithmetic wherever you can.
//
//		Energy wise, half is cheaper than float.
//
//		And float is cheaper than integer, but even among integers, smaller integers are cheaper than bigger ones.
//
//		And the most effective thing you can do to save registers is
//		to use half for texture reads and interpolates because most of the time
//		you really do not need float for these.
//		And note I do not mean your texture formats.
//		I mean the data types you're using to store the results of a texture sample or an interpolate.
//
//		And one aspect of A8 in later GPUs that is fairly convenient and
//		makes using smaller data types easier than on some other GPUs is
//		that data type conversions are typically free, even between float and half,
//		which means that you don't have to worry, oh am I introducing too many conversions
//		in this by trying to use half here?
//


//	Note:  In the Metal shading language, uint and a uint32_t are the same thing,
//	but for some reason a call like
//
//		[theGPUFunctionLibrary newFunctionWithName:@"Foo"
//								constantValues:theCompileTimeConstants error:&theError];
//
//	insists on uint, and fails if it finds a uint32_t here.
//
constant uint	gFogAndClipBoxType	[[ function_constant(0) ]];	//	32-bit unsigned integer
constant bool	gUseCubeMap			[[ function_constant(1) ]];	//	 8-bit boolean
constant bool	gUsePlainTexture = ! gUseCubeMap;
constant uint	gShapeOfSpaceFigure	[[ function_constant(2) ]];	//	32-bit unsigned integer


struct VertexInput
{
	float4	pos [[ attribute(VertexAttributePosition)	]];
	float3	tex [[ attribute(VertexAttributeTexCoords)	]];	//	(u,v,-) for regular texture or (u,v,w) for cube map
	half4	col [[ attribute(VertexAttributeColor)		]];
};

struct VertexOutput
{
	float4	position	[[ position			]];
	float3	texCoords	[[ user(texcoords)	]];	//	(u,v,-) for regular texture or (u,v,w) for cube map
	half4	color		[[ user(color)		]];
};
struct FragmentInput
{
	float3	texCoords	[[ user(texcoords)	]];	//	(u,v,-) for regular texture or (u,v,w) for cube map
	half4	color		[[ user(color)		]];
};


vertex VertexOutput CurvedSpacesVertexFunction(
	VertexInput							in				[[ stage_in							]],
	const device float4x4				*tilingGroup	[[ buffer(BufferIndexTilingGroup)	]],	//	includes view matrix (and antipodal map where appropriate)
	constant CurvedSpacesUniformData	&uniforms		[[ buffer(BufferIndexUniforms)		]],
//#warning restore ushort iid [[ instance_id ]]
//	uint								iid				[[ instance_id						]])
	ushort								iid				[[ instance_id						]])
{
	VertexOutput	out;
	float4			theTransformedPosition;
	half			theFogValue;

	//	position
	
	//	tilingGroup[iid] holds the pre-computed product of the tiling matrix and the view matrix
	//	(and also the antipodal map, in the case of ShaderSphericalFogBoxBack).
	theTransformedPosition = tilingGroup[iid] * in.pos;

	//	Typically we render to the full clipping box.
	//	The only exceptions are spherical spaces that lack antipodal symmetry
	//	(odd-order lens spaces and the 3-sphere itself), for which we render
	//	the  back hemisphere to the  back half of the clipping box and
	//	the front hemisphere to the front half of the clipping box.
	switch (gFogAndClipBoxType)
	{
		case ShaderSphericalFogBoxFull:
		case ShaderEuclideanFogBoxFull:
		case ShaderHyperbolicFogBoxFull:
		case ShaderNoFogBoxFull:
			out.position = uniforms.itsProjectionMatrixForBoxFull  * theTransformedPosition;
			break;

		case ShaderSphericalFogBoxFront:
		case ShaderNoFogBoxFront:
			out.position = uniforms.itsProjectionMatrixForBoxFront * theTransformedPosition;
			break;
		
		case ShaderSphericalFogBoxBack:
		case ShaderNoFogBoxBack:
			out.position = uniforms.itsProjectionMatrixForBoxBack  * theTransformedPosition;
			break;
	}

	//	texture coordinates
	out.texCoords = in.tex;

	//	color
	switch (gFogAndClipBoxType)
	{
		case ShaderSphericalFogBoxFull:
			//	Whether we use the true distance d or the coordinate w = cos(d)
			//	to compute the fog, the results come out nearly identical.
			//	So let's stick with w, to reduce the computational load.
			//
			//	Just for the record, here's the "true distance" version
			//	that we are *not* using:
			//
			//		#define PI_INVERSE	0.31830988618379067154h
			//
			//		tmpFogValue	= acos(clamp(half(theTransformedPosition.w), -1.0h, 1.0h)) * PI_INVERSE;
			//
			//	We're rendering only the front hemisphere,
			//	so we interpolate the fog from its "near" to "mid" values.
			theFogValue	= uniforms.itsSphFogSaturationNear * 0.5h * (1.0h + half(theTransformedPosition.w))
						+ uniforms.itsSphFogSaturationMid  * 0.5h * (1.0h - half(theTransformedPosition.w));
			break;

		case ShaderSphericalFogBoxFront:
			theFogValue	= uniforms.itsSphFogSaturationNear * 0.5h * (1.0h + half(theTransformedPosition.w))
						+ uniforms.itsSphFogSaturationMid  * 0.5h * (1.0h - half(theTransformedPosition.w));
			break;
		
		case ShaderSphericalFogBoxBack:
			//	tilingGroup[iid] includes the antipodal map, so at this point
			//	we're rendering scenery that's already been mapped
			//	from the back hemisphere into the front hemisphere.
			//	Of course we nevertheless interpolate the fog
			//	from its "mid" to "far" values.
			theFogValue	= uniforms.itsSphFogSaturationMid  * 0.5h * (1.0h + half(theTransformedPosition.w))
						+ uniforms.itsSphFogSaturationFar  * 0.5h * (1.0h - half(theTransformedPosition.w));
			break;

		case ShaderEuclideanFogBoxFull:
			//	Let the fog be proportional to ( r / max_r )²
			theFogValue = min(
							uniforms.itsEucFogInverseSquareSaturationDistance
								* dot(half3(theTransformedPosition.xyz), half3(theTransformedPosition.xyz)),
							1.0h);
			break;

		case ShaderHyperbolicFogBoxFull:
			//	Let the fog be proportional to
			//
			//		log(w) = log(cosh(d)) ≈ log(exp(d)/2) = d - log(2)
			//
			theFogValue	= uniforms.itsHypFogInverseLogCoshSaturationDistance
						* log(half(theTransformedPosition.w));
			break;

		case ShaderNoFogBoxFull:
		case ShaderNoFogBoxFront:
		case ShaderNoFogBoxBack:
			theFogValue	= 0.0h;
			break;
	}
	if (gShapeOfSpaceFigure == 70)	//	various figures in Chapter 7
	{
		half	s,
				t;
		half3	thePaleBlueColor	= half3(0.875h, 1.000h, 1.000h);

		//	Fog towards the pale blue background color.
		//	The interpolated value of out.color will get passed
		//	to the fragment function, where it will multiply
		//	a pure white dummy texture.
		t = theFogValue;
		s = 1.0h - t;
		out.color	= half4(
						s * in.col.rgb + t * thePaleBlueColor,
						in.col.a);
	}
	else
	if (gShapeOfSpaceFigure == 154)	//	figure 15.4
	{
		//	Fogging is disabled for the hyperbolic tiling
		//	in Figure 15.4, to show more repeating images more clearly.
		//	So this code should never get called.  Nevertheless
		//	let's provide a fallback value in case it's ever needed.
		//
		out.color = in.col;
	}
	else
	if (gShapeOfSpaceFigure == 163)	//	figure 16.3
	{
		//	Fogging is disabled for the hyperbolic tiling
		//	in Figure 16.3, to show more repeating images more clearly.
		//	So this code should never get called.  Nevertheless
		//	let's provide a fallback value in case it's ever needed.
		//
		out.color = in.col;
	}
	else
	if (gShapeOfSpaceFigure == 166)	//	figure 16.6
	{
		//	Pass in.col.rgb unmodified, but let the alpha channel
		//	tell the fragment function how to blend its computed color
		//	with the background color.
		//
		out.color = half4(in.col.rgb, theFogValue);
	}
	else
	{
		half	theFogCoef;
		
		//	Fog towards black.
		theFogCoef	= 1.0h - theFogValue;
		out.color	= half4(
						theFogCoef * in.col.rgb,
						in.col.a);
	}

	return out;
}

fragment half4 CurvedSpacesFragmentFunction(
	FragmentInput		in				[[ stage_in															]],
	texture2d<half>		plainTexture	[[ texture(TextureIndexPrimary), function_constant(gUsePlainTexture)]],
	texturecube<half>	cubeMapTexture	[[ texture(TextureIndexPrimary), function_constant(gUseCubeMap)		]],
	sampler				textureSampler	[[ sampler(SamplerIndexPrimary)										]])
{
	half4	tmpTexelColor,
			tmpFragmentColor;

	if (gUsePlainTexture)
	{
		tmpTexelColor = plainTexture.sample(textureSampler, in.texCoords.xy);
	}
	if (gUseCubeMap)
	{
		tmpTexelColor = cubeMapTexture.sample(textureSampler, in.texCoords.xyz);
	}

	if (gShapeOfSpaceFigure == 166)	//	figure 16.6
	{
		tmpFragmentColor = half4(
							max(in.color.rgb * tmpTexelColor.rgb,
								half3(	in.color.a * tmpTexelColor.a,	//	premultiplied alpha
										in.color.a * tmpTexelColor.a,
										in.color.a * tmpTexelColor.a)),
							tmpTexelColor.a);
	}
	else
	{
		tmpFragmentColor = in.color * tmpTexelColor;
	}

	return tmpFragmentColor;
}

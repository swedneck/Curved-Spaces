//	CurvedSpacesRenderer.m
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt


#import "CurvedSpacesRenderer.h"
#import "CurvedSpaces-Common.h"
#import "CurvedSpacesGPUDefinitions.h"
#import "GeometryGamesUtilities-Common.h"
#import "GeometryGamesUtilities-Mac-iOS.h"
#import "GeometryGamesFauxSimd.h"	//	Metal file would need explicit path
#import <MetalKit/MetalKit.h>


//	How big should the possible centerpieces be?
#ifdef SHAPE_OF_SPACE_CH_16
#define EARTH_RADIUS		0.20	//	Use larger-than-normal  Earth  in Seifert-Weber space.
#define GALAXY_RADIUS		0.17	//	Use smaller-than-normal galaxy in Poincaré dodecahedral space.
#else
#define EARTH_RADIUS		0.1
#define GALAXY_RADIUS		0.25
#endif

//	How fast should the galaxy, Earth and gyroscope spin?
//	Express their speeds as integer multiples of the default rotation speed.
//	The reason they're integer multiples is that itsRotationAngle
//	occasionally jumps by 2π.
#define EARTH_SPEED				2
#define GALAXY_SPEED			1
#define GYROSCOPE_SPEED			6

//	How big should the dart that represents the observer be?
//
//	Note:  In a 3-sphere, the image of the dart at the user's origin
//	and/or antipodal point will display correctly iff the clipping distance
//	is at most about DART_INNER_WIDTH/2.
//
#define DART_HALF_LENGTH	0.050
#define DART_OUTER_WIDTH	0.017
#define DART_INNER_WIDTH	0.004

//	What colors should the fletches be?
#define DART_COLOR_FLETCH_LEFT		PREMULTIPLY_RGBA(1.000, 0.250, 0.375, 1.0)	//	linear sRGB color coordinates
#define DART_COLOR_FLETCH_RIGHT		PREMULTIPLY_RGBA(0.250, 1.000, 0.375, 1.0)	//	linear sRGB color coordinates
#define DART_COLOR_FLETCH_BOTTOM	PREMULTIPLY_RGBA(0.250, 0.375, 1.000, 1.0)	//	linear sRGB color coordinates
#define DART_COLOR_FLETCH_TOP		PREMULTIPLY_RGBA(1.000, 1.000, 0.250, 1.0)	//	linear sRGB color coordinates
#define DART_COLOR_TAIL				PREMULTIPLY_RGBA(0.500, 0.500, 0.500, 1.0)	//	linear sRGB color coordinates

//	How many levels of detail should we support?
#define MAX_NUM_LOD_LEVELS	4


#ifdef START_OUTSIDE
//	When viewing the fundamental polyhedron from outside,
//	how far away should it sit?
#define EXTRINSIC_VIEWING_DISTANCE	0.75
#endif


typedef struct
{
	simd_float4		pos;	//	position (x,y,z,w)
	simd_float3		tex;	//	2D texture coordinates (u,v,-) or cube map texture coordinates (u,v,w)
	faux_simd_half4	col;	//	premultiplied (αR,αG,αB,α)
} CurvedSpacesVertexData;


typedef struct
{
	Matrix	itsViewMatrix;

	double	itsProjectionMatrixForBoxFull[4][4],
			itsProjectionMatrixForBoxFront[4][4],
			itsProjectionMatrixForBoxBack[4][4];

} ViewProjectionMatrixSet;


//	ARC can't handle object references within C structs,
//	so package up the following as Objective-C objects instead.

@interface TilingBufferSet : NSObject
{
@public
	//	How many visible cells does this TilingBufferSet hold?
	//
	//		The buffers may include empty space at the end,
	//		following the valid matrices, so we can't rely
	//		on the buffer sizes to tell us how many matrices are present.
	//
	unsigned int	itsNumPlainTiles,
					itsNumReflectedTiles,
					itsTotalNumTiles,
					itsNumInvertedTiles;

	//	For opaque content, we'll want to draw
	//	the plain images and the reflected images separately,
	//	to get the backface culling right.
	//
	//		On macOS (non-deferred rendering) it may be
	//			more efficient to draw the scene front-to-back
	//			to minimize overdraw.
	//		On iOS (deferred rendering) the drawing order
	//			shouldn't matter, because the renderer
	//			sorts all triangles before it draws anything.
	//
	//	These buffers contain only the visible tiles' matrices,
	//	already post-multiplied by the view matrix.
	//
	id<MTLBuffer>	itsPlainFrontToBackTilingBuffer,
					itsReflectedFrontToBackTilingBuffer;

	//	On iOS as well as macOS, it's essential that we draw
	//	all semi-transparent content in a single pass,
	//	back-to-front, to get the transparency right.
	//
	//	Curved Spaces' only semi-transparent content is the galaxy,
	//	which we draw with backface culling disabled.
	//	If we needed to draw semi-transparent surfaces
	//	with distinct front and back faces, then this approach
	//	wouldn't work.  Luckily we don't need to do that.
	//
	//	This buffer contains only the visible tiles' matrices,
	//	already post-multiplied by the view matrix.
	//
	id<MTLBuffer>	itsFullBackToFrontTilingBuffer;
	
	//	For spherical spaces with antipodal symmetry,
	//	we need render only the front hemisphere,
	//	because any scenery in the back hemisphere
	//	gets precisely eclipsed by its antipodal objects
	//	in the front hemisphere.
	//	But for spherical spaces without antipodal symmetry
	//	(for example odd-order lens spaces and the 3-sphere itself),
	//	we need to render the back and front hemispheres separately.
	//
	//		Note #1:  Performance isn't an issue for these
	//		small spherical symmetry groups, so to keep
	//		the code simple we copy the whole group into the buffer,
	//		with no culling or sorting whatsoever.
	//
	//		Note #2:  Partially transparent scenery might
	//		not get rendered correctly in the back hemisphere.
	//		To render it correctly, we'd need to create
	//		an array of pointers and sort them according
	//		to the distance from the observer to the inverted images.
	//		To avoid this issue, -encodeFullSphereWithEncoder:…
	//		doesn't render partially transparent scenery.
	//
	//	Each matrix in this buffer will be post-multiplied
	//	by the antipodal map as well as the view matrix.
	//
	id<MTLBuffer>	itsInvertedTilingBuffer;
}
@end
@implementation TilingBufferSet
@end

@interface Mesh : NSObject
{
@public
	unsigned int	itsNumFacets;
	id<MTLBuffer>	itsVertexBuffer,
					itsIndexBuffer;
	bool			itsCubeMapFlag;	//	true = cube map;  false = traditional texture
}
@end
@implementation Mesh
@end

@interface MeshSet : NSObject
{
@public
	//	itsMeshes[0] represents the finest level of detail
	//		and must always be present.
	//
	//	itsMeshes[1], itsMeshes[2], …
	//		are optional and represent successively coarser levels of detail.
	//
	//	Unused levels must be set to nil.
	//
	Mesh	*itsMeshes[MAX_NUM_LOD_LEVELS];
}
@end
@implementation MeshSet
@end



static id<MTLRenderPipelineState>	MakePipelineState(id<MTLDevice> aDevice, MTLPixelFormat aColorPixelFormat, id<MTLLibrary> aGPUFunctionLibrary,
										bool aMultisamplingFlag, ShaderFogAndClipBoxType aShaderFogAndClipBoxType, bool aCubeMapFlag, bool anAlphaBlendingFlag);
static TilingBufferSet				*MakeEmptyTilingBufferSet(void);
static MeshSet						*MakeEmptyMeshSet(void);
static Mesh							*MakeEmptyMesh(void);
static void							RefreshDirichletWalls(MeshSet *aMeshSet, id<MTLDevice> aDevice, ModelData *md);
static MeshSet						*MakeCenterpieceMeshSet(CenterpieceType aCenterpieceType, id<MTLDevice> aDevice);
static MeshSet						*MakeEarthMeshSet(id<MTLDevice> aDevice);
static MeshSet						*MakeGalaxyMeshSet(id<MTLDevice> aDevice);
static MeshSet						*MakeGyroscopeMeshSet(id<MTLDevice> aDevice);
#ifdef SHAPE_OF_SPACE_CH_7
static MeshSet						*MakeCubeMeshSet(id<MTLDevice> aDevice);
#endif
static MeshSet						*MakeVertexFigureMeshSet(id<MTLDevice> aDevice, ModelData *md);
static MeshSet						*MakeObserverMeshSet(id<MTLDevice> aDevice);
static Mesh							*MakeMeshFromArrays(id<MTLDevice> aDevice,
										unsigned int aNumMeshVertices, double (*someMeshVertexPositions)[4], double (*someMeshVertexTexCoords)[3], double (*someMeshVertexColors)[4],
										unsigned int aNumMeshFacets, unsigned int (*someMeshFacets)[3],
										bool aCubeMapFlag);
static unsigned int					GetNumLevelsOfDetail(MeshSet *aMeshSet);


//	Privately-declared methods
@interface CurvedSpacesRenderer()

- (void)   setUpPipelineStates;
- (void)shutDownPipelineStates;

- (void)   setUpDepthStencilState;
- (void)shutDownDepthStencilState;

- (void)   setUpFixedBuffersWithModelData:(ModelData *)md;
- (void)shutDownFixedBuffers;

- (void)   setUpInflightBuffers;
- (void)shutDownInflightBuffers;

- (void)   setUpTextures;
- (void)shutDownTextures;

- (void)   setUpSamplers;
- (void)shutDownSamplers;

- (ViewProjectionMatrixSet)makeViewProjectionMatrixSetForImageSize:(CGSize)anImageSize modelData:(ModelData *)md;
- (void)writeUniformsIntoBuffer:(id<MTLBuffer>)aUniformsBuffer forImageSize:(CGSize)anImageSize matrixSet:(ViewProjectionMatrixSet)aMatrixSet modelData:(ModelData *)md;
- (void)writeSortedVisibleTilesIntoBufferSet:(TilingBufferSet *)aTilingBufferSet forImageSize:(CGSize)anImageSize matrixSet:(ViewProjectionMatrixSet)aMatrixSet modelData:(ModelData *)md;

@end


@implementation CurvedSpacesRenderer
{
	//	Which centerpiece do itsUnrotatedCenterpieceMeshSet
	//	and itsCenterpieceTexture currently represent?
	CenterpieceType				itsCenterpieceType;

	id<MTLBuffer>				itsUniformBuffer[NUM_INFLIGHT_BUFFERS];
	TilingBufferSet				*itsTilingBufferSet[NUM_INFLIGHT_BUFFERS];
	MeshSet						*itsRotatedCenterpieceMeshSet[NUM_INFLIGHT_BUFFERS],	//	Earth, galaxy or gyroscope, with current rotation applied
								*itsTransformedObserverMeshSet[NUM_INFLIGHT_BUFFERS];
	
	id<MTLRenderPipelineState>	itsRenderPipelineStateSphericalFogBoxFull,
								itsRenderPipelineStateSphericalFogBoxFullCubeMap,
								itsRenderPipelineStateSphericalFogBoxFullBlended,
								itsRenderPipelineStateSphericalFogBoxFront,
								itsRenderPipelineStateSphericalFogBoxFrontCubeMap,
								itsRenderPipelineStateSphericalFogBoxBack,
								itsRenderPipelineStateSphericalFogBoxBackCubeMap,
								itsRenderPipelineStateEuclideanFogBoxFull,
								itsRenderPipelineStateEuclideanFogBoxFullCubeMap,
								itsRenderPipelineStateEuclideanFogBoxFullBlended,
								itsRenderPipelineStateHyperbolicFogBoxFull,
								itsRenderPipelineStateHyperbolicFogBoxFullCubeMap,
								itsRenderPipelineStateHyperbolicFogBoxFullBlended,
								itsRenderPipelineStateNoFogBoxFull,
								itsRenderPipelineStateNoFogBoxFullCubeMap,
								itsRenderPipelineStateNoFogBoxFullBlended,
								itsRenderPipelineStateNoFogBoxFront,
								itsRenderPipelineStateNoFogBoxFrontCubeMap,
								itsRenderPipelineStateNoFogBoxBack,
								itsRenderPipelineStateNoFogBoxBackCubeMap;
	id<MTLDepthStencilState>	itsDepthStencilState;

	//	itsDirichletWallsMeshSet and itsVertexFigureMeshSet
	//	get used directly by the GPU.
	//	By contrast, itsUnrotatedCenterpieceMeshSet and
	//	itsUntransformedObserverMeshSet serve only
	//	as the input data that gets transformed and written to
	//	itsRotatedCenterpieceMeshSet and itsTransformedObserverMeshSet
	//	once per frame.
	MeshSet						*itsDirichletWallsMeshSet,
								*itsUnrotatedCenterpieceMeshSet,
								*itsVertexFigureMeshSet,
								*itsUntransformedObserverMeshSet;

	id<MTLTexture>				itsWoodWallTexture,
								itsPaperWallTexture,
								itsCenterpieceTexture,	//	for Earth, galaxy or gyroscope
								itsStoneVertexFigureTexture,
								itsWhiteObserverTexture;

	id<MTLSamplerState>			itsAnisotropicTextureSampler;
}


- (id)initWithLayer:(CAMetalLayer *)aLayer device:(id<MTLDevice>)aDevice
	multisampling:(bool)aMultisamplingFlag depthBuffer:(bool)aDepthBufferFlag stencilBuffer:(bool)aStencilBufferFlag
{
	self = [super initWithLayer:aLayer device:aDevice
		multisampling:aMultisamplingFlag depthBuffer:aDepthBufferFlag stencilBuffer:aStencilBufferFlag
		mayExportWithTransparentBackground:false];
	if (self != nil)
	{
	}
	return self;
}


#pragma mark -
#pragma mark graphics setup/shutdown

- (void)setUpGraphicsWithModelData:(ModelData *)md
{
	//	This method gets called once when the view is first created, and then
	//	again whenever the app comes out of background and re-enters the foreground.
	//
	//		Technical note:  An added advantage of setting up the graphics here,
	//		instead of in initWithLayer:…, is that we avoid calling [self …]
	//		from within the initializer.  In the present implementation,
	//		calls to [self …] from the initializer would be harmless,
	//		but the risk is that if we ever wrote a subclass of this class,
	//		then such calls would be received by a not-yet-fully-initialized object.
	//
	
	[super setUpGraphicsWithModelData:md];
	
	itsCenterpieceType = md->itsCenterpieceType;

	[self setUpPipelineStates];
	[self setUpDepthStencilState];
	[self setUpFixedBuffersWithModelData:md];
	[self setUpInflightBuffers];
	[self setUpTextures];
	[self setUpSamplers];

	//	There's no urgent need to redraw the space immediately,
	//	but it seems like a good thing to do anyhow,
	//	so if there are ever any problems, they'll be visible immediately.
	//
	md->itsChangeCount++;
}

- (void)shutDownGraphicsWithModelData:(ModelData *)md
{
	//	This method gets called when the view
	//	leaves the foreground and goes into the background.
	//	There's no need to call it when the view gets deallocated,
	//	because at that point ARC automatically releases all
	//	the view's Metal objects.

	[self shutDownPipelineStates];
	[self shutDownDepthStencilState];
	[self shutDownFixedBuffers];
	[self shutDownInflightBuffers];
	[self shutDownTextures];
	[self shutDownSamplers];
	
	[super shutDownGraphicsWithModelData:md];
}

- (void)setUpPipelineStates
{
	id<MTLLibrary>	theGPUFunctionLibrary;

	theGPUFunctionLibrary = [itsDevice newDefaultLibrary];

	itsRenderPipelineStateSphericalFogBoxFull			= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderSphericalFogBoxFull,		false,	false);
	itsRenderPipelineStateSphericalFogBoxFullCubeMap	= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderSphericalFogBoxFull,		true,	false);
	itsRenderPipelineStateSphericalFogBoxFullBlended	= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderSphericalFogBoxFull,		false,	true );

	itsRenderPipelineStateSphericalFogBoxFront			= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderSphericalFogBoxFront,	false,	false);
	itsRenderPipelineStateSphericalFogBoxFrontCubeMap	= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderSphericalFogBoxFront,	true,	false);

	itsRenderPipelineStateSphericalFogBoxBack			= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderSphericalFogBoxBack,		false,	false);
	itsRenderPipelineStateSphericalFogBoxBackCubeMap	= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderSphericalFogBoxBack,		true,	false);

	itsRenderPipelineStateEuclideanFogBoxFull			= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderEuclideanFogBoxFull,		false,	false);
	itsRenderPipelineStateEuclideanFogBoxFullCubeMap	= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderEuclideanFogBoxFull,		true,	false);
	itsRenderPipelineStateEuclideanFogBoxFullBlended	= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderEuclideanFogBoxFull,		false,	true );

	itsRenderPipelineStateHyperbolicFogBoxFull			= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderHyperbolicFogBoxFull,	false,	false);
	itsRenderPipelineStateHyperbolicFogBoxFullCubeMap	= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderHyperbolicFogBoxFull,	true,	false);
	itsRenderPipelineStateHyperbolicFogBoxFullBlended	= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderHyperbolicFogBoxFull,	false,	true );

	itsRenderPipelineStateNoFogBoxFull					= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderNoFogBoxFull,			false,	false);
	itsRenderPipelineStateNoFogBoxFullCubeMap			= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderNoFogBoxFull,			true,	false);
	itsRenderPipelineStateNoFogBoxFullBlended			= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderNoFogBoxFull,			false,	true );

	itsRenderPipelineStateNoFogBoxFront					= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderNoFogBoxFront,			false,	false);
	itsRenderPipelineStateNoFogBoxFrontCubeMap			= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderNoFogBoxFront,			true,	false);

	itsRenderPipelineStateNoFogBoxBack					= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderNoFogBoxBack,			false,	false);
	itsRenderPipelineStateNoFogBoxBackCubeMap			= MakePipelineState(itsDevice, itsColorPixelFormat, theGPUFunctionLibrary, itsMultisamplingFlag, ShaderNoFogBoxBack,			true,	false);
}

- (void)shutDownPipelineStates
{
	itsRenderPipelineStateSphericalFogBoxFull			= nil;
	itsRenderPipelineStateSphericalFogBoxFullCubeMap	= nil;
	itsRenderPipelineStateSphericalFogBoxFullBlended	= nil;

	itsRenderPipelineStateSphericalFogBoxFront			= nil;
	itsRenderPipelineStateSphericalFogBoxFrontCubeMap	= nil;

	itsRenderPipelineStateSphericalFogBoxBack			= nil;
	itsRenderPipelineStateSphericalFogBoxBackCubeMap	= nil;

	itsRenderPipelineStateEuclideanFogBoxFull			= nil;
	itsRenderPipelineStateEuclideanFogBoxFullCubeMap	= nil;
	itsRenderPipelineStateEuclideanFogBoxFullBlended	= nil;

	itsRenderPipelineStateHyperbolicFogBoxFull			= nil;
	itsRenderPipelineStateHyperbolicFogBoxFullCubeMap	= nil;
	itsRenderPipelineStateHyperbolicFogBoxFullBlended	= nil;

	itsRenderPipelineStateNoFogBoxFull					= nil;
	itsRenderPipelineStateNoFogBoxFullCubeMap			= nil;
	itsRenderPipelineStateNoFogBoxFullBlended			= nil;

	itsRenderPipelineStateNoFogBoxFront					= nil;
	itsRenderPipelineStateNoFogBoxFrontCubeMap			= nil;

	itsRenderPipelineStateNoFogBoxBack					= nil;
	itsRenderPipelineStateNoFogBoxBackCubeMap			= nil;
}

- (void)setUpDepthStencilState
{
	MTLDepthStencilDescriptor	*theDepthStencilDescriptor;

	theDepthStencilDescriptor = [[MTLDepthStencilDescriptor alloc] init];
	[theDepthStencilDescriptor setDepthCompareFunction:MTLCompareFunctionLess];
	[theDepthStencilDescriptor setDepthWriteEnabled:YES];
	itsDepthStencilState = [itsDevice newDepthStencilStateWithDescriptor:theDepthStencilDescriptor];
}

- (void)shutDownDepthStencilState
{
	itsDepthStencilState = nil;
}

- (void)setUpFixedBuffersWithModelData:(ModelData *)md
{
	itsDirichletWallsMeshSet = MakeEmptyMeshSet();
	RefreshDirichletWalls(itsDirichletWallsMeshSet, itsDevice, md);
	
	itsUnrotatedCenterpieceMeshSet	= MakeCenterpieceMeshSet(itsCenterpieceType, itsDevice);
	itsVertexFigureMeshSet			= MakeVertexFigureMeshSet(itsDevice, md);
	itsUntransformedObserverMeshSet	= MakeObserverMeshSet(itsDevice);
}

- (void)shutDownFixedBuffers
{
	itsDirichletWallsMeshSet		= nil;
	itsUnrotatedCenterpieceMeshSet	= nil;
	itsVertexFigureMeshSet			= nil;
	itsUntransformedObserverMeshSet	= nil;
}

- (void)setUpInflightBuffers
{
	unsigned int	i;
	
	for (i = 0; i < NUM_INFLIGHT_BUFFERS; i++)
	{
		itsUniformBuffer[i]					= [itsDevice newBufferWithLength:sizeof(CurvedSpacesUniformData) options:MTLResourceStorageModeShared];
		itsTilingBufferSet[i]				= MakeEmptyTilingBufferSet();
		itsRotatedCenterpieceMeshSet[i]		= MakeEmptyMeshSet();
		itsTransformedObserverMeshSet[i]	= MakeEmptyMeshSet();
	}
}

- (void)shutDownInflightBuffers
{
	unsigned int	i;
	
	for (i = 0; i < NUM_INFLIGHT_BUFFERS; i++)
	{
		itsUniformBuffer[i]					= nil;
		itsTilingBufferSet[i]				= nil;
		itsRotatedCenterpieceMeshSet[i]		= nil;
		itsTransformedObserverMeshSet[i]	= nil;
	}
}

- (void)setUpTextures
{
	MTKTextureLoader							*theTextureLoader;
	NSDictionary<MTKTextureLoaderOption, id>	*theTextureLoaderOptions;
	NSError										*theError = nil;
	
	theTextureLoader		= [[MTKTextureLoader alloc] initWithDevice:itsDevice];
	theTextureLoaderOptions	=
	@{
		MTKTextureLoaderOptionTextureUsage			:	@(MTLTextureUsageShaderRead),
		MTKTextureLoaderOptionTextureStorageMode	:	@(MTLStorageModePrivate),
		MTKTextureLoaderOptionAllocateMipmaps		:	@(YES),
		MTKTextureLoaderOptionGenerateMipmaps		:	@(YES)	//	-newTextureWithName:… ignores this option
																//		(as confirmed in its documentation).
																//	Instead, the Asset Catalog provides the mipmaps.
	};

#if   (SHAPE_OF_SPACE_CH_16 == 3)
	itsWoodWallTexture			= [self makeRGBATextureOfColor:(ColorP3Linear){0.00, 1.00, 0.50, 1.00} size:1];
									//	The 3rd edition of The Shape of Space used a texture
									//	whose gamma-encode color values were sRGB(0.00, 1.00, 0.75).
									//	If we ever wanted to match that shade exactly, we could
									//	easily convert that color to Linear Display P3.
									//	For now, the more saturated color Linear P3(0.0, 1.0, 0.5)
									//	looks OK, and in any case, the final figure in the book
									//	would have been adapted to whatever color gamut
									//	the printer supports.
#elif (SHAPE_OF_SPACE_CH_16 == 6)
	itsWoodWallTexture			= [self makeRGBATextureOfColor:(ColorP3Linear){0.25, 0.25, 1.00, 1.00} size:1];
									//	The 3rd edition of The Shape of Space used a texture
									//	whose gamma-encode color values were sRGB(0.50, 0.50, 1.00).
									//	See comment immediately above for further discussion.
#else
	itsWoodWallTexture			= [theTextureLoader
									newTextureWithName:@"wood"
									scaleFactor:1.0
									bundle:nil
									options:theTextureLoaderOptions
									error:&theError];
#endif

	itsPaperWallTexture			= [theTextureLoader
									newTextureWithName:@"paper"
									scaleFactor:1.0
									bundle:nil
									options:theTextureLoaderOptions
									error:&theError];

	itsCenterpieceTexture		= [self loadTextureForCenterpiece:	itsCenterpieceType
											textureLoader:			theTextureLoader
											textureLoaderOptions:	theTextureLoaderOptions
											error:					&theError];

	itsStoneVertexFigureTexture	= [theTextureLoader
									newTextureWithName:@"stone"
									scaleFactor:1.0
									bundle:nil
									options:theTextureLoaderOptions
									error:&theError];

	itsWhiteObserverTexture		= [self makeRGBATextureOfColor:(ColorP3Linear){1.0, 1.0, 1.0, 1.0} size:1];
}

- (void)shutDownTextures
{
	itsWoodWallTexture			= nil;
	itsPaperWallTexture			= nil;
	itsCenterpieceTexture		= nil;
	itsStoneVertexFigureTexture	= nil;
	itsWhiteObserverTexture		= nil;
}

- (void)setUpSamplers
{
	MTLSamplerDescriptor	*theDescriptor;
	
	theDescriptor = [[MTLSamplerDescriptor alloc] init];

	[theDescriptor setNormalizedCoordinates:YES];

	[theDescriptor setSAddressMode:MTLSamplerAddressModeRepeat];
	[theDescriptor setTAddressMode:MTLSamplerAddressModeRepeat];

	[theDescriptor setMinFilter:MTLSamplerMinMagFilterLinear];
	[theDescriptor setMagFilter:MTLSamplerMinMagFilterLinear];
	[theDescriptor setMipFilter:MTLSamplerMipFilterLinear];
	
	[theDescriptor setMaxAnisotropy:16];
	
	itsAnisotropicTextureSampler = [itsDevice newSamplerStateWithDescriptor:theDescriptor];
}

- (void)shutDownSamplers
{
	itsAnisotropicTextureSampler = nil;
}


#pragma mark -
#pragma mark render

- (NSDictionary<NSString *, id> *)prepareInflightDataBuffersAtIndex:(unsigned int)anInflightBufferIndex modelData:(ModelData *)md
{
	NSMutableDictionary<NSString *, id>	*theDictionary;
	ViewProjectionMatrixSet				theViewProjectionMatrixSet;

	theDictionary = [[NSMutableDictionary<NSString *, id> alloc] initWithCapacity:2];

	theViewProjectionMatrixSet = [self makeViewProjectionMatrixSetForImageSize:itsOnscreenNativeSizePx modelData:md];
	
	[self writeUniformsIntoBuffer:	itsUniformBuffer[anInflightBufferIndex]
					forImageSize:	itsOnscreenNativeSizePx
					matrixSet:		theViewProjectionMatrixSet
					modelData:		md];
	[theDictionary setValue:	itsUniformBuffer[anInflightBufferIndex]
					 forKey:	@"uniform buffer"];

	[self writeSortedVisibleTilesIntoBufferSet:	itsTilingBufferSet[anInflightBufferIndex]
								forImageSize:	itsOnscreenNativeSizePx
								matrixSet:		theViewProjectionMatrixSet
								modelData:		md];
	[theDictionary setValue:	itsTilingBufferSet[anInflightBufferIndex]
					 forKey:	@"tiling buffer set"];

	[self updateMeshesAndTexturesAsNeededUsingModelData:md];
	
	[self writeRotatedCenterpieceIntoMeshSet:itsRotatedCenterpieceMeshSet[anInflightBufferIndex] modelData:md];
	[theDictionary setValue:	itsRotatedCenterpieceMeshSet[anInflightBufferIndex]
					 forKey:	@"rotated centerpiece mesh set"];
	
	[self writeTransformedObserverIntoMeshSet:itsTransformedObserverMeshSet[anInflightBufferIndex] modelData:md];
	[theDictionary setValue:	itsTransformedObserverMeshSet[anInflightBufferIndex]
					 forKey:	@"transformed observer mesh set"];

	return theDictionary;
}

- (NSDictionary<NSString *, id> *)prepareInflightDataBuffersForOffscreenRenderingAtSize:(CGSize)anImageSize modelData:(ModelData *)md
{
	NSMutableDictionary<NSString *, id>	*theDictionary;
	ViewProjectionMatrixSet				theViewProjectionMatrixSet;
	id<MTLBuffer>						theOneShotUniformBuffer;
	TilingBufferSet						*theOneShotTilingBufferSet;
	MeshSet								*theOneShotCenterpieceMeshSet,
										*theOneShotObserverMeshSet;
	
	theDictionary = [[NSMutableDictionary<NSString *, id> alloc] initWithCapacity:2];

	theViewProjectionMatrixSet = [self makeViewProjectionMatrixSetForImageSize:anImageSize modelData:md];
	
	theOneShotUniformBuffer	= [itsDevice newBufferWithLength:sizeof(CurvedSpacesUniformData) options:MTLResourceStorageModeShared];
	[self writeUniformsIntoBuffer:	theOneShotUniformBuffer
					forImageSize:	anImageSize
					matrixSet:		theViewProjectionMatrixSet
					modelData:		md];
	[theDictionary setValue:	theOneShotUniformBuffer
					 forKey:	@"uniform buffer"];

	theOneShotTilingBufferSet = MakeEmptyTilingBufferSet();
	[self writeSortedVisibleTilesIntoBufferSet:	theOneShotTilingBufferSet
								forImageSize:	anImageSize
								matrixSet:		theViewProjectionMatrixSet
								modelData:		md];
	[theDictionary setValue:	theOneShotTilingBufferSet
					 forKey:	@"tiling buffer set"];

	[self updateMeshesAndTexturesAsNeededUsingModelData:md];

	theOneShotCenterpieceMeshSet = MakeEmptyMeshSet();
	[self writeRotatedCenterpieceIntoMeshSet:theOneShotCenterpieceMeshSet modelData:md];
	[theDictionary setValue:	theOneShotCenterpieceMeshSet
					 forKey:	@"rotated centerpiece mesh set"];
	
	theOneShotObserverMeshSet = MakeEmptyMeshSet();
	[self writeTransformedObserverIntoMeshSet:theOneShotObserverMeshSet modelData:md];
	[theDictionary setValue:	theOneShotObserverMeshSet
					 forKey:	@"transformed observer mesh set"];

	return theDictionary;
}

- (ViewProjectionMatrixSet)makeViewProjectionMatrixSetForImageSize:(CGSize)anImageSize modelData:(ModelData *)md
{
	ViewProjectionMatrixSet	theViewProjectionMatrixSet;

	//	view matrix
	MatrixGeometricInverse(&md->itsUserBodyPlacement, &theViewProjectionMatrixSet.itsViewMatrix);

	//	projection matrices

	//	The BoxFull projection matrix is always needed.
	//	In the (somewhat unusual) case of SpaceSpherical with itsDrawBackHemisphere,
	//	the BoxFull matrix is used only for culling.
	MakeProjectionMatrix(anImageSize.width, anImageSize.height, md->itsSpaceType, BoxFull, theViewProjectionMatrixSet.itsProjectionMatrixForBoxFull);

	//	The BoxFront and BoxBack projection matrices
	//	are needed only for SpaceSpherical with itsDrawBackHemisphere.
	if (md->itsSpaceType == SpaceSpherical
	 && md->itsDrawBackHemisphere)
	{
		MakeProjectionMatrix(anImageSize.width, anImageSize.height, md->itsSpaceType, BoxFront, theViewProjectionMatrixSet.itsProjectionMatrixForBoxFront);
		MakeProjectionMatrix(anImageSize.width, anImageSize.height, md->itsSpaceType, BoxBack,  theViewProjectionMatrixSet.itsProjectionMatrixForBoxBack );
	}
	else
	{
		Matrix44Identity(theViewProjectionMatrixSet.itsProjectionMatrixForBoxFront);
		Matrix44Identity(theViewProjectionMatrixSet.itsProjectionMatrixForBoxBack );
	}
	
	return theViewProjectionMatrixSet;
}

- (void)writeUniformsIntoBuffer:	(id<MTLBuffer>)aUniformsBuffer
					forImageSize:	(CGSize)anImageSize
					matrixSet:		(ViewProjectionMatrixSet)aMatrixSet
					modelData:		(ModelData *)md
{
	CurvedSpacesUniformData	*theUniformData;


	GEOMETRY_GAMES_ASSERT(
		[aUniformsBuffer length] == sizeof(CurvedSpacesUniformData),
		"internal error:  aUniformsBuffer has wrong size");
	
	theUniformData = (CurvedSpacesUniformData *) [aUniformsBuffer contents];

	theUniformData->itsProjectionMatrixForBoxFull	= ConvertMatrix44ToSIMD(aMatrixSet.itsProjectionMatrixForBoxFull );
	theUniformData->itsProjectionMatrixForBoxFront	= ConvertMatrix44ToSIMD(aMatrixSet.itsProjectionMatrixForBoxFront);
	theUniformData->itsProjectionMatrixForBoxBack	= ConvertMatrix44ToSIMD(aMatrixSet.itsProjectionMatrixForBoxBack );

	switch (md->itsSpaceType)
	{
		case SpaceSpherical:

			theUniformData->itsSphFogSaturationNear	= 0.000;	//	at distance  0
			theUniformData->itsSphFogSaturationMid	= 0.750;	//	at distance  π
			theUniformData->itsSphFogSaturationFar	= 0.875;	//	at distance 2π

			break;
		
		case SpaceFlat:

			//	Fog is proportional to d².
			//	Rather than passing that saturation distance directly,
			//	and thus forcing the vertex shader to compute an inverse square,
			//	pass the saturation distance in a pre-digested form.
			//	Better to compute the inverse square once and for all here,
			//	rather than over and over in the shader, once for each vertex.
			//
			theUniformData->itsEucFogInverseSquareSaturationDistance = 1.0 / (md->itsHorizonRadius * md->itsHorizonRadius);

			break;
		
		case SpaceHyperbolic:

			//	Fog is proportional to log(w) = log(cosh(d)).
			//	Rather than passing that saturation distance directly,
			//	and thus forcing the vertex shader to compute an inverse log cosh,
			//	pass the saturation distance in a pre-digested form.
			//	Better to compute the inverse log cosh once and for all here,
			//	rather than over and over in the shader, once for each vertex.
			//
			theUniformData->itsHypFogInverseLogCoshSaturationDistance =
				//	Typically we're tiling deep enough
				//	that we don't need fog to suppress flickering
				//	(the beams are so thin and the Earth so small that
				//	they "self-suppress" at large distances).
				//	We do still need some fog for a depth cue, though,
				//	so try half-strength fog.
//				1.0 / log(cosh(md->itsHorizonRadius));
				0.5 / log(cosh(md->itsHorizonRadius));

			break;
		
		case SpaceNone:
			break;
	}
}

- (void)writeSortedVisibleTilesIntoBufferSet:	(TilingBufferSet *)aTilingBufferSet
								forImageSize:	(CGSize)anImageSize
								matrixSet:		(ViewProjectionMatrixSet)aMatrixSet
								modelData:		(ModelData *)md
{
	unsigned int	theRequiredPlainBufferLengthInBytes,
					theRequiredReflectedBufferLengthInBytes,
					theRequiredFullBufferLengthInBytes,
					theRequiredInvertedBufferLengthInBytes;
	simd_float4x4	*thePlainBufferData,
					*theReflectedBufferData,
					*theFullBufferData,
					*theInvertedBufferData;
#ifdef START_OUTSIDE
	Matrix			theTranslation;
#endif
	Honeycell		**theHoneyCellPtr;
	simd_float4x4	*thePlainMatrix,
					*theReflectedMatrix,
					*theFullMatrix;
	unsigned int	i;
	Matrix			theTileViewMatrix;
	simd_float4x4	theTileViewMatrixAsSIMD;
	Matrix			theAntipodalMap;
	Honeycell		*theHoneyCell;
	simd_float4x4	*theInvertedMatrix;
	Matrix			theInvertedTileMatrix,
					theInvertedTileViewMatrix;


	//	If no honeycomb is present, release all buffers and return.
	if (md->itsHoneycomb == NULL)
	{
		aTilingBufferSet->itsNumPlainTiles		= 0;
		aTilingBufferSet->itsNumReflectedTiles	= 0;
		aTilingBufferSet->itsTotalNumTiles		= 0;
		aTilingBufferSet->itsNumInvertedTiles	= 0;

		aTilingBufferSet->itsPlainFrontToBackTilingBuffer		= nil;
		aTilingBufferSet->itsReflectedFrontToBackTilingBuffer	= nil;
		aTilingBufferSet->itsFullBackToFrontTilingBuffer		= nil;
		aTilingBufferSet->itsInvertedTilingBuffer				= nil;

		return;
	}


	//	Determine which cells are visible relative
	//	to the current view and projection matrices,
	//	and sort them in order of increasing distance from the observer
	//	(so transparency effects come out right, and also
	//	so level-of-detail gets applied correctly).
	//	CullAndSortVisibleCells() also counts
	//	how many cells will be "plain" (not mirror-reversed) and
	//	how many cells will be "reflected" (mirror-reversed)
	//	after factoring in the parity of the current view matrix.
	//
#ifdef START_OUTSIDE
	if (md->itsViewpoint != ViewpointIntrinsic)
		SelectFirstCellOnly(md->itsHoneycomb);
	else
#endif
	CullAndSortVisibleCells(md->itsHoneycomb,
							&aMatrixSet.itsViewMatrix,
							anImageSize.width,
							anImageSize.height,
							md->itsHorizonRadius,
							DirichletDomainOutradius(md->itsDirichletDomain),
							md->itsSpaceType);

	//	There should always be a cell containing the origin,
	//	and it should be visible.  Nevertheless the code below
	//	allows for the case that no cells are visible, in which case
	//	it either lets [itsDevice -newBufferWithLength:0 options:…] return nil,
	//	or it writes 0 matrices into a pre-existing buffer.
	aTilingBufferSet->itsNumPlainTiles		= md->itsHoneycomb->itsNumVisiblePlainCells;
	aTilingBufferSet->itsNumReflectedTiles	= md->itsHoneycomb->itsNumVisibleReflectedCells;
	aTilingBufferSet->itsTotalNumTiles		= md->itsHoneycomb->itsNumVisibleCells;
	aTilingBufferSet->itsNumInvertedTiles	= (md->itsDrawBackHemisphere ? md->itsHoneycomb->itsNumCells : 0);

	//	Make sure each buffer is big enough to hold the required number of matrices.
	//	If it isn't, replace it with a buffer that's 125% the new required size.
	//	The reason for the factor of 125% is to avoid replacing a buffer over and over,
	//	adding only one or two more matrices each time.
	//
	//		Note #1:
	//			[itsDevice newBufferWithLength:0 options:…] refuses to create
	//			a buffer of zero length, and instead flags an error.
	//
	//		Note #2:
	//			[nil length] returns 0 (and is officially documented to do so),
	//			so the following code works as intended.
	//

	theRequiredPlainBufferLengthInBytes		= aTilingBufferSet->itsNumPlainTiles     * sizeof(simd_float4x4);
	theRequiredReflectedBufferLengthInBytes	= aTilingBufferSet->itsNumReflectedTiles * sizeof(simd_float4x4);
	theRequiredFullBufferLengthInBytes		= aTilingBufferSet->itsTotalNumTiles     * sizeof(simd_float4x4);
	theRequiredInvertedBufferLengthInBytes	= aTilingBufferSet->itsNumInvertedTiles  * sizeof(simd_float4x4);

	if ([aTilingBufferSet->itsPlainFrontToBackTilingBuffer length] < theRequiredPlainBufferLengthInBytes)
	{
		aTilingBufferSet->itsPlainFrontToBackTilingBuffer = [itsDevice
			newBufferWithLength:	(unsigned int)(1.25 * theRequiredPlainBufferLengthInBytes)
			options:				MTLResourceStorageModeShared];
	}

	if ([aTilingBufferSet->itsReflectedFrontToBackTilingBuffer length] < theRequiredReflectedBufferLengthInBytes)
	{
		aTilingBufferSet->itsReflectedFrontToBackTilingBuffer = [itsDevice
			newBufferWithLength:	(unsigned int)(1.25 * theRequiredReflectedBufferLengthInBytes)
			options:				MTLResourceStorageModeShared];
	}

	if ([aTilingBufferSet->itsFullBackToFrontTilingBuffer length] < theRequiredFullBufferLengthInBytes)
	{
		aTilingBufferSet->itsFullBackToFrontTilingBuffer = [itsDevice
			newBufferWithLength:	(unsigned int)(1.25 * theRequiredFullBufferLengthInBytes)
			options:				MTLResourceStorageModeShared];
	}

	if ([aTilingBufferSet->itsInvertedTilingBuffer length] < theRequiredInvertedBufferLengthInBytes)
	{
		aTilingBufferSet->itsInvertedTilingBuffer = [itsDevice
			newBufferWithLength:	(unsigned int)(1.25 * theRequiredInvertedBufferLengthInBytes)
			options:				MTLResourceStorageModeShared];
	}

	thePlainBufferData		= (simd_float4x4 *) [aTilingBufferSet->itsPlainFrontToBackTilingBuffer     contents];
	theReflectedBufferData	= (simd_float4x4 *) [aTilingBufferSet->itsReflectedFrontToBackTilingBuffer contents];
	theFullBufferData		= (simd_float4x4 *) [aTilingBufferSet->itsFullBackToFrontTilingBuffer      contents];
	theInvertedBufferData	= (simd_float4x4 *) [aTilingBufferSet->itsInvertedTilingBuffer             contents];

#ifdef START_OUTSIDE
	if (md->itsViewpoint != ViewpointIntrinsic)
	{
		MatrixTranslation(	&theTranslation,
							md->itsSpaceType,
							0.0,
							0.0,
							md->itsViewpointTransition * EXTRINSIC_VIEWING_DISTANCE);
	}
#endif

	//	Copy the visible tiles' tiling matrices into the various buffers.
	theHoneyCellPtr		= md->itsHoneycomb->itsVisibleCells;						//	source
	thePlainMatrix		= thePlainBufferData;										//	destination
	theReflectedMatrix	= theReflectedBufferData;									//	destination
	theFullMatrix		= theFullBufferData + md->itsHoneycomb->itsNumVisibleCells;	//	destination, but writing in reverse order
	for (i = 0; i < md->itsHoneycomb->itsNumVisibleCells; i++)
	{
		//	Compose each tiling matrix with the view matrix now,
		//	so the vertex shader doesn't have to multiply
		//	by the view matrix over and over, for each vertex.
		MatrixProduct(	&(*theHoneyCellPtr)->itsMatrix,
						&aMatrixSet.itsViewMatrix,
						&theTileViewMatrix);
#ifdef START_OUTSIDE
		if (md->itsViewpoint != ViewpointIntrinsic)
			MatrixProduct(&theTileViewMatrix, &theTranslation, &theTileViewMatrix);
#endif
		theTileViewMatrixAsSIMD = ConvertMatrix44ToSIMD(theTileViewMatrix.m);

		//	Write the tile-view matrix to either the plain buffer or the reflected buffer, as appropriate.
		if (theTileViewMatrix.itsParity == ImagePositive)
		{
			GEOMETRY_GAMES_ASSERT(thePlainMatrix != NULL,     "'impossible' NULL pointer (thePlainMatrix)");		//	suppress static analyzer warning
			*thePlainMatrix++		= theTileViewMatrixAsSIMD;
		}
		else
		{
			GEOMETRY_GAMES_ASSERT(theReflectedMatrix != NULL, "'impossible' NULL pointer (theReflectedMatrix)");	//	suppress static analyzer warning
			*theReflectedMatrix++	= theTileViewMatrixAsSIMD;
		}

		//	Write the same matrix to the full back-to-front buffer as well.
		GEOMETRY_GAMES_ASSERT(theFullMatrix != NULL, "'impossible' NULL pointer (theFullMatrix)");	//	suppress static analyzer warning
		*--theFullMatrix = theTileViewMatrixAsSIMD;
		
		//	Advance to the next HoneyCell pointer.
		theHoneyCellPtr++;
	}
	GEOMETRY_GAMES_ASSERT(
			thePlainMatrix		== thePlainBufferData     + aTilingBufferSet->itsNumPlainTiles
		 && theReflectedMatrix	== theReflectedBufferData + aTilingBufferSet->itsNumReflectedTiles
		 && theFullMatrix		== theFullBufferData      + 0,
		"Wrote unexpected number of visible-tile matrices in -writeSortedVisibleTilesIntoBufferSet:...");
	
	//	If the space is an odd-order lens space or the 3-sphere itself,
	//	then compose the matrices for back-hemisphere rendering with the antipodal and view matrices,
	//	and write them into the inverted-matrix buffer.
	//
	//		Warning:  This code does not culling or sorting.
	//		For these small symmetry groups, the simplicity of the code
	//		is more important than the run-time performance.
	//		The main drawback is that partially transparent content
	//		may get rendered incorrectly.
	//
	if (md->itsDrawBackHemisphere)
	{
		MatrixAntipodalMap(&theAntipodalMap);
		
		theHoneyCell 		= md->itsHoneycomb->itsCells;	//	source;  all the cells, not just itsVisibleCells
		theInvertedMatrix	= theInvertedBufferData;		//	destination
		for (i = 0; i < md->itsHoneycomb->itsNumCells; i++)
		{
			MatrixProduct(	&theHoneyCell->itsMatrix,
							&theAntipodalMap,
							&theInvertedTileMatrix);

			MatrixProduct(	&theInvertedTileMatrix,
							&aMatrixSet.itsViewMatrix,
							&theInvertedTileViewMatrix);

			*theInvertedMatrix++ = ConvertMatrix44ToSIMD(theInvertedTileViewMatrix.m);
			
			theHoneyCell++;
		}
		GEOMETRY_GAMES_ASSERT(
			theInvertedMatrix == theInvertedBufferData + aTilingBufferSet->itsNumInvertedTiles,
			"Wrote unexpected number of inverted back-hemisphere matrices in -writeSortedVisibleTilesIntoBufferSet:...");
	}
}


- (void)updateMeshesAndTexturesAsNeededUsingModelData:(ModelData *)md
{
	MTKTextureLoader							*theTextureLoader;
	NSDictionary<MTKTextureLoaderOption, id>	*theTextureLoaderOptions;
	NSError										*theError = nil;

	if (md->itsDirichletWallsMeshNeedsRefresh)
	{
		RefreshDirichletWalls(itsDirichletWallsMeshSet, itsDevice, md);
		md->itsDirichletWallsMeshNeedsRefresh = false;
	}

	if (itsCenterpieceType != md->itsCenterpieceType)
	{
		theTextureLoader		= [[MTKTextureLoader alloc] initWithDevice:itsDevice];
		theTextureLoaderOptions	=
		@{
			MTKTextureLoaderOptionTextureUsage			:	@(MTLTextureUsageShaderRead),
			MTKTextureLoaderOptionTextureStorageMode	:	@(MTLStorageModePrivate),
			MTKTextureLoaderOptionAllocateMipmaps		:	@(YES),
			MTKTextureLoaderOptionGenerateMipmaps		:	@(YES)	//	-newTextureWithName:… ignores this option
																	//		(as confirmed in its documentation).
																	//	Instead, the Asset Catalog provides the mipmaps.
		};

		itsCenterpieceType				= md->itsCenterpieceType;
		itsUnrotatedCenterpieceMeshSet	= MakeCenterpieceMeshSet(md->itsCenterpieceType, itsDevice);
		itsCenterpieceTexture			= [self
											loadTextureForCenterpiece:	md->itsCenterpieceType
											textureLoader:				theTextureLoader
											textureLoaderOptions:		theTextureLoaderOptions
											error:						&theError];
	}

	if (md->itsVertexFigureMeshNeedsReplacement)
	{
		itsVertexFigureMeshSet = MakeVertexFigureMeshSet(itsDevice, md);
		md->itsVertexFigureMeshNeedsReplacement = false;
	}
}

- (id<MTLTexture>)loadTextureForCenterpiece:(CenterpieceType)aCenterpieceType
	textureLoader:aTextureLoader textureLoaderOptions:someTextureLoaderOptions error:(NSError **)anError
{
	switch (aCenterpieceType)
	{
		case CenterpieceNone:		return nil;

		case CenterpieceEarth:		return [aTextureLoader
										newTextureWithName:@"blue marble"
										scaleFactor:1.0
										bundle:nil
										options:someTextureLoaderOptions
										error:anError];

		case CenterpieceGalaxy:		return [aTextureLoader
										newTextureWithName:@"galaxy"
										scaleFactor:1.0
										bundle:nil
										options:someTextureLoaderOptions
										error:anError];

		case CenterpieceGyroscope:	return [aTextureLoader
										newTextureWithName:@"gyroscope"
										scaleFactor:1.0
										bundle:nil
										options:someTextureLoaderOptions
										error:anError];

#ifdef SHAPE_OF_SPACE_CH_7
		case CenterpieceCube:		return [self makeRGBATextureOfColor:(ColorP3Linear){1.0, 1.0, 1.0, 1.0} size:1 roughingFactor:0.0];
#endif
	}
}


- (bool)wantsClearWithModelData:(ModelData *)md
{
	UNUSED_PARAMETER(md);

	return true;
}

- (ColorP3Linear)clearColorWithModelData:(ModelData *)md
{
	UNUSED_PARAMETER(md);

#if defined(SHAPE_OF_SPACE_CH_7)
	//	At first I tried using a white background,
	//	but the image looked like four separate subfigures.
	//	Better to use light grey, to hold the figure together
	//	as a single image.
	return (ColorP3Linear){0.8750, 1.0000, 1.0000, 1.0000};
#elif (defined(SHAPE_OF_SPACE_CH_15) || defined(SHAPE_OF_SPACE_CH_16))	//	Fig 15.4, 16.3 or 16.6
	return (ColorP3Linear){1.0000, 1.0000, 1.0000, 1.0000};
#else
	//	We could use some other background color (say, dark blue)
	//	but we'd need to adjust the fog to blend to it.
	return (ColorP3Linear){0.00, 0.00, 0.00, 1.00};
#endif

	//	A transparent background can be useful for screenshots.
//	return (ColorP3Linear){0.00, 0.00, 0.00, 0.00};
}

- (void)encodeCommandsToCommandBuffer:(id<MTLCommandBuffer>)aCommandBuffer
	withRenderPassDescriptor:(MTLRenderPassDescriptor *)aRenderPassDescriptor
	inflightDataBuffers:(NSDictionary<NSString *, id> *)someInflightDataBuffers
	modelData:(ModelData *)md
{
	id<MTLBuffer>				theUniformBuffer;
	TilingBufferSet				*theTilingBufferSet;
	MeshSet						*theCenterpieceMeshSet,
								*theObserverMeshSet;
	id<MTLRenderCommandEncoder>	theRenderEncoder;

	//	Unpack the dictionary of inflight data buffers.
	theUniformBuffer		= [someInflightDataBuffers objectForKey:@"uniform buffer"				];
	theTilingBufferSet		= [someInflightDataBuffers objectForKey:@"tiling buffer set"			];
	theCenterpieceMeshSet	= [someInflightDataBuffers objectForKey:@"rotated centerpiece mesh set"	];
	theObserverMeshSet		= [someInflightDataBuffers objectForKey:@"transformed observer mesh set"];

	//	Create a MTLRenderCommandEncoder no matter what,
	//	to ensure that the framebuffer gets cleared to the background color,
	//	but then...
	theRenderEncoder = [aCommandBuffer renderCommandEncoderWithDescriptor:aRenderPassDescriptor];

	//	...draw a tiling only if itsTotalNumTiles > 0.
	if (theTilingBufferSet->itsTotalNumTiles > 0)
	{
		[theRenderEncoder setDepthStencilState:itsDepthStencilState];

		[theRenderEncoder setVertexBuffer:theUniformBuffer
							offset:0
							atIndex:BufferIndexUniforms];

		[theRenderEncoder setFragmentSamplerState:itsAnisotropicTextureSampler
							atIndex:SamplerIndexPrimary];

		//	Dirichlet domain
		[theRenderEncoder
			setFragmentTexture:	(md->itsShowColorCoding ? itsPaperWallTexture : itsWoodWallTexture)
			atIndex:			TextureIndexPrimary];
		[self encodeMeshWithEncoder:	theRenderEncoder
							meshSet:	itsDirichletWallsMeshSet	//	always non-nil, but will be empty if no Dirichlet domain is visible
							 tiling:	theTilingBufferSet
						  spaceType:	md->itsSpaceType
				 drawBackHemisphere:	md->itsDrawBackHemisphere
					  alphaBlending:	false
								fog:	md->itsFogFlag];
		
		//	vertex figures
		if (md->itsShowVertexFigures)
		{
			[theRenderEncoder
				setFragmentTexture:itsStoneVertexFigureTexture
				atIndex:TextureIndexPrimary];
			[self encodeMeshWithEncoder:	theRenderEncoder
								meshSet:	itsVertexFigureMeshSet
								 tiling:	theTilingBufferSet
							  spaceType:	md->itsSpaceType
					 drawBackHemisphere:	md->itsDrawBackHemisphere
						  alphaBlending:	false
									fog:	md->itsFogFlag];
		}
		
		//	observer
		if (md->itsShowObserver)
		{
			[theRenderEncoder
				setFragmentTexture:itsWhiteObserverTexture
				atIndex:TextureIndexPrimary];
			[self encodeMeshWithEncoder:	theRenderEncoder
								meshSet:	theObserverMeshSet
								 tiling:	theTilingBufferSet
							  spaceType:	md->itsSpaceType
					 drawBackHemisphere:	md->itsDrawBackHemisphere
						  alphaBlending:	false
									fog:	md->itsFogFlag];
		}
		
		//	centerpiece
		//
		//		The galaxy is transparent so, if present,
		//		it should be drawn last, after all opaque content.
		//
		[theRenderEncoder
			setFragmentTexture:itsCenterpieceTexture
			atIndex:TextureIndexPrimary];
		[self encodeMeshWithEncoder:	theRenderEncoder
							meshSet:	theCenterpieceMeshSet
							 tiling:	theTilingBufferSet
						  spaceType:	md->itsSpaceType
				 drawBackHemisphere:	md->itsDrawBackHemisphere
					  alphaBlending:	(itsCenterpieceType == CenterpieceGalaxy)
								fog:	md->itsFogFlag];
	}

	[theRenderEncoder endEncoding];
}

- (void)encodeMeshWithEncoder:	(id<MTLRenderCommandEncoder>)aRenderEncoder
					  meshSet:	(MeshSet *)aMeshSet
					   tiling:	(TilingBufferSet *)aTilingBufferSet
					spaceType:	(SpaceType)aSpaceType
		   drawBackHemisphere:	(bool)aDrawBackHemisphereFlag
				alphaBlending:	(bool)anAlphaBlendingFlag
						  fog:	(bool)aFogFlag
{
	if (aSpaceType == SpaceNone)
		return;
	
	if (aMeshSet->itsMeshes[0] == nil)	//	MeshSet is empty?
		return;

	if (aDrawBackHemisphereFlag)	//	3-sphere or odd-order lens space?
	{
		[self   encodeFullSphereWithEncoder:	aRenderEncoder
									meshSet:	aMeshSet
									 tiling:	aTilingBufferSet
								  spaceType:	aSpaceType
							  alphaBlending:	anAlphaBlendingFlag
										fog:	aFogFlag];
	}
	else							//	typical space
	{
		[self encodeTypicalSpaceWithEncoder:	aRenderEncoder
									meshSet:	aMeshSet
									 tiling:	aTilingBufferSet
								  spaceType:	aSpaceType
							  alphaBlending:	anAlphaBlendingFlag
										fog:	aFogFlag];
	}
}

- (void)encodeFullSphereWithEncoder:	(id<MTLRenderCommandEncoder>)aRenderEncoder
							meshSet:	(MeshSet *)aMeshSet
							 tiling:	(TilingBufferSet *)aTilingBufferSet
						  spaceType:	(SpaceType)aSpaceType
					  alphaBlending:	(bool)anAlphaBlendingFlag
								fog:	(bool)aFogFlag
{
	Mesh						*theMesh;
	id<MTLRenderPipelineState>	thePipelineStateBoxFront	= nil,
								thePipelineStateBoxBack		= nil;
	
	GEOMETRY_GAMES_ASSERT(aSpaceType == SpaceSpherical, "expected spherical space");
	
	//	Curved Space does not currently support partially transparent content
	//	in spaces that require back-hemisphere rendering (namely the 3-sphere
	//	and odd-order lens spaces).  See comments accompanying the declaration
	//	of itsInvertedTilingBuffer in TilingBufferSet.
	if (anAlphaBlendingFlag)
		return;
	
	//	As explained in -encodeTypicalSpaceWithEncoder: below,
	//	spherical spaces look best when we use the finest mesh
	//	for the whole tiling.
	theMesh = aMeshSet->itsMeshes[0];

	//	We'll draw each mesh twice, once for the back hemisphere
	//	and once for the front hemisphere.
	if (theMesh->itsCubeMapFlag)
	{
		if (aFogFlag)
		{
			thePipelineStateBoxFront	= itsRenderPipelineStateSphericalFogBoxFrontCubeMap;
			thePipelineStateBoxBack		= itsRenderPipelineStateSphericalFogBoxBackCubeMap;
		}
		else	//	! aFogFlag
		{
			thePipelineStateBoxFront	= itsRenderPipelineStateNoFogBoxFrontCubeMap;
			thePipelineStateBoxBack		= itsRenderPipelineStateNoFogBoxBackCubeMap;
		}
	}
	else	//	traditional texture
	{
		if (aFogFlag)
		{
			thePipelineStateBoxFront	= itsRenderPipelineStateSphericalFogBoxFront;
			thePipelineStateBoxBack		= itsRenderPipelineStateSphericalFogBoxBack;
		}
		else	//	! aFogFlag
		{
			thePipelineStateBoxFront	= itsRenderPipelineStateNoFogBoxFront;
			thePipelineStateBoxBack		= itsRenderPipelineStateNoFogBoxBack;
		}
	}

	[aRenderEncoder
		setVertexBuffer:theMesh->itsVertexBuffer
		offset:0
		atIndex:BufferIndexVertexAttributes];

	//	All spherical spaces are orientable, so the front-face winding
	//	is always the same.  Following Metal conventions, we set it to clockwise.
	[aRenderEncoder setCullMode:MTLCullModeBack];
	[aRenderEncoder setFrontFacingWinding:MTLWindingClockwise];

	//	back hemisphere
	[aRenderEncoder setRenderPipelineState:thePipelineStateBoxBack];
	[aRenderEncoder setVertexBuffer:aTilingBufferSet->itsInvertedTilingBuffer
						offset:0
						atIndex:BufferIndexTilingGroup];
	[aRenderEncoder	drawIndexedPrimitives:	MTLPrimitiveTypeTriangle
					indexCount:				3 * theMesh->itsNumFacets
					indexType:				MTLIndexTypeUInt16
					indexBuffer:			theMesh->itsIndexBuffer
					indexBufferOffset:		0
					instanceCount:			aTilingBufferSet->itsNumInvertedTiles];

	//	front hemisphere
	[aRenderEncoder setRenderPipelineState:thePipelineStateBoxFront];
	[aRenderEncoder setVertexBuffer:aTilingBufferSet->itsFullBackToFrontTilingBuffer
						offset:0
						atIndex:BufferIndexTilingGroup];
	[aRenderEncoder	drawIndexedPrimitives:	MTLPrimitiveTypeTriangle
					indexCount:				3 * theMesh->itsNumFacets
					indexType:				MTLIndexTypeUInt16
					indexBuffer:			theMesh->itsIndexBuffer
					indexBufferOffset:		0
					instanceCount:			aTilingBufferSet->itsTotalNumTiles];
}

- (void)encodeTypicalSpaceWithEncoder:	(id<MTLRenderCommandEncoder>)aRenderEncoder
							  meshSet:	(MeshSet *)aMeshSet
							   tiling:	(TilingBufferSet *)aTilingBufferSet
							spaceType:	(SpaceType)aSpaceType
						alphaBlending:	(bool)anAlphaBlendingFlag
								  fog:	(bool)aFogFlag
{
	unsigned int				theNumLevelsOfDetail;
	id<MTLRenderPipelineState>	thePipelineState	= nil;
	unsigned int				thePlainLevelCutoffs[MAX_NUM_LOD_LEVELS + 1],
								theReflectedLevelCutoffs[MAX_NUM_LOD_LEVELS + 1],
								theLevel;
	Mesh						*theMesh;
	unsigned int				theNumPlainInstances,
								theNumReflectedInstances;

#if (MAX_NUM_LOD_LEVELS != 4)
#error The code below assumes the maximum number of levels-of-detail is exactly 4 .
#endif
	//	After how many instances should we drop to the next lower level-of-detail?
	//	With the current values,
	//
	//		instance    0        gets rendered with mesh 0
	//		instances   1 -  63  get  rendered with mesh 1
	//		instances  64 - 255  get  rendered with mesh 2
	//		instances 256 -      get  rendered with mesh 3
	//
#if (defined(MAKE_SCREENSHOTS) || SHAPE_OF_SPACE_CH_16 == 3)
	//	In The Shape of Space 3rd edition, Figure 16.3 uses a narrow field of view,
	//	so let's draw higher level-of-detail images of the Earth.
	//	And let's also just higher level-of-detail images for screenshots.
	const unsigned int	theLevelCutoffs[MAX_NUM_LOD_LEVELS + 1] = {0, 32, 256, 4024, UINT_MAX};
#else
	const unsigned int	theLevelCutoffs[MAX_NUM_LOD_LEVELS + 1] = {0, 1, 64, 256, UINT_MAX};
#endif
	
	
	//	How many levels of detail does aMeshSet contain?
	theNumLevelsOfDetail = GetNumLevelsOfDetail(aMeshSet);

	switch (aSpaceType)
	{
		case SpaceSpherical:

			//	For spherical spaces, use the best level of detail for the whole tiling,
			//	because the number of cells is typically not too large, and
			//	the spinning Earths near the antipodal point appear large
			//	and require best quality.
			for (theLevel = 0; theLevel <= theNumLevelsOfDetail; theLevel++)
			{
				thePlainLevelCutoffs[theLevel]		= (theLevel == 0 ? 0 : UINT_MAX);
				
				//	Spherical manifolds never have reflected tiles, but
				//	spherical orbfolds -- for example a mirrored lens -- may have them.
				theReflectedLevelCutoffs[theLevel]	= (theLevel == 0 ? 0 : UINT_MAX);
			}

			break;
		
		case SpaceFlat:
		case SpaceHyperbolic:

			//	Either all the tiles will be plain (not reflected)...
			if (aTilingBufferSet->itsNumReflectedTiles == 0)
			{
				for (theLevel = 0; theLevel <= theNumLevelsOfDetail; theLevel++)
				{
					thePlainLevelCutoffs[theLevel]		= theLevelCutoffs[theLevel];
					theReflectedLevelCutoffs[theLevel]	= 0;
				}
			}
			else	//	... or they'll be roughly half plain and half reflected.
			{
				//	Split the number of instances at each level
				//	between the plain tiles and the reflected ones.
				for (theLevel = 0; theLevel <= theNumLevelsOfDetail; theLevel++)
				{
					thePlainLevelCutoffs[theLevel]		= (theLevelCutoffs[theLevel] < UINT_MAX ?
															(theLevelCutoffs[theLevel] + 1) / 2 :	//	add +1 so integer division rounds up
															UINT_MAX );
					theReflectedLevelCutoffs[theLevel]	= thePlainLevelCutoffs[theLevel];
				}
			}
			
			break;
		
		case SpaceNone:
			GEOMETRY_GAMES_ABORT("The caller should have already excluded SpaceNone");
	}
	
	//	Limit the level cutoffs to the number of tiles actually present.
	for (theLevel = 0; theLevel <= theNumLevelsOfDetail; theLevel++)
	{
		if (thePlainLevelCutoffs[theLevel]     > aTilingBufferSet->itsNumPlainTiles)
			thePlainLevelCutoffs[theLevel]     = aTilingBufferSet->itsNumPlainTiles;
		if (theReflectedLevelCutoffs[theLevel] > aTilingBufferSet->itsNumReflectedTiles)
			theReflectedLevelCutoffs[theLevel] = aTilingBufferSet->itsNumReflectedTiles;
	}
	
	//	The last available level-of-detail should handle all remaining tiles.
	thePlainLevelCutoffs[theNumLevelsOfDetail]		= aTilingBufferSet->itsNumPlainTiles;
	theReflectedLevelCutoffs[theNumLevelsOfDetail]	= aTilingBufferSet->itsNumReflectedTiles;

	if (aFogFlag)
	{
		//	The same GPU vertex function would work for all three geometries
		//	(spherical, Euclidean and hyperbolic), except that
		//	it uses a different fog formula in each case.
		switch (aSpaceType)
		{
			case SpaceSpherical:
				if (anAlphaBlendingFlag)
					thePipelineState = itsRenderPipelineStateSphericalFogBoxFullBlended;
				else
				if (aMeshSet->itsMeshes[0]->itsCubeMapFlag)
					thePipelineState = itsRenderPipelineStateSphericalFogBoxFullCubeMap;
				else
					thePipelineState = itsRenderPipelineStateSphericalFogBoxFull;
				break;
			
			case SpaceFlat:
				if (anAlphaBlendingFlag)
					thePipelineState = itsRenderPipelineStateEuclideanFogBoxFullBlended;
				else
				if (aMeshSet->itsMeshes[0]->itsCubeMapFlag)
					thePipelineState = itsRenderPipelineStateEuclideanFogBoxFullCubeMap;
				else
					thePipelineState = itsRenderPipelineStateEuclideanFogBoxFull;
				break;
				
			case SpaceHyperbolic:
				if (anAlphaBlendingFlag)
					thePipelineState = itsRenderPipelineStateHyperbolicFogBoxFullBlended;
				else
				if (aMeshSet->itsMeshes[0]->itsCubeMapFlag)
					thePipelineState = itsRenderPipelineStateHyperbolicFogBoxFullCubeMap;
				else
					thePipelineState = itsRenderPipelineStateHyperbolicFogBoxFull;
				break;
				
			case SpaceNone:	//	should never occur
				thePipelineState = itsRenderPipelineStateEuclideanFogBoxFull;
				break;
		}
	}
	else	//	! aFogFlag
	{
		if (anAlphaBlendingFlag)
			thePipelineState = itsRenderPipelineStateNoFogBoxFullBlended;
		else
		if (aMeshSet->itsMeshes[0]->itsCubeMapFlag)
			thePipelineState = itsRenderPipelineStateNoFogBoxFullCubeMap;
		else
			thePipelineState = itsRenderPipelineStateNoFogBoxFull;
	}
	[aRenderEncoder setRenderPipelineState:thePipelineState];


	if (anAlphaBlendingFlag)
	{
		//	Curved Spaces' partially transparent content (namely the galaxy)
		//	uses only one level-of-detail, so there's no need for a loop
		//	like in the opaque case below.
		theMesh = aMeshSet->itsMeshes[0];
		[aRenderEncoder setVertexBuffer:theMesh->itsVertexBuffer
							offset:0
							atIndex:BufferIndexVertexAttributes];
		
		//	Curved Spaces' partially transparent content (namely the galaxy)
		//	is 2-sided, so we can disable backface culling.
		[aRenderEncoder setCullMode:MTLCullModeNone];
		[aRenderEncoder setFrontFacingWinding:MTLWindingClockwise];	//	unnecessary but harmless

		//	Draw the instances back-to-front, so more distant instances
		//	appear underneath nearer ones.
		//
		//		Note:  We must draw the plain and reflected instances
		//		all together, in a single back-to-front pass,
		//		to get the transparency right.
		//		All instances will get drawn correctly
		//		because we disabled backface culling.
		//
		[aRenderEncoder setVertexBuffer:aTilingBufferSet->itsFullBackToFrontTilingBuffer
							offset:0
							atIndex:BufferIndexTilingGroup];

		[aRenderEncoder	drawIndexedPrimitives:	MTLPrimitiveTypeTriangle
						indexCount:				3 * theMesh->itsNumFacets
						indexType:				MTLIndexTypeUInt16
						indexBuffer:			theMesh->itsIndexBuffer
						indexBufferOffset:		0
#if (defined(PREPARE_FOR_SCREENSHOT) || defined(MAKE_SCREENSHOTS) || defined(HIGH_RESOLUTION_SCREENSHOT) || (SHAPE_OF_SPACE_CH_16 == 6))
	//	Omit the two nearest images of the galaxy, to avoid obscuring the view.
	//	Somehow it seems that the second image is the one causing the problem
	//	(the most central image is probably behind the observer), which is
	//	why we must omit two images instead of only one.
						instanceCount:			aTilingBufferSet->itsTotalNumTiles - 2];
//	NOT NEEDED:
//		//	omit both the nearest image and the farthest image
//						instanceCount:			aTilingBufferSet->itsTotalNumTiles - 2
//						baseInstance:			1
//						];
#else	//	typical case
						instanceCount:			aTilingBufferSet->itsTotalNumTiles];
#endif
	}
	else	//	! anAlphaBlendingFlag
	{
#ifdef START_OUTSIDE
		if (aTilingBufferSet->itsTotalNumTiles == 1)	//	occurs iff md->itsViewpoint != ViewpointIntrinsic
			[aRenderEncoder setCullMode:MTLCullModeNone];
		else
#endif
		[aRenderEncoder setCullMode:MTLCullModeBack];

		for (theLevel = 0; theLevel < theNumLevelsOfDetail; theLevel++)
		{
			theMesh = aMeshSet->itsMeshes[theLevel];
			
			[aRenderEncoder setVertexBuffer:theMesh->itsVertexBuffer
								offset:0
								atIndex:BufferIndexVertexAttributes];

			//	plain images
			theNumPlainInstances = thePlainLevelCutoffs[theLevel + 1] - thePlainLevelCutoffs[theLevel];
			if (theNumPlainInstances > 0)
			{
				[aRenderEncoder setVertexBuffer:	aTilingBufferSet->itsPlainFrontToBackTilingBuffer
								offset:				thePlainLevelCutoffs[theLevel] * sizeof(simd_float4x4)
								atIndex:			BufferIndexTilingGroup];
				[aRenderEncoder setFrontFacingWinding:MTLWindingClockwise];			//	Metal's default
				[aRenderEncoder	drawIndexedPrimitives:	MTLPrimitiveTypeTriangle
								indexCount:				3 * theMesh->itsNumFacets
								indexType:				MTLIndexTypeUInt16
								indexBuffer:			theMesh->itsIndexBuffer
								indexBufferOffset:		0
								instanceCount:			theNumPlainInstances];
			}

			//	reflected images (if any)
			theNumReflectedInstances = theReflectedLevelCutoffs[theLevel + 1] - theReflectedLevelCutoffs[theLevel];
			if (theNumReflectedInstances > 0)
			{
				[aRenderEncoder setVertexBuffer:	aTilingBufferSet->itsReflectedFrontToBackTilingBuffer
								offset:				theReflectedLevelCutoffs[theLevel] * sizeof(simd_float4x4)
								atIndex:			BufferIndexTilingGroup];
				[aRenderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];	//	the opposite of Metal's default
				[aRenderEncoder	drawIndexedPrimitives:	MTLPrimitiveTypeTriangle
								indexCount:				3 * theMesh->itsNumFacets
								indexType:				MTLIndexTypeUInt16
								indexBuffer:			theMesh->itsIndexBuffer
								indexBufferOffset:		0
								instanceCount:			theNumReflectedInstances];
			}
		}
	}
}


#pragma mark -
#pragma mark mesh transformations

- (void)writeRotatedCenterpieceIntoMeshSet:(MeshSet *)aRotatedCenterpieceMeshSet modelData:(ModelData *)md
{
	unsigned int			theNumLevelsOfDetail,
							theLevel;
	NSUInteger				theVertexBufferLengthInBytes,
							theIndexBufferLengthInBytes;
	CurvedSpacesVertexData	*theSrcVertexData,
							*theDstVertexData;
	Matrix					theSpin,
							theTilt,
							theOrientation;	//	“orientation” in the sense of an element of O(3),
											//		not a connected component of O(3)
#ifdef CENTERPIECE_DISPLACEMENT
	Matrix					thePlacement;
#endif
	simd_float4x4			thePlacementAsSIMD;
	NSUInteger				theNumVertices;
	unsigned int			i;
	UInt16					(*theSrcIndexData)[3],
							(*theDstIndexData)[3];
#ifdef CENTERPIECE_DISPLACEMENT
	UInt16					theSwapValue;
#endif


	switch (md->itsCenterpieceType)
	{
		case CenterpieceNone:
			MatrixIdentity(&theSpin);
			MatrixIdentity(&theTilt);
			break;

		case CenterpieceEarth:
			MatrixRotation(&theSpin, 0.0, 0.0, EARTH_SPEED * md->itsRotationAngle);
			MatrixRotation(&theTilt, -PI/2, 0.0, 0.0);	//	takes Earth's spin axis from z-axis to y-axis
			break;
			
		case CenterpieceGalaxy:
			MatrixRotation(&theSpin, 0.0, 0.0, GALAXY_SPEED * md->itsRotationAngle);
			MatrixRotation(&theTilt, 0.2, 0.3, 0.0);	//	arbitrary spin axis that looks nice
			break;
			
		case CenterpieceGyroscope:
			MatrixRotation(&theSpin, 0.0, 0.0, GYROSCOPE_SPEED * md->itsRotationAngle);
			MatrixRotation(&theTilt, -PI/2, 0.0, 0.0);	//	takes gyroscope's spin axis from z-axis to y-axis
			break;

#ifdef SHAPE_OF_SPACE_CH_7
		case CenterpieceCube:
			MatrixRotation(&theSpin, 0.0, 0.0, 0.0);
			MatrixRotation(&theTilt, 0.0, 0.0, 0.0);
			break;
#endif
	}
	MatrixProduct(&theSpin, &theTilt, &theOrientation);
#ifdef CENTERPIECE_DISPLACEMENT
	MatrixProduct(&theOrientation, &md->itsCenterpiecePlacement, &thePlacement);
	thePlacementAsSIMD = ConvertMatrix44ToSIMD(thePlacement.m);
#else
	thePlacementAsSIMD = ConvertMatrix44ToSIMD(theOrientation.m);	//	translational component is zero
#endif

	theNumLevelsOfDetail = GetNumLevelsOfDetail(itsUnrotatedCenterpieceMeshSet);

	for (theLevel = 0; theLevel < theNumLevelsOfDetail; theLevel++)
	{
		//	Make sure that this level of detail
		//	is present in aRotatedCenterpieceMeshSet ...
		if (aRotatedCenterpieceMeshSet->itsMeshes[theLevel] == nil)
			aRotatedCenterpieceMeshSet->itsMeshes[theLevel] = MakeEmptyMesh();

		//	... and that it contains buffers of the correct size.

		theVertexBufferLengthInBytes	= [itsUnrotatedCenterpieceMeshSet->itsMeshes[theLevel]->itsVertexBuffer length];
		theIndexBufferLengthInBytes		= [itsUnrotatedCenterpieceMeshSet->itsMeshes[theLevel]->itsIndexBuffer  length];

		GEOMETRY_GAMES_ASSERT(theVertexBufferLengthInBytes > 0, "Source vertex buffer is empty");
		GEOMETRY_GAMES_ASSERT(theIndexBufferLengthInBytes > 0,  "Source index buffer is empty" );

		[self adjustSizeOfMesh:	aRotatedCenterpieceMeshSet->itsMeshes[theLevel]
			 vertexBufferBytes:	theVertexBufferLengthInBytes
			  indexBufferBytes:	theIndexBufferLengthInBytes];

		//	Copy itsNumFacets.
		aRotatedCenterpieceMeshSet->itsMeshes[theLevel]->itsNumFacets = itsUnrotatedCenterpieceMeshSet->itsMeshes[theLevel]->itsNumFacets;

		//	Copy the vertex data.
		theSrcVertexData = (CurvedSpacesVertexData *) [itsUnrotatedCenterpieceMeshSet->itsMeshes[theLevel]->itsVertexBuffer contents];
		theDstVertexData = (CurvedSpacesVertexData *) [aRotatedCenterpieceMeshSet->itsMeshes[theLevel]->itsVertexBuffer contents];
		memcpy(theDstVertexData, theSrcVertexData, theVertexBufferLengthInBytes);

		//	Rotate the vertex data.
		GEOMETRY_GAMES_ASSERT(
			theVertexBufferLengthInBytes % sizeof(CurvedSpacesVertexData) == 0,
			"The vertex buffer length is not an integer multiple of the CurvedSpacesVertexData size.");
		theNumVertices = theVertexBufferLengthInBytes / sizeof(CurvedSpacesVertexData);
		for (i = 0; i < theNumVertices; i++)
		{
			//	The SIMD library uses right-to-left matrix actions,
			//	in contrast to our left-to-right actions.
			//	So to realize our
			//
			//		(column vector) (matrix)
			//
			//	we need to pass simd_mul(matrix, row vector).
			//
			theDstVertexData[i].pos = simd_mul(thePlacementAsSIMD, theSrcVertexData[i].pos);
		}

		//	Copy the index data.
		theSrcIndexData = (UInt16 (*)[3]) [itsUnrotatedCenterpieceMeshSet->itsMeshes[theLevel]->itsIndexBuffer contents];
		theDstIndexData = (UInt16 (*)[3]) [aRotatedCenterpieceMeshSet->itsMeshes[theLevel]->itsIndexBuffer contents];
		memcpy(theDstIndexData, theSrcIndexData, theIndexBufferLengthInBytes);

#ifdef CENTERPIECE_DISPLACEMENT
		//	If itsCenterpiecePlacement has negative chirality,
		//	then reverse the order of each triangle's indices,
		//	so that aRotatedCenterpieceMeshSet always has
		//	the same winding sense as itsUnrotatedCenterpieceMeshSet.
		//
		if (md->itsCenterpiecePlacement.itsParity == ImageNegative)
		{
			for (i = 0; i < aRotatedCenterpieceMeshSet->itsMeshes[theLevel]->itsNumFacets; i++)
			{
				theSwapValue			= theDstIndexData[i][0];
				theDstIndexData[i][0]	= theDstIndexData[i][2];
				theDstIndexData[i][2]	= theSwapValue;
			}
		}
#endif

		//	Copy itsCubeMapFlag.
		aRotatedCenterpieceMeshSet->itsMeshes[theLevel]->itsCubeMapFlag = itsUnrotatedCenterpieceMeshSet->itsMeshes[theLevel]->itsCubeMapFlag;
	}

	//	Clear any unneeded levels of detail.
	for (theLevel = theNumLevelsOfDetail; theLevel < MAX_NUM_LOD_LEVELS; theLevel++)
	{
		aRotatedCenterpieceMeshSet->itsMeshes[theLevel] = nil;
	}
}

- (void)writeTransformedObserverIntoMeshSet:(MeshSet *)aTransformedObserverMeshSet modelData:(ModelData *)md
{
	NSUInteger				theVertexBufferLengthInBytes,
							theIndexBufferLengthInBytes;
	CurvedSpacesVertexData	*theSrcVertexData,
							*theDstVertexData;
	simd_float4x4			theUserBodyPlacementAsSIMD;
	NSUInteger				theNumVertices;
	unsigned int			i;
	UInt16					(*theSrcIndexData)[3],
							(*theDstIndexData)[3],
							theSwapValue;
	unsigned int			theLevel;

	//	The observer mesh set has only one level of detail.
	if (md->itsShowObserver)
	{
		//	Make sure that level of detail 0
		//	is present in aTransformedObserverMeshSet ...
		if (aTransformedObserverMeshSet->itsMeshes[0] == nil)
			aTransformedObserverMeshSet->itsMeshes[0] = MakeEmptyMesh();

		//	... and that it contains buffers of the correct size.

		theVertexBufferLengthInBytes	= [itsUntransformedObserverMeshSet->itsMeshes[0]->itsVertexBuffer length];
		theIndexBufferLengthInBytes		= [itsUntransformedObserverMeshSet->itsMeshes[0]->itsIndexBuffer  length];

		GEOMETRY_GAMES_ASSERT(theVertexBufferLengthInBytes > 0, "Source vertex buffer is empty");
		GEOMETRY_GAMES_ASSERT(theIndexBufferLengthInBytes > 0,  "Source index buffer is empty" );

		[self adjustSizeOfMesh:	aTransformedObserverMeshSet->itsMeshes[0]
			 vertexBufferBytes:	theVertexBufferLengthInBytes
			  indexBufferBytes:	theIndexBufferLengthInBytes];

		//	Copy itsNumFacets.
		aTransformedObserverMeshSet->itsMeshes[0]->itsNumFacets = itsUntransformedObserverMeshSet->itsMeshes[0]->itsNumFacets;

		//	Copy the vertex data.
		theSrcVertexData = (CurvedSpacesVertexData *) [itsUntransformedObserverMeshSet->itsMeshes[0]->itsVertexBuffer contents];
		theDstVertexData = (CurvedSpacesVertexData *) [aTransformedObserverMeshSet->itsMeshes[0]->itsVertexBuffer contents];
		memcpy(theDstVertexData, theSrcVertexData, theVertexBufferLengthInBytes);

		//	Rotate the vertex data.
		theUserBodyPlacementAsSIMD = ConvertMatrix44ToSIMD(md->itsUserBodyPlacement.m);
		GEOMETRY_GAMES_ASSERT(
			theVertexBufferLengthInBytes % sizeof(CurvedSpacesVertexData) == 0,
			"The vertex buffer length is not an integer multiple of the CurvedSpacesVertexData size.");
		theNumVertices = theVertexBufferLengthInBytes / sizeof(CurvedSpacesVertexData);
		for (i = 0; i < theNumVertices; i++)
		{
			//	The SIMD library uses right-to-left matrix actions,
			//	in contrast to our left-to-right actions.
			//	So to realize our
			//
			//		(column vector) (matrix)
			//
			//	we need to pass simd_mul(matrix, row vector).
			//
			theDstVertexData[i].pos = simd_mul(theUserBodyPlacementAsSIMD, theSrcVertexData[i].pos);
		}

		//	Copy the index data.
		theSrcIndexData = (UInt16 (*)[3]) [itsUntransformedObserverMeshSet->itsMeshes[0]->itsIndexBuffer contents];
		theDstIndexData = (UInt16 (*)[3]) [aTransformedObserverMeshSet->itsMeshes[0]->itsIndexBuffer contents];
		memcpy(theDstIndexData, theSrcIndexData, theIndexBufferLengthInBytes);

		//	If itsUserBodyPlacement has negative chirality,
		//	then reverse the order of each triangle's indices,
		//	so that aTransformedObserverMeshSet always has
		//	the same winding sense as itsUntransformedObserverMeshSet.
		//
		if (md->itsUserBodyPlacement.itsParity == ImageNegative)
		{
			for (i = 0; i < aTransformedObserverMeshSet->itsMeshes[0]->itsNumFacets; i++)
			{
				theSwapValue			= theDstIndexData[i][0];
				theDstIndexData[i][0]	= theDstIndexData[i][2];
				theDstIndexData[i][2]	= theSwapValue;
			}
		}

		//	Copy itsCubeMapFlag.
		aTransformedObserverMeshSet->itsMeshes[0]->itsCubeMapFlag = itsUntransformedObserverMeshSet->itsMeshes[0]->itsCubeMapFlag;
	}
	else	//	! itsShowObserver
	{
		aTransformedObserverMeshSet->itsMeshes[0] = nil;
	}

	//	The remaining levels of detail should always be nil.
	for (theLevel = 1; theLevel < MAX_NUM_LOD_LEVELS; theLevel++)
		aTransformedObserverMeshSet->itsMeshes[theLevel] = nil;
}

- (void)adjustSizeOfMesh:	(Mesh *)aMesh
	   vertexBufferBytes:	(NSUInteger)aRequiredVertexBufferLengthInBytes
		indexBufferBytes:	(NSUInteger)aRequiredIndexBufferLengthInBytes
{
	//	vertex buffer

	if (aRequiredVertexBufferLengthInBytes > 0)
	{
		if (aMesh->itsVertexBuffer == nil
		 || [aMesh->itsVertexBuffer length] != aRequiredVertexBufferLengthInBytes)
		{
			aMesh->itsVertexBuffer = [itsDevice
				newBufferWithLength:	aRequiredVertexBufferLengthInBytes
				options:				MTLResourceStorageModeShared];
		}
	}
	else
	{
		aMesh->itsVertexBuffer = nil;
	}

	//	index buffer
	
	if (aRequiredIndexBufferLengthInBytes > 0)
	{
		if (aMesh->itsIndexBuffer == nil
		 || [aMesh->itsIndexBuffer length] != aRequiredIndexBufferLengthInBytes)
		{
			aMesh->itsIndexBuffer = [itsDevice
				newBufferWithLength:	aRequiredIndexBufferLengthInBytes
				options:				MTLResourceStorageModeShared];
		}
	}
	else
	{
		aMesh->itsIndexBuffer = nil;
	}
}


@end


static id<MTLRenderPipelineState> MakePipelineState(
	id<MTLDevice>			aDevice,
	MTLPixelFormat			aColorPixelFormat,
	id<MTLLibrary>			aGPUFunctionLibrary,
	bool					aMultisamplingFlag,
	ShaderFogAndClipBoxType	aShaderFogAndClipBoxType,
	bool					aCubeMapFlag,
	bool					anAlphaBlendingFlag)
{
	MTLFunctionConstantValues	*theCompileTimeConstants;
	uint32_t					theFogAndClipBoxType;
	uint8_t						theCubeMapFlag;
	id<MTLFunction>				theGPUVertexFunction,
								theGPUFragmentFunction;
	NSError						*theError;
	MTLVertexDescriptor			*theVertexDescriptor;
	MTLRenderPipelineDescriptor	*thePipelineDescriptor;
	id<MTLRenderPipelineState>	thePipelineState;

#if defined(SHAPE_OF_SPACE_CH_7)
	uint32_t					theShapeOfSpaceFigure	= 70;	//	various figures in Chapter 7
#elif defined(SHAPE_OF_SPACE_CH_15)	//	Fig 15.4
	uint32_t					theShapeOfSpaceFigure	= 154;	//	figure 15.4
#elif (SHAPE_OF_SPACE_CH_16 == 3)	//	Fig 16.3
	uint32_t					theShapeOfSpaceFigure	= 163;	//	figure 16.3
#elif (SHAPE_OF_SPACE_CH_16 == 6)	//	Fig 16.6
	uint32_t					theShapeOfSpaceFigure	= 166;	//	figure 16.6
#else
	uint32_t					theShapeOfSpaceFigure	= 0;	//	not making a Shape of Space figure
#endif

	//	GPU functions

	theCompileTimeConstants	= [[MTLFunctionConstantValues alloc] init];
	theFogAndClipBoxType	= aShaderFogAndClipBoxType;	//	copy to 32-bit variable, don't assume that aShaderFogAndClipBoxType is 32-bit
	theCubeMapFlag			= aCubeMapFlag;				//	copy to  8-bit variable, don't assume that aCubeMapFlag is 8-bit
	[theCompileTimeConstants setConstantValue:&theFogAndClipBoxType		type:MTLDataTypeUInt withName:@"gFogAndClipBoxType"	];
	[theCompileTimeConstants setConstantValue:&theCubeMapFlag			type:MTLDataTypeBool withName:@"gUseCubeMap"		];
	[theCompileTimeConstants setConstantValue:&theShapeOfSpaceFigure	type:MTLDataTypeUInt withName:@"gShapeOfSpaceFigure"];
	theGPUVertexFunction	= [aGPUFunctionLibrary newFunctionWithName:@"CurvedSpacesVertexFunction"
								constantValues:theCompileTimeConstants error:&theError];
	theGPUFragmentFunction	= [aGPUFunctionLibrary newFunctionWithName:@"CurvedSpacesFragmentFunction"
								constantValues:theCompileTimeConstants error:&theError];

	//	vertex descriptor

	theVertexDescriptor = [MTLVertexDescriptor vertexDescriptor];

	[[theVertexDescriptor attributes][VertexAttributePosition] setFormat:MTLVertexFormatFloat4];
	[[theVertexDescriptor attributes][VertexAttributePosition] setBufferIndex:BufferIndexVertexAttributes];
	[[theVertexDescriptor attributes][VertexAttributePosition] setOffset:offsetof(CurvedSpacesVertexData, pos)];

	[[theVertexDescriptor attributes][VertexAttributeTexCoords] setFormat:MTLVertexFormatFloat3];
	[[theVertexDescriptor attributes][VertexAttributeTexCoords] setBufferIndex:BufferIndexVertexAttributes];
	[[theVertexDescriptor attributes][VertexAttributeTexCoords] setOffset:offsetof(CurvedSpacesVertexData, tex)];

	[[theVertexDescriptor attributes][VertexAttributeColor] setFormat:MTLVertexFormatHalf4];
	[[theVertexDescriptor attributes][VertexAttributeColor] setBufferIndex:BufferIndexVertexAttributes];
	[[theVertexDescriptor attributes][VertexAttributeColor] setOffset:offsetof(CurvedSpacesVertexData, col)];

	[[theVertexDescriptor layouts][BufferIndexVertexAttributes] setStepFunction:MTLVertexStepFunctionPerVertex];
	[[theVertexDescriptor layouts][BufferIndexVertexAttributes] setStride:sizeof(CurvedSpacesVertexData)];

	//	pipeline state

	thePipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
	[thePipelineDescriptor setLabel:@"Curved Spaces render pipeline"];
	[thePipelineDescriptor setSampleCount:(aMultisamplingFlag ? METAL_MULTISAMPLING_NUM_SAMPLES : 1)];
	[thePipelineDescriptor setVertexFunction:  theGPUVertexFunction  ];
	[thePipelineDescriptor setFragmentFunction:theGPUFragmentFunction];
	[thePipelineDescriptor setVertexDescriptor:theVertexDescriptor];
	[[thePipelineDescriptor colorAttachments][0] setPixelFormat:aColorPixelFormat];
	[thePipelineDescriptor setDepthAttachmentPixelFormat:MTLPixelFormatDepth32Float];
	[thePipelineDescriptor setStencilAttachmentPixelFormat:MTLPixelFormatInvalid];	//	redundant -- this is the default

	//	Alpha blending determines how the final fragment
	//	blends in with the previous color buffer contents.
	//	For opaque surfaces we may disable blending.
	//	For partially transparent surfaces we may enable blending
	//	but must take care to draw the scene in back-to-front order.
	if (anAlphaBlendingFlag)
	{
		//	The comment accompanying the definition of PREMULTIPLY_RGBA()
		//	explains the (1, 1 - α) blend factors.
		[[thePipelineDescriptor colorAttachments][0] setBlendingEnabled:YES];
		[[thePipelineDescriptor colorAttachments][0] setRgbBlendOperation:MTLBlendOperationAdd];
		[[thePipelineDescriptor colorAttachments][0] setAlphaBlendOperation:MTLBlendOperationAdd];
		[[thePipelineDescriptor colorAttachments][0] setSourceRGBBlendFactor:MTLBlendFactorOne];
		[[thePipelineDescriptor colorAttachments][0] setSourceAlphaBlendFactor:MTLBlendFactorOne];
		[[thePipelineDescriptor colorAttachments][0] setDestinationRGBBlendFactor:MTLBlendFactorOneMinusSourceAlpha];
		[[thePipelineDescriptor colorAttachments][0] setDestinationAlphaBlendFactor:MTLBlendFactorOneMinusSourceAlpha];
	}
	else
	{
		[[thePipelineDescriptor colorAttachments][0] setBlendingEnabled:NO];	//	redundant -- this is the default
	}

	thePipelineState = [aDevice newRenderPipelineStateWithDescriptor:thePipelineDescriptor error:NULL];
	
	return thePipelineState;
}


static TilingBufferSet *MakeEmptyTilingBufferSet(void)
{
	TilingBufferSet	*theTilingBufferSet;
	
	theTilingBufferSet = [[TilingBufferSet alloc] init];

	theTilingBufferSet->itsNumPlainTiles		= 0;
	theTilingBufferSet->itsNumReflectedTiles	= 0;
	theTilingBufferSet->itsTotalNumTiles		= 0;

	theTilingBufferSet->itsPlainFrontToBackTilingBuffer		= nil;
	theTilingBufferSet->itsReflectedFrontToBackTilingBuffer	= nil;
	theTilingBufferSet->itsFullBackToFrontTilingBuffer		= nil;

	return theTilingBufferSet;
}


static MeshSet *MakeEmptyMeshSet(void)
{
	MeshSet			*theMeshSet;
	unsigned int	i;
	
	theMeshSet = [[MeshSet alloc] init];

	for (i = 0; i < MAX_NUM_LOD_LEVELS; i++)
		theMeshSet->itsMeshes[i] = nil;
	
	return theMeshSet;
}

static Mesh *MakeEmptyMesh(void)
{
	Mesh	*theMesh;
	
	theMesh = [[Mesh alloc] init];

	theMesh->itsNumFacets		= 0;
	theMesh->itsVertexBuffer	= nil;
	theMesh->itsIndexBuffer		= nil;
	theMesh->itsCubeMapFlag		= false;
	
	return theMesh;
}

static void RefreshDirichletWalls(
	MeshSet			*aMeshSet,	//	output;  ≠ nil, but the Meshes within it may be nil
	id<MTLDevice>	aDevice,	//	input
	ModelData		*md)		//	input
{
	unsigned int	theNumMeshVertices				= 0;
	double			(*theMeshVertexPositions)[4]	= NULL,
					(*theMeshVertexTexCoords)[3]	= NULL,	//	last tex coord is always zero, because we don't use a cube map texture
					(*theMeshVertexColors)[4]		= NULL;	//	premultiplied alpha
	unsigned int	theNumMeshFacets				= 0,
					(*theMeshFacets)[3]				= NULL,
					theLevel;


	//	itsMeshes[0] should be present iff
	//
	//		a Dirichlet domain is present
	//	and
	//		the aperture is at least partly closed.
	//
	if (md->itsDirichletDomain == NULL	//	Dirichlet domain not present
	 || md->itsAperture == 1.0)			//	aperture fully open (meaning walls are completely invisible)
	{
		aMeshSet->itsMeshes[0] = nil;
	}
	else	//	Dirichlet domain is present and visible
	{
		//	Create a mesh for the Dirichlet domain
		//	with windows cut in its walls.
		MakeDirichletMesh(	md->itsDirichletDomain,
							md->itsAperture,
							md->itsShowColorCoding,
							&theNumMeshVertices,
							&theMeshVertexPositions,
							&theMeshVertexTexCoords,
							&theMeshVertexColors,
							&theNumMeshFacets,
							&theMeshFacets);

		//	The Dirichlet domain walls are kept in a single Mesh object,
		//	(containing one vertex buffer and one index buffer),
		//	not a series of "inflight" Mesh objects.
		//	So we must be careful not to modify a buffer
		//	that the GPU might currently be using.
		//	To rigorously avoid trouble, let's create a new Mesh object
		//	each time RefreshDirichletWalls() gets called.
		//	This may be less efficient, but users spend only
		//	a small amount of time adjusting the aperture size,
		//	so the inefficiency is unlikely to be significant.
		//
		//		Comment:  Standard practice seems to be
		//		to recycle the "inflight" buffers used for uniforms
		//		and other data that changes once per frame,
		//		but to be honest I don't understand why
		//		the time required to re-allocate those buffers
		//		once per frame wouldn't be trivial.
		//		(Maybe things are more complicated with
		//		managed buffers that must be coordinated
		//		between the CPU and the GPU, even if
		//		buffers in shared memory could be allocated quickly?)
		//
		aMeshSet->itsMeshes[0] = MakeMeshFromArrays(
									aDevice,
									theNumMeshVertices,
									theMeshVertexPositions,
									theMeshVertexTexCoords,
									theMeshVertexColors,
									theNumMeshFacets,
									theMeshFacets,
									false);	//	traditional texture (not a cube map)
		
		//	Free the Dirichlet mesh data.
		FreeDirichletMesh(	&theNumMeshVertices,
							&theMeshVertexPositions,
							&theMeshVertexTexCoords,
							&theMeshVertexColors,
							&theNumMeshFacets,
							&theMeshFacets);
	}
	
	//	The Dirichlet domain walls never need any additional levels of detail.
	for (theLevel = 1; theLevel < MAX_NUM_LOD_LEVELS; theLevel++)
		aMeshSet->itsMeshes[theLevel] = nil;
}


static MeshSet *MakeCenterpieceMeshSet(
	CenterpieceType	aCenterpieceType,
	id<MTLDevice>	aDevice)
{
	switch (aCenterpieceType)
	{
		case CenterpieceNone:		return MakeEmptyMeshSet();
		case CenterpieceEarth:		return MakeEarthMeshSet(aDevice);
		case CenterpieceGalaxy:		return MakeGalaxyMeshSet(aDevice);
		case CenterpieceGyroscope:	return MakeGyroscopeMeshSet(aDevice);
#ifdef SHAPE_OF_SPACE_CH_7
		case CenterpieceCube:		return MakeCubeMeshSet(aDevice);
#endif
	}
}

static MeshSet *MakeEarthMeshSet(
	id<MTLDevice>	aDevice)
{
	MeshSet			*theMeshSet;
	unsigned int	theLevel,
					theNumSubdivisions;
	unsigned int	theNumMeshVertices				= 0;
	double			(*theMeshVertexPositions)[4]	= NULL,
					(*theMeshVertexTexCoords)[3]	= NULL,	//	3 tex coords for cube map texture
					(*theMeshVertexColors)[4]		= NULL;	//	premultiplied alpha
	unsigned int	theNumMeshFacets				= 0,
					(*theMeshFacets)[3]				= NULL;

	static const unsigned int	theNumLevels		= 4;
	static const double			theWhiteColor[4]	= {1.0, 1.0, 1.0, 1.0};	//	pre-multiplied (αR, αG, αB, α)

	GEOMETRY_GAMES_ASSERT(
		MAX_NUM_LOD_LEVELS >= theNumLevels,
		"Sphere mesh set creation code uses 4 levels of detail");
	
	theMeshSet = MakeEmptyMeshSet();

	for (theLevel = 0; theLevel < theNumLevels; theLevel++)
	{
		//	Level 0 holds the finest subdivision.
		//	Level 1 holds the second finest subdivision.
		//	Level 2 is next.   Then
		//	Level 3 holds the coarsest subdivision (a plain dodecahedron).
		theNumSubdivisions = (theNumLevels - 1) - theLevel;
		
		//	Let the platform-independent code create the mesh
		//	and return the data as C arrays
		//	(which we must free when we're done with them).
		//
		//		Design note:  Each call to MakeSphereMesh()
		//		generates all the subdivisions.  We don't mind
		//		the inefficiency, because this MakeEarthMeshSet() method
		//		gets called only once.  The simplicity of the code
		//		is more important.
		//
		MakeSphereMesh(	EARTH_RADIUS,
						theNumSubdivisions,
						theWhiteColor,
						&theNumMeshVertices,
						&theMeshVertexPositions,
						&theMeshVertexTexCoords,
						&theMeshVertexColors,
						&theNumMeshFacets,
						&theMeshFacets);

		//	Use the C arrays to construct a Mesh object for this level.
		theMeshSet->itsMeshes[theLevel] = MakeMeshFromArrays(
												aDevice,
												theNumMeshVertices,
												theMeshVertexPositions,
												theMeshVertexTexCoords,
												theMeshVertexColors,
												theNumMeshFacets,
												theMeshFacets,
												true);	//	The sphere mesh uses a cube map texture

		//	Free the temporary C arrays.
		FreeSphereMesh(	&theNumMeshVertices,
						&theMeshVertexPositions,
						&theMeshVertexTexCoords,
						&theMeshVertexColors,
						&theNumMeshFacets,
						&theMeshFacets);
	}

	//	All done!
	return theMeshSet;
}


static MeshSet *MakeGalaxyMeshSet(
	id<MTLDevice>	aDevice)
{
	MeshSet	*theMeshSet;
	Mesh	*theMesh;

	const CurvedSpacesVertexData	theSquareVertices[4] =
	{
		{{-GALAXY_RADIUS, -GALAXY_RADIUS, 0.0, 1.0}, {0.0, 1.0, 0.0}, {1.0, 1.0, 1.0, 1.0}},
		{{-GALAXY_RADIUS, +GALAXY_RADIUS, 0.0, 1.0}, {0.0, 0.0, 0.0}, {1.0, 1.0, 1.0, 1.0}},
		{{+GALAXY_RADIUS, -GALAXY_RADIUS, 0.0, 1.0}, {1.0, 1.0, 0.0}, {1.0, 1.0, 1.0, 1.0}},
		{{+GALAXY_RADIUS, +GALAXY_RADIUS, 0.0, 1.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 1.0, 1.0}}
	};
	
	const UInt16					theSquareFacets[2][3] =
	{
		{0, 1, 2},
		{2, 1, 3}
	};

	theMeshSet					= MakeEmptyMeshSet();
	theMeshSet->itsMeshes[0]	= MakeEmptyMesh();
	theMesh						= theMeshSet->itsMeshes[0];

	theMesh->itsNumFacets = BUFFER_LENGTH(theSquareFacets);

	theMesh->itsVertexBuffer = [aDevice
		newBufferWithBytes:	theSquareVertices
		length:				sizeof(theSquareVertices)
		options:			MTLResourceStorageModeShared];

	theMesh->itsIndexBuffer = [aDevice
		newBufferWithBytes:	theSquareFacets
		length:				sizeof(theSquareFacets)
		options:			MTLResourceStorageModeShared];

	theMesh->itsCubeMapFlag = false;
	
	return theMeshSet;
}


static MeshSet *MakeGyroscopeMeshSet(
	id<MTLDevice>	aDevice)
{
	MeshSet			*theMeshSet;
	unsigned int	theNumMeshVertices				= 0;
	double			(*theMeshVertexPositions)[4]	= NULL,
					(*theMeshVertexTexCoords)[3]	= NULL,	//	last tex coord is always zero, because we don't use a cube map texture
					(*theMeshVertexColors)[4]		= NULL;	//	premultiplied alpha
	unsigned int	theNumMeshFacets				= 0,
					(*theMeshFacets)[3]				= NULL;

	theMeshSet = MakeEmptyMeshSet();

	//	Let the platform-independent code create the mesh
	//	and return the data as C arrays
	//	(which we must free when we're done with them).
	//
	MakeGyroscopeMesh(	&theNumMeshVertices,
						&theMeshVertexPositions,
						&theMeshVertexTexCoords,
						&theMeshVertexColors,
						&theNumMeshFacets,
						&theMeshFacets);

	//	Use the C arrays to construct a Mesh object.
	theMeshSet->itsMeshes[0] = MakeMeshFromArrays(
								aDevice,
								theNumMeshVertices,
								theMeshVertexPositions,
								theMeshVertexTexCoords,
								theMeshVertexColors,
								theNumMeshFacets,
								theMeshFacets,
								false);	//	traditional texture (not a cube map)

	//	Free the temporary C arrays.
	FreeGyroscopeMesh(	&theNumMeshVertices,
						&theMeshVertexPositions,
						&theMeshVertexTexCoords,
						&theMeshVertexColors,
						&theNumMeshFacets,
						&theMeshFacets);
	
	return theMeshSet;
}


#ifdef SHAPE_OF_SPACE_CH_7

static MeshSet *MakeCubeMeshSet(
	id<MTLDevice>	aDevice)
{
	MeshSet			*theMeshSet;
	unsigned int	theNumMeshVertices				= 0;
	double			(*theMeshVertexPositions)[4]	= NULL,
					(*theMeshVertexTexCoords)[3]	= NULL,	//	last tex coord is always zero, because we don't use a cube map texture
					(*theMeshVertexColors)[4]		= NULL;	//	premultiplied alpha
	unsigned int	theNumMeshFacets				= 0,
					(*theMeshFacets)[3]				= NULL;

	theMeshSet = MakeEmptyMeshSet();

	//	Let the platform-independent code create the mesh
	//	and return the data as C arrays
	//	(which we must free when we're done with them).
	//
	MakeCubeMesh(	&theNumMeshVertices,
					&theMeshVertexPositions,
					&theMeshVertexTexCoords,
					&theMeshVertexColors,
					&theNumMeshFacets,
					&theMeshFacets);

	//	Use the C arrays to construct a Mesh object.
	theMeshSet->itsMeshes[0] = MakeMeshFromArrays(
								aDevice,
								theNumMeshVertices,
								theMeshVertexPositions,
								theMeshVertexTexCoords,
								theMeshVertexColors,
								theNumMeshFacets,
								theMeshFacets,
								false);	//	traditional texture (not a cube map)

	//	Free the temporary C arrays.
	FreeCubeMesh(	&theNumMeshVertices,
					&theMeshVertexPositions,
					&theMeshVertexTexCoords,
					&theMeshVertexColors,
					&theNumMeshFacets,
					&theMeshFacets);
	
	return theMeshSet;
}

#endif


static MeshSet *MakeVertexFigureMeshSet(
	id<MTLDevice>	aDevice,
	ModelData		*md)
{
	MeshSet			*theMeshSet;
	unsigned int	theNumMeshVertices				= 0;
	double			(*theMeshVertexPositions)[4]	= NULL,
					(*theMeshVertexTexCoords)[3]	= NULL,	//	last tex coord is always zero, because we don't use a cube map texture
					(*theMeshVertexColors)[4]		= NULL;	//	premultiplied alpha
	unsigned int	theNumMeshFacets				= 0,
					(*theMeshFacets)[3]				= NULL;

	theMeshSet = MakeEmptyMeshSet();

	if (md->itsDirichletDomain != NULL)
	{
		//	Let the platform-independent code create the mesh
		//	and return the data as C arrays
		//	(which we must free when we're done with them).
		//
		MakeVertexFigureMesh(	md->itsDirichletDomain,
								&theNumMeshVertices,
								&theMeshVertexPositions,
								&theMeshVertexTexCoords,
								&theMeshVertexColors,
								&theNumMeshFacets,
								&theMeshFacets);

		//	Use the C arrays to construct a Mesh object.
		theMeshSet->itsMeshes[0] = MakeMeshFromArrays(
									aDevice,
									theNumMeshVertices,
									theMeshVertexPositions,
									theMeshVertexTexCoords,
									theMeshVertexColors,
									theNumMeshFacets,
									theMeshFacets,
									false);	//	traditional texture (not a cube map)

		//	Free the temporary C arrays.
		FreeVertexFigureMesh(	&theNumMeshVertices,
								&theMeshVertexPositions,
								&theMeshVertexTexCoords,
								&theMeshVertexColors,
								&theNumMeshFacets,
								&theMeshFacets);
	}
	
	return theMeshSet;
}


static MeshSet *MakeObserverMeshSet(
	id<MTLDevice>	aDevice)
{
	MeshSet	*theMeshSet;
	Mesh	*theMesh;

//	Represent the observer as a dart with four fletches.

	const CurvedSpacesVertexData	theObserverVertices[4*4 + 8] =
									{
										//	left fletch
										{{-DART_INNER_WIDTH, +DART_INNER_WIDTH, -DART_HALF_LENGTH, 1.0}, {0.0, 0.0, 0.0}, DART_COLOR_FLETCH_LEFT	},
										{{-DART_INNER_WIDTH, -DART_INNER_WIDTH, -DART_HALF_LENGTH, 1.0}, {0.0, 0.0, 0.0}, DART_COLOR_FLETCH_LEFT	},
										{{-DART_OUTER_WIDTH,  0.0,              -DART_HALF_LENGTH, 1.0}, {0.0, 1.0, 0.0}, DART_COLOR_FLETCH_LEFT	},
										{{ 0.0,               0.0,              +DART_HALF_LENGTH, 1.0}, {0.0, 1.0, 0.0}, DART_COLOR_FLETCH_LEFT	},

										//	right fletch
										{{+DART_INNER_WIDTH, -DART_INNER_WIDTH, -DART_HALF_LENGTH, 1.0}, {0.0, 0.0, 0.0}, DART_COLOR_FLETCH_RIGHT	},
										{{+DART_INNER_WIDTH, +DART_INNER_WIDTH, -DART_HALF_LENGTH, 1.0}, {0.0, 0.0, 0.0}, DART_COLOR_FLETCH_RIGHT	},
										{{+DART_OUTER_WIDTH,  0.0,              -DART_HALF_LENGTH, 1.0}, {0.0, 1.0, 0.0}, DART_COLOR_FLETCH_RIGHT	},
										{{ 0.0,               0.0,              +DART_HALF_LENGTH, 1.0}, {0.0, 1.0, 0.0}, DART_COLOR_FLETCH_RIGHT	},

										//	bottom fletch
										{{-DART_INNER_WIDTH, -DART_INNER_WIDTH, -DART_HALF_LENGTH, 1.0}, {0.0, 0.0, 0.0}, DART_COLOR_FLETCH_BOTTOM	},
										{{+DART_INNER_WIDTH, -DART_INNER_WIDTH, -DART_HALF_LENGTH, 1.0}, {0.0, 0.0, 0.0}, DART_COLOR_FLETCH_BOTTOM	},
										{{ 0.0,              -DART_OUTER_WIDTH, -DART_HALF_LENGTH, 1.0}, {0.0, 1.0, 0.0}, DART_COLOR_FLETCH_BOTTOM	},
										{{ 0.0,               0.0,              +DART_HALF_LENGTH, 1.0}, {0.0, 1.0, 0.0}, DART_COLOR_FLETCH_BOTTOM	},

										//	top fletch
										{{+DART_INNER_WIDTH, +DART_INNER_WIDTH, -DART_HALF_LENGTH, 1.0}, {0.0, 0.0, 0.0}, DART_COLOR_FLETCH_TOP		},
										{{-DART_INNER_WIDTH, +DART_INNER_WIDTH, -DART_HALF_LENGTH, 1.0}, {0.0, 0.0, 0.0}, DART_COLOR_FLETCH_TOP		},
										{{ 0.0,              +DART_OUTER_WIDTH, -DART_HALF_LENGTH, 1.0}, {0.0, 1.0, 0.0}, DART_COLOR_FLETCH_TOP		},
										{{ 0.0,               0.0,              +DART_HALF_LENGTH, 1.0}, {0.0, 1.0, 0.0}, DART_COLOR_FLETCH_TOP		},
										
										//	tail
										{{-DART_OUTER_WIDTH,  0.0,              -DART_HALF_LENGTH, 1.0}, {0.0, 1.0, 0.0}, DART_COLOR_TAIL			},
										{{-DART_INNER_WIDTH, -DART_INNER_WIDTH, -DART_HALF_LENGTH, 1.0}, {0.0, 0.0, 0.0}, DART_COLOR_TAIL			},
										{{ 0.0,              -DART_OUTER_WIDTH, -DART_HALF_LENGTH, 1.0}, {0.0, 1.0, 0.0}, DART_COLOR_TAIL			},
										{{+DART_INNER_WIDTH, -DART_INNER_WIDTH, -DART_HALF_LENGTH, 1.0}, {0.0, 0.0, 0.0}, DART_COLOR_TAIL			},
										{{+DART_OUTER_WIDTH,  0.0,              -DART_HALF_LENGTH, 1.0}, {0.0, 1.0, 0.0}, DART_COLOR_TAIL			},
										{{+DART_INNER_WIDTH, +DART_INNER_WIDTH, -DART_HALF_LENGTH, 1.0}, {0.0, 0.0, 0.0}, DART_COLOR_TAIL			},
										{{ 0.0,              +DART_OUTER_WIDTH, -DART_HALF_LENGTH, 1.0}, {0.0, 1.0, 0.0}, DART_COLOR_TAIL			},
										{{-DART_INNER_WIDTH, +DART_INNER_WIDTH, -DART_HALF_LENGTH, 1.0}, {0.0, 0.0, 0.0}, DART_COLOR_TAIL			},
									};
	
	const UInt16					theObserverFacets[4*2 + 6][3] =
									{
										//	left fletch
										{ 2,  3,  0},
										{ 2,  1,  3},

										//	right fletch
										{ 6,  7,  4},
										{ 6,  5,  7},

										//	bottom fletch
										{10, 11,  8},
										{10,  9, 11},

										//	top fletch
										{14, 15, 12},
										{14, 13, 15},
										
										//	“transom”
										{16, 23, 17},
										{18, 17, 19},
										{20, 19, 21},
										{22, 21, 23},
										{17, 21, 19},
										{21, 17, 23}
									};

	theMeshSet					= MakeEmptyMeshSet();
	theMeshSet->itsMeshes[0]	= MakeEmptyMesh();
	theMesh						= theMeshSet->itsMeshes[0];

	theMesh->itsNumFacets = BUFFER_LENGTH(theObserverFacets);

	theMesh->itsVertexBuffer = [aDevice
		newBufferWithBytes:	theObserverVertices
		length:				sizeof(theObserverVertices)
		options:			MTLResourceStorageModeShared];

	theMesh->itsIndexBuffer = [aDevice
		newBufferWithBytes:	theObserverFacets
		length:				sizeof(theObserverFacets)
		options:			MTLResourceStorageModeShared];

	theMesh->itsCubeMapFlag = false;
	
	return theMeshSet;
}


static Mesh *MakeMeshFromArrays(
	id<MTLDevice>	aDevice,
	unsigned int	aNumMeshVertices,
	double			(*someMeshVertexPositions)[4],
	double			(*someMeshVertexTexCoords)[3],
	double			(*someMeshVertexColors)[4],
	unsigned int	aNumMeshFacets,
	unsigned int	(*someMeshFacets)[3],
	bool			aCubeMapFlag)
{
	Mesh					*theMesh;
	unsigned int			theNumRequiredVertexBufferBytes,
							theNumRequiredIndexBufferBytes;
	CurvedSpacesVertexData	*theVertexBufferData;
	UInt16					(*theIndexBufferData)[3];
	unsigned int			i,
							j;

	//	Create a new empty Mesh.
	theMesh = MakeEmptyMesh();
	
	//	Record the number of mesh faces.
	theMesh->itsNumFacets = aNumMeshFacets;

	//	Allocate the Metal vertex buffer.
	theNumRequiredVertexBufferBytes	= aNumMeshVertices * sizeof(CurvedSpacesVertexData);
	theMesh->itsVertexBuffer = [aDevice
		newBufferWithLength:	theNumRequiredVertexBufferBytes
		options:				MTLResourceStorageModeShared];

	//	Allocate the Metal index buffer.
	theNumRequiredIndexBufferBytes	= aNumMeshFacets * sizeof(UInt16 [3]);
	theMesh->itsIndexBuffer = [aDevice
		newBufferWithLength:	theNumRequiredIndexBufferBytes
		options:				MTLResourceStorageModeShared];

	//	Copy the mesh data into the Metal buffers.

	theVertexBufferData = (CurvedSpacesVertexData *) [theMesh->itsVertexBuffer contents];
	for (i = 0; i < aNumMeshVertices; i++)
	{
		for (j = 0; j < 4; j++)
			theVertexBufferData[i].pos[j] = someMeshVertexPositions[i][j];

		for (j = 0; j < 3; j++)
			theVertexBufferData[i].tex[j] = someMeshVertexTexCoords[i][j];

		for (j = 0; j < 4; j++)
			theVertexBufferData[i].col[j] = someMeshVertexColors[i][j];
	}

	theIndexBufferData = (UInt16 (*)[3]) [theMesh->itsIndexBuffer contents];
	for (i = 0; i < aNumMeshFacets; i++)
		for (j = 0; j < 3; j++)
			theIndexBufferData[i][j] = someMeshFacets[i][j];

	//	Does the mesh expect a cube map texture?
	theMesh->itsCubeMapFlag = aCubeMapFlag;
	
	return theMesh;
}


static unsigned int GetNumLevelsOfDetail(
	MeshSet	*aMeshSet)
{
	unsigned int	theNumLevelsOfDetail,
					theLevel,
					theCoarserLevel;

	//	How many levels of detail does aMeshSet contain?
	
	theNumLevelsOfDetail = 0;

	if (aMeshSet != nil)
	{
		for (theLevel = 0; theLevel < MAX_NUM_LOD_LEVELS; theLevel++)
		{
			if (aMeshSet->itsMeshes[theLevel] != nil)
			{
				theNumLevelsOfDetail++;
			}
			else
			{
				//	This level isn't present,
				//	so no coarser levels should be present either.
				for (theCoarserLevel = theLevel + 1; theCoarserLevel < MAX_NUM_LOD_LEVELS; theCoarserLevel++)
				{
					GEOMETRY_GAMES_ASSERT(
						aMeshSet->itsMeshes[theCoarserLevel] == nil,
						"A coarser mesh level-of-detail is present when some finer level-of-detail is not.");
				}
			}
		}
	}
	
	return theNumLevelsOfDetail;
}

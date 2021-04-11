//	CurvedSpacesRenderer.h
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt


#import "GeometryGamesRenderer.h"

@interface CurvedSpacesRenderer : GeometryGamesRenderer

- (id)initWithLayer:(CAMetalLayer *)aLayer device:(id<MTLDevice>)aDevice
	multisampling:(bool)aMultisamplingFlag depthBuffer:(bool)aDepthBufferFlag stencilBuffer:(bool)aStencilBufferFlag;

- (void)setUpGraphicsWithModelData:(ModelData *)md;
- (void)shutDownGraphicsWithModelData:(ModelData *)md;

- (NSDictionary<NSString *, id> *)prepareInflightDataBuffersAtIndex:(unsigned int)anInflightBufferIndex modelData:(ModelData *)md;
- (NSDictionary<NSString *, id> *)prepareInflightDataBuffersForOffscreenRenderingAtSize:(CGSize)anImageSize modelData:(ModelData *)md;
- (bool)wantsClearWithModelData:(ModelData *)md;
- (ColorP3Linear)clearColorWithModelData:(ModelData *)md;
- (void)encodeCommandsToCommandBuffer:(id<MTLCommandBuffer>)aCommandBuffer
		withRenderPassDescriptor:(MTLRenderPassDescriptor *)aRenderPassDescriptor
		inflightDataBuffers:(NSDictionary<NSString *, id> *)someInflightDataBuffers
		modelData:(ModelData *)md;

@end

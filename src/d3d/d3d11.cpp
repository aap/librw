#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WITH_D3D
#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "rwd3d.h"
#include "rwd3d11.h"

#include "rwd3dimpl.h"

#define PLUGIN_ID 2

namespace rw
{
	namespace d3d11
	{
		using namespace d3d;

		// TODO: move to header, but not as #define
	#ifndef RW_D3D11
		static VertexElement _d3ddec_end = { 0xFF,0,0,0,0,0 };
	#define D3DDECL_END() _d3ddec_end
	#endif

	#define NUMDECLELT 12

		static void*
			driverOpen( void* o, int32, int32 )
		{
		#ifdef RW_D3D11
			// createDefaultShaders();
		#endif
			engine->driver[ PLATFORM_D3D11 ]->defaultPipeline = makeDefaultPipeline();

			engine->driver[ PLATFORM_D3D11 ]->rasterNativeOffset = nativeRasterOffset;
			engine->driver[ PLATFORM_D3D11 ]->rasterCreate = d3d11::rasterCreate;
			engine->driver[ PLATFORM_D3D11 ]->rasterLock = d3d11::rasterLock;
			engine->driver[ PLATFORM_D3D11 ]->rasterUnlock = d3d11::rasterUnlock;
			engine->driver[ PLATFORM_D3D11 ]->rasterNumLevels = d3d11::rasterNumLevels;
			engine->driver[ PLATFORM_D3D11 ]->imageFindRasterFormat = d3d11::imageFindRasterFormat;
			engine->driver[ PLATFORM_D3D11 ]->rasterFromImage = d3d11::rasterFromImage;
			engine->driver[ PLATFORM_D3D11 ]->rasterToImage = d3d11::rasterToImage;
			return o;
		}

		static void*
			driverClose( void* o, int32, int32 )
		{
		#ifdef RW_D3D11
			// destroyDefaultShaders();
		#endif
			return o;
		}

		void
			registerPlatformPlugins( void )
		{
			Driver::registerPlugin( PLATFORM_D3D11, 0, PLATFORM_D3D11,
									driverOpen, driverClose );
			// shared between D3D8, 9 and 11
			if( nativeRasterOffset == 0 )
				registerNativeRaster();
		}

		void*
			createVertexDeclaration( VertexElement* elements )
		{
		#ifdef RW_D3D11
			// TODO: Implement D3D11 vertex declaration creation
			return nil;
		#else
			int n = 0;
			VertexElement* e = ( VertexElement* )elements;
			while( e[ n++ ].stream != 0xFF )
				;
			e = rwNewT( VertexElement, n, MEMDUR_EVENT | ID_DRIVER );
			memcpy( e, elements, n * sizeof( VertexElement ) );
			return e;
		#endif
		}

		void
			destroyVertexDeclaration( void* declaration )
		{
		#ifdef RW_D3D11
			// TODO: Implement D3D11 vertex declaration destruction
		#else
			rwFree( declaration );
		#endif
		}

		uint32
			getDeclaration( void* declaration, VertexElement* elements )
		{
		#ifdef RW_D3D11
			// TODO: Implement D3D11 vertex declaration retrieval
			return 0;
		#else
			int n = 0;
			VertexElement* e = ( VertexElement* )declaration;
			while( e[ n++ ].stream != 0xFF )
				;
			if( elements )
				memcpy( elements, declaration, n * sizeof( VertexElement ) );
			return n;
		#endif
		}

		void
			freeInstanceData( Geometry* geometry )
		{
			if( geometry->instData == nil ||
				geometry->instData->platform != PLATFORM_D3D11 )
				return;
			InstanceDataHeader* header =
				( InstanceDataHeader* )geometry->instData;
			geometry->instData = nil;
			destroyVertexDeclaration( header->vertexDeclaration );
			destroyIndexBuffer( header->indexBuffer );
			destroyVertexBuffer( header->vertexStream[ 0 ].vertexBuffer );
			destroyVertexBuffer( header->vertexStream[ 1 ].vertexBuffer );
			rwFree( header->inst );
			rwFree( header );
			return;
		}


		void*
			destroyNativeData( void* object, int32, int32 )
		{
			freeInstanceData( ( Geometry* )object );
			return object;
		}

		Stream*
			readNativeData( Stream* stream, int32, void* object, int32, int32 )
		{
			// TODO: Implement D3D11 native data reading
			return stream;
		}

		Stream*
			writeNativeData( Stream* stream, int32 len, void* object, int32, int32 )
		{
			// TODO: Implement D3D11 native data writing
			return stream;
		}

		int32
			getSizeNativeData( void* object, int32, int32 )
		{
			// TODO: Implement D3D11 native data size calculation
			return 0;
		}

		void
			registerNativeDataPlugin( void )
		{
			Geometry::registerPlugin( 0, ID_NATIVEDATA,
									  nil, destroyNativeData, nil );
			Geometry::registerPluginStream( ID_NATIVEDATA,
											readNativeData,
											writeNativeData,
											getSizeNativeData );
		}

		static InstanceDataHeader*
			instanceMesh( rw::ObjPipeline* rwpipe, Geometry* geo )
		{
			( void )rwpipe;

		#ifdef RW_D3D11
			InstanceDataHeader* header = rwNewT( InstanceDataHeader, 1, MEMDUR_EVENT | ID_GEOMETRY );
			MeshHeader* meshh = geo->meshHeader;
			header->platform = PLATFORM_D3D11;

			header->serialNumber = meshh->serialNum;
			header->numMeshes = meshh->numMeshes;
			header->primType = meshh->flags == 1 ?
				D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP :
				D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			header->useOffsets = 0;
			header->vertexDeclaration = nil;
			header->totalNumVertex = geo->numVertices;
			header->totalNumIndex = meshh->totalIndices;
			header->inst = rwNewT( InstanceData, header->numMeshes, MEMDUR_EVENT | ID_GEOMETRY );

			header->indexBuffer = createIndexBuffer( header->totalNumIndex * 2, false );
			uint16* indices = lockIndices( header->indexBuffer, 0, 0, 0 );
			if( indices == nil )
			{
				destroyIndexBuffer( header->indexBuffer );
				rwFree( header->inst );
				rwFree( header );
				return nil;
			}

			InstanceData* inst = header->inst;
			Mesh* mesh = meshh->getMeshes();
			uint32 startindex = 0;
			for( uint32 i = 0; i < header->numMeshes; i++ )
			{
				findMinVertAndNumVertices( mesh->indices, mesh->numIndices,
										   &inst->minVert, ( int32* )&inst->numVertices );
				inst->numIndex = mesh->numIndices;
				inst->material = mesh->material;
				inst->vertexAlpha = 0;
				inst->vertexShader = nil;
				inst->baseIndex = inst->minVert;
				inst->startIndex = startindex;
				inst->numPrimitives = header->primType == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP ?
					inst->numIndex - 2 : inst->numIndex / 3;
				if( inst->minVert == 0 )
					memcpy( &indices[ inst->startIndex ], mesh->indices,
							inst->numIndex * sizeof( uint16 ) );
				else
					for( uint32 j = 0; j < inst->numIndex; j++ )
						indices[ inst->startIndex + j ] = mesh->indices[ j ] - inst->minVert;
				startindex += inst->numIndex;
				mesh++;
				inst++;
			}
			unlockIndices( header->indexBuffer );

			memset( &header->vertexStream, 0, sizeof( header->vertexStream ) );
			return header;
		#else
			( void )geo;
			return nil;
		#endif
		}

		static void
			instance( rw::ObjPipeline* rwpipe, Atomic* atomic )
		{
			ObjPipeline* pipe = ( ObjPipeline* )rwpipe;
			Geometry* geo = atomic->geometry;
			// Native geometry belongs to another platform and cannot be reinstanced
			// until D3D11 native-data loading is implemented.
			if( geo->flags & Geometry::NATIVE )
				return;

			InstanceDataHeader* header = ( InstanceDataHeader* )geo->instData;
			if( header )
			{
				assert( header->platform == PLATFORM_D3D11 );
				if( header->serialNumber != geo->meshHeader->serialNum )
					freeInstanceData( geo );
			}

			if( geo->instData == nil )
			{
				geo->instData = instanceMesh( rwpipe, geo );
				if( geo->instData && pipe->instanceCB )
					pipe->instanceCB( geo, ( InstanceDataHeader* )geo->instData, 0 );
			}
			else if( geo->lockedSinceInst && pipe->instanceCB )
				pipe->instanceCB( geo, ( InstanceDataHeader* )geo->instData, 1 );

			geo->lockedSinceInst = 0;
		}

		static void
			uninstance( rw::ObjPipeline* rwpipe, Atomic* atomic )
		{
			// TODO: Implement D3D11 uninstancing
		}

		static void
			render( rw::ObjPipeline* rwpipe, Atomic* atomic )
		{
			ObjPipeline* pipe = ( ObjPipeline* )rwpipe;
			Geometry* geo = atomic->geometry;
			pipe->instance( atomic );
			if( geo->instData == nil )
				return;
			assert( geo->instData->platform == PLATFORM_D3D11 );
			if( pipe->renderCB )
				pipe->renderCB( atomic, ( InstanceDataHeader* )geo->instData );
		}

		void
			ObjPipeline::init( void )
		{
			this->rw::ObjPipeline::init( PLATFORM_D3D11 );
			this->impl.instance = d3d11::instance;
			this->impl.uninstance = d3d11::uninstance;
			this->impl.render = d3d11::render;
			this->instanceCB = nil;
			this->uninstanceCB = nil;
			this->renderCB = nil;
		}

		ObjPipeline*
			ObjPipeline::create( void )
		{
			ObjPipeline* pipe = rwNewT( ObjPipeline, 1, MEMDUR_GLOBAL );
			pipe->init();
			return pipe;
		}

		void
			defaultInstanceCB( Geometry* geo, InstanceDataHeader* header, bool32 reinstance )
		{
			VertexStream* stream = &header->vertexStream[ 0 ];
			if( geo->numVertices <= 0 || geo->morphTargets == nil ||
				geo->morphTargets[ 0 ].vertices == nil )
				return;

			if( !reinstance )
			{
				stream->offset = 0;
				stream->stride = sizeof( DefaultVertex );
				stream->managed = 1;
				stream->geometryFlags = 0;
				stream->dynamicLock = 0;
				stream->vertexBuffer = createVertexBuffer(
					header->totalNumVertex * stream->stride, 0, false );
			}
			if( stream->vertexBuffer == nil )
				return;

			DefaultVertex* vertices = ( DefaultVertex* )lockVertices( stream->vertexBuffer,
																	  0, 0, 0 );
			if( vertices == nil )
				return;

			bool isPrelit = (geo->flags & Geometry::PRELIT) != 0 && geo->colors;
			bool hasNormals = (geo->flags & Geometry::NORMALS) != 0 &&
				geo->morphTargets[ 0 ].normals;
			bool hasTexCoords = geo->numTexCoordSets > 0 && geo->texCoords[ 0 ];
			V3d defaultNormal = { 0.0f, 0.0f, 1.0f };
			RGBA white = { 255, 255, 255, 255 };
			TexCoords zeroTexCoord = { 0.0f, 0.0f };
			for( uint32 i = 0; i < header->totalNumVertex; i++ )
			{
				vertices[ i ].position = geo->morphTargets[ 0 ].vertices[ i ];
				vertices[ i ].normal = hasNormals ? geo->morphTargets[ 0 ].normals[ i ] : defaultNormal;
				vertices[ i ].color = isPrelit ? geo->colors[ i ] : white;
				vertices[ i ].texcoord = hasTexCoords ? geo->texCoords[ 0 ][ i ] : zeroTexCoord;
			}
			unlockVertices( stream->vertexBuffer );

			InstanceData* inst = header->inst;
			for( uint32 i = 0; i < header->numMeshes; i++, inst++ )
			{
				inst->vertexAlpha = 0;
				if( isPrelit )
					for( uint32 j = 0; j < inst->numVertices; j++ )
						if( geo->colors[ inst->minVert + j ].alpha != 255 )
						{
							inst->vertexAlpha = 1;
							break;
						}
			}
		}

		void
			defaultUninstanceCB( Geometry* geo, InstanceDataHeader* header )
		{
			// TODO: Implement D3D11 default uninstance callback
		}

		ObjPipeline*
			makeDefaultPipeline( void )
		{
			ObjPipeline* pipe = ObjPipeline::create();
			pipe->instanceCB = defaultInstanceCB;
			pipe->uninstanceCB = defaultUninstanceCB;
			pipe->renderCB = defaultRenderCB_Shader;
			return pipe;
		}

		void
			defaultRenderCB_Fix( Atomic* atomic, InstanceDataHeader* header )
		{
			// TODO: Implement D3D11 default render callback (fixed function)
		}

		// Native Texture and Raster

		Texture*
			readNativeTexture( Stream* stream )
		{
			// TODO: Implement D3D11 native texture reading
			return nil;
		}

		void
			writeNativeTexture( Texture* tex, Stream* stream )
		{
			// TODO: Implement D3D11 native texture writing
		}

		uint32
			getSizeNativeTexture( Texture* tex )
		{
			// TODO: Implement D3D11 native texture size calculation
			return 0;
		}

	}
}

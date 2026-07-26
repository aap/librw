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
			ASSERTLITTLE;
			Geometry* geometry = ( Geometry* )object;
			if( !findChunk( stream, ID_STRUCT, nil, nil ) )
			{
				RWERROR( (ERR_CHUNK, "STRUCT") );
				return nil;
			}

			uint32 platform = stream->readU32();
			if( platform != PLATFORM_D3D11 )
			{
				RWERROR( (ERR_PLATFORM, platform) );
				return nil;
			}

			InstanceDataHeader* header = rwNewT( InstanceDataHeader, 1,
												 MEMDUR_EVENT | ID_GEOMETRY );
			memset( header, 0, sizeof( *header ) );
			geometry->instData = header;
			header->platform = PLATFORM_D3D11;
			uint8* data = nil;

			{
				int32 size = stream->readI32();
				if( size < 64 )
					goto fail;
				data = rwNewT( uint8, size,
							   MEMDUR_FUNCTION | ID_GEOMETRY );
				if( stream->read8( data, size ) != ( uint32 )size )
					goto fail;

				uint8* p = data;
				header->serialNumber = *( uint32* )p; p += 4;
				header->numMeshes = *( uint32* )p; p += 4;
				if( header->numMeshes >
					( uint32 )(size - 64) / 36 )
					goto fail;
				p += 4; // index buffer
				header->primType = *( uint32* )p; p += 4;
				p += 16 * 2;
				header->useOffsets = *( bool32* )p; p += 4;
				p += 4; // vertex declaration
				header->totalNumIndex = *( uint32* )p; p += 4;
				header->totalNumVertex = *( uint32* )p; p += 4;
				if( geometry->meshHeader == nil ||
					header->numMeshes != geometry->meshHeader->numMeshes ||
					header->totalNumIndex != geometry->meshHeader->totalIndices ||
					header->totalNumVertex != ( uint32 )geometry->numVertices )
					goto fail;
				header->inst = rwNewT( InstanceData, header->numMeshes,
									   MEMDUR_EVENT | ID_GEOMETRY );

				InstanceData* inst = header->inst;
				for( uint32 i = 0; i < header->numMeshes; i++, inst++ )
				{
					inst->numIndex = *( uint32* )p; p += 4;
					inst->minVert = *( uint32* )p; p += 4;
					uint32 matid = *( uint32* )p; p += 4;
					if( matid >= ( uint32 )geometry->matList.numMaterials )
						goto fail;
					inst->material = geometry->matList.materials[ matid ];
					inst->vertexAlpha = *( bool32* )p; p += 4;
					inst->vertexShader = nil; p += 4;
					inst->baseIndex = *( uint32* )p; p += 4;
					inst->numVertices = *( uint32* )p; p += 4;
					inst->startIndex = *( uint32* )p; p += 4;
					inst->numPrimitives = *( uint32* )p; p += 4;
					if( inst->startIndex > header->totalNumIndex ||
						inst->numIndex >
						header->totalNumIndex - inst->startIndex ||
						inst->minVert > header->totalNumVertex ||
						inst->numVertices >
						header->totalNumVertex - inst->minVert )
						goto fail;
				}
				rwFree( data );
				data = nil;
			}

			{
				uint32 numDeclarations = stream->readU32();
				if( numDeclarations != 0 )
					goto fail;

				header->indexBuffer = createIndexBuffer(
					header->totalNumIndex * sizeof( uint16 ), false );
				uint16* indices = lockIndices( header->indexBuffer, 0, 0, 0 );
				if( indices == nil )
					goto fail;
				uint32 indexDataSize =
					header->totalNumIndex * sizeof( uint16 );
				if( stream->read8( indices, indexDataSize ) != indexDataSize )
				{
					unlockIndices( header->indexBuffer );
					goto fail;
				}
				unlockIndices( header->indexBuffer );

				for( int32 i = 0; i < 2; i++ )
				{
					uint8 streamData[ 16 ];
					if( stream->read8( streamData, sizeof( streamData ) ) !=
						sizeof( streamData ) )
						goto fail;
					uint8* p = streamData;

					bool32 hasVertexBuffer = *( uint32* )p != 0; p += 4;
					VertexStream* vertexStream = &header->vertexStream[ i ];
					vertexStream->offset = *( uint32* )p; p += 4;
					vertexStream->stride = *( uint32* )p; p += 4;
					vertexStream->geometryFlags = *( uint16* )p; p += 2;
					vertexStream->managed = *p++;
					vertexStream->dynamicLock = *p++;

					if( !hasVertexBuffer )
						continue;

					if( vertexStream->stride == 0 ||
						header->totalNumVertex >
						0xFFFFFFFFu / vertexStream->stride )
						goto fail;
					uint32 vertexDataSize =
						vertexStream->stride * header->totalNumVertex;
					vertexStream->vertexBuffer = createVertexBuffer(
						vertexDataSize, 0, false );
					uint8* vertices = lockVertices(
						vertexStream->vertexBuffer, 0, 0, 0 );
					if( vertices == nil )
						goto fail;
					if( stream->read8( vertices, vertexDataSize ) !=
						vertexDataSize )
					{
						unlockVertices( vertexStream->vertexBuffer );
						goto fail;
					}
					unlockVertices( vertexStream->vertexBuffer );
				}
			}

			return stream;

fail:
			if( data )
				rwFree( data );
			freeInstanceData( geometry );
			return nil;
		}

		Stream*
			writeNativeData( Stream* stream, int32 len, void* object, int32, int32 )
		{
			ASSERTLITTLE;
			Geometry* geometry = ( Geometry* )object;
			writeChunkHeader( stream, ID_STRUCT, len - 12 );
			if( geometry->instData == nil ||
				geometry->instData->platform != PLATFORM_D3D11 )
				return stream;

			stream->writeU32( PLATFORM_D3D11 );

			InstanceDataHeader* header = ( InstanceDataHeader* )geometry->instData;
			int32 size = 64 + header->numMeshes * 36;
			uint8* data = rwNewT( uint8, size,
								  MEMDUR_FUNCTION | ID_GEOMETRY );
			stream->writeI32( size );
			memset( data, 0, size );

			uint8* p = data;
			*( uint32* )p = header->serialNumber; p += 4;
			*( uint32* )p = header->numMeshes; p += 4;
			p += 4; // index buffer
			*( uint32* )p = header->primType; p += 4;
			p += 16 * 2;
			*( bool32* )p = header->useOffsets; p += 4;
			p += 4; // vertex declaration
			*( uint32* )p = header->totalNumIndex; p += 4;
			*( uint32* )p = header->totalNumVertex; p += 4;

			InstanceData* inst = header->inst;
			for( uint32 i = 0; i < header->numMeshes; i++ )
			{
				*( uint32* )p = inst->numIndex; p += 4;
				*( uint32* )p = inst->minVert; p += 4;
				int32 matid = geometry->matList.findIndex( inst->material );
				*( int32* )p = matid; p += 4;
				*( bool32* )p = inst->vertexAlpha; p += 4;
				p += 4; // vertex shader
				*( uint32* )p = inst->baseIndex; p += 4;
				*( uint32* )p = inst->numVertices; p += 4;
				*( uint32* )p = inst->startIndex; p += 4;
				*( uint32* )p = inst->numPrimitives; p += 4;
				inst++;
			}
			stream->write8( data, size );

			// The standard D3D11 pipeline uses the fixed default input layout.
			stream->writeU32( 0 );

			uint16* indices = lockIndices( header->indexBuffer, 0, 0, 0 );
			stream->write8( indices,
							header->totalNumIndex * sizeof( uint16 ) );
			unlockIndices( header->indexBuffer );

			for( int32 i = 0; i < 2; i++ )
			{
				VertexStream* vertexStream = &header->vertexStream[ i ];
				p = data;
				*( uint32* )p =
					vertexStream->vertexBuffer ? 0xBADEAFFE : 0; p += 4;
				*( uint32* )p = vertexStream->offset; p += 4;
				*( uint32* )p = vertexStream->stride; p += 4;
				*( uint16* )p = vertexStream->geometryFlags; p += 2;
				*p++ = vertexStream->managed;
				*p++ = vertexStream->dynamicLock;
				stream->write8( data, 16 );

				if( vertexStream->vertexBuffer == nil )
					continue;

				uint32 vertexDataSize =
					vertexStream->stride * header->totalNumVertex;
				uint8* vertices = lockVertices(
					vertexStream->vertexBuffer, 0, 0, D3DLOCK_NOSYSLOCK );
				stream->write8( vertices, vertexDataSize );
				unlockVertices( vertexStream->vertexBuffer );
			}

			rwFree( data );
			return stream;
		}

		int32
			getSizeNativeData( void* object, int32, int32 )
		{
			Geometry* geometry = ( Geometry* )object;
			if( geometry->instData == nil ||
				geometry->instData->platform != PLATFORM_D3D11 )
				return 0;

			InstanceDataHeader* header =
				( InstanceDataHeader* )geometry->instData;
			int32 size = 12 + 4 + 4 + 64 + header->numMeshes * 36;
			size += 4;
			size += header->totalNumIndex * sizeof( uint16 );
			for( int32 i = 0; i < 2; i++ )
			{
				VertexStream* stream = &header->vertexStream[ i ];
				size += 16;
				if( stream->vertexBuffer )
					size += stream->stride * header->totalNumVertex;
			}
			return size;
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
			// Native geometry already contains D3D11-ready vertex and index data.
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
			ObjPipeline* pipe = ( ObjPipeline* )rwpipe;
			Geometry* geo = atomic->geometry;
			if( (geo->flags & Geometry::NATIVE) == 0 )
				return;

			assert( geo->instData != nil );
			if( geo->instData == nil )
				return;
			assert( geo->instData->platform == PLATFORM_D3D11 );
			if( geo->instData->platform != PLATFORM_D3D11 )
				return;

			geo->numTriangles = geo->meshHeader->guessNumTriangles();
			geo->allocateData();
			geo->allocateMeshes( geo->meshHeader->numMeshes,
								 geo->meshHeader->totalIndices, 0 );

			InstanceDataHeader* header =
				( InstanceDataHeader* )geo->instData;
			uint16* indices = lockIndices( header->indexBuffer, 0, 0, 0 );
			if( indices == nil )
				return;

			InstanceData* inst = header->inst;
			Mesh* mesh = geo->meshHeader->getMeshes();
			for( uint32 i = 0; i < header->numMeshes; i++ )
			{
				if( inst->minVert == 0 )
					memcpy( mesh->indices, &indices[ inst->startIndex ],
							inst->numIndex * sizeof( uint16 ) );
				else
					for( uint32 j = 0; j < inst->numIndex; j++ )
						mesh->indices[ j ] =
						indices[ inst->startIndex + j ] + inst->minVert;
				mesh++;
				inst++;
			}
			unlockIndices( header->indexBuffer );

			assert( pipe->uninstanceCB != nil );
			if( pipe->uninstanceCB == nil )
				return;
			pipe->uninstanceCB( geo, header );
			geo->generateTriangles();
			geo->flags &= ~Geometry::NATIVE;
			destroyNativeData( geo, 0, 0 );
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
			RGBA black = { 0, 0, 0, 255 };
			TexCoords zeroTexCoord = { 0.0f, 0.0f };
			for( uint32 i = 0; i < header->totalNumVertex; i++ )
			{
				vertices[ i ].position = geo->morphTargets[ 0 ].vertices[ i ];
				vertices[ i ].normal = hasNormals ? geo->morphTargets[ 0 ].normals[ i ] : defaultNormal;
				vertices[ i ].color = isPrelit ? geo->colors[ i ] : black;
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
			VertexStream* stream = &header->vertexStream[ 0 ];
			if( stream->vertexBuffer == nil ||
				stream->stride < sizeof( DefaultVertex ) )
				return;

			uint8* vertices = lockVertices( stream->vertexBuffer, 0, 0,
											D3DLOCK_NOSYSLOCK );
			if( vertices == nil )
				return;

			bool hasNormals = (geo->flags & Geometry::NORMALS) != 0 &&
				geo->morphTargets[ 0 ].normals;
			bool isPrelit = (geo->flags & Geometry::PRELIT) != 0 &&
				geo->colors;
			bool hasTexCoords = geo->numTexCoordSets > 0 &&
				geo->texCoords[ 0 ];

			for( uint32 i = 0; i < header->totalNumVertex; i++ )
			{
				DefaultVertex* vertex = ( DefaultVertex* )(
					vertices + stream->offset + i * stream->stride);
				geo->morphTargets[ 0 ].vertices[ i ] = vertex->position;
				if( hasNormals )
					geo->morphTargets[ 0 ].normals[ i ] = vertex->normal;
				if( isPrelit )
					geo->colors[ i ] = vertex->color;
				if( hasTexCoords )
					geo->texCoords[ 0 ][ i ] = vertex->texcoord;
			}

			unlockVertices( stream->vertexBuffer );
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

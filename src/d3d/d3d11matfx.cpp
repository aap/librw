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
#include "../rwanim.h"
#include "../rwengine.h"
#include "../rwrender.h"
#include "../rwplugins.h"
#include "rwd3d.h"
#include "rwd3d11.h"

#ifdef RW_D3D11
#include <d3dcompiler.h>

#include "rwd3dimpl.h"
#include "matfx_env_VS_d11.h"
#include "matfx_env_PS_d11.h"
#endif

namespace rw
{
	namespace d3d11
	{
		using namespace d3d;

	#ifndef RW_D3D11
		void matfxRenderCB_Shader( Atomic* atomic, InstanceDataHeader* header ) {}
	#else

		static ID3D11VertexShader* matfx_env_amb_VS;
		static ID3D11VertexShader* matfx_env_amb_dir_VS;
		static ID3D11VertexShader* matfx_env_all_VS;
		static ID3D11PixelShader* matfx_env_PS;
		static ID3D11PixelShader* matfx_env_tex_PS;
		static ID3D11Buffer* matfxVSConstantBuffer;
		static ID3D11Buffer* matfxPSConstantBuffer;

		struct MatFXVSConstants
		{
			float texMat[ 16 ];
			float colorClamp[ 4 ];
			float envColor[ 4 ];
		};

		struct MatFXPSConstants
		{
			float shininess;
			float disableFBA;
			float unused[ 2 ];
		};

		static MatFXVSConstants matfxVSConstants;
		void destroyMatFXShaders( void );
		void destroyMatFXConstantBuffers( void );

		enum
		{
			VSLOC_texMat = VSLOC_afterLights,
			VSLOC_colorClamp = VSLOC_texMat + 4,
			VSLOC_envColor,

			PSLOC_shininess = 1,
		};

		void
			matfxRender_Default( InstanceDataHeader* header, InstanceData* inst, int32 lightBits )
		{
			Material* m = inst->material;

			setDefaultVertexShader( lightBits );
			SetRenderState( VERTEXALPHA,
				inst->vertexAlpha || m->color.alpha != 255 );
			setTexture( 0, m->texture );
			setDefaultPixelShader();
			drawInst( header, inst );
		}

		static Frame* lastEnvFrame;

		static RawMatrix normal2texcoord = {
			{ 0.5f,  0.0f, 0.0f }, 0.0f,
			{ 0.0f, -0.5f, 0.0f }, 0.0f,
			{ 0.0f,  0.0f, 1.0f }, 0.0f,
			{ 0.5f,  0.5f, 0.0f }, 1.0f
		};

		void
			uploadEnvMatrix( Frame* frame )
		{
			Matrix invMat;
			if( frame == nil )
				frame = engine->currentCamera->getFrame();

			RawMatrix envMtx, invMtx;
			Matrix::invert( &invMat, frame->getLTM() );
			convMatrix( &invMtx, &invMat );
			invMtx.pos.set( 0.0f, 0.0f, 0.0f );
			float uscale = fabs( normal2texcoord.right.x );
			normal2texcoord.right.x = MatFX::envMapFlipU ? -uscale : uscale;
			RawMatrix::mult( &envMtx, &invMtx, &normal2texcoord );

			memcpy( matfxVSConstants.texMat, &envMtx, sizeof( envMtx ) );
			d3d11Globals.context->UpdateSubresource( matfxVSConstantBuffer, 0, nil, &matfxVSConstants, 0, 0 );
			d3d11Globals.context->VSSetConstantBuffers( VSlotMatFX, 1, &matfxVSConstantBuffer );
		}

		void
			matfxRender_EnvMap( InstanceDataHeader* header, InstanceData* inst, int32 lightBits, MatFX::Env* env )
		{
			Material* m = inst->material;

			if( env->tex == nil || env->coefficient == 0.0f )
			{
				matfxRender_Default( header, inst, lightBits );
				return;
			}

			setTexture( 1, env->tex );
			uploadEnvMatrix( env->frame );

			SetRenderState( SRCBLEND, BLENDONE );

			MatFXPSConstants fxparams;
			fxparams.shininess = env->coefficient;
			fxparams.disableFBA = env->fbAlpha ? 0.0f : 1.0f;
			fxparams.unused[ 0 ] = 0.0f;
			fxparams.unused[ 1 ] = 0.0f;
			d3d11Globals.context->UpdateSubresource(
				matfxPSConstantBuffer, 0, nil, &fxparams, 0, 0 );
			d3d11Globals.context->PSSetConstantBuffers(
				PSSlotMatFX, 1, &matfxPSConstantBuffer );

			float clamp = MatFX::envMapApplyLight ? 0.0f : 1.0f;
			for( int32 i = 0; i < 4; i++ )
				matfxVSConstants.colorClamp[ i ] = clamp;

			RGBAf envColor;
			if( MatFX::envMapUseMatColor )
				convColor( &envColor, &m->color );
			else
				convColor( &envColor, &MatFX::envMapColor );
			memcpy( matfxVSConstants.envColor, &envColor,
				sizeof( envColor ) );
			d3d11Globals.context->UpdateSubresource(
				matfxVSConstantBuffer, 0, nil, &matfxVSConstants, 0, 0 );
			d3d11Globals.context->VSSetConstantBuffers(
				VSlotMatFX, 1, &matfxVSConstantBuffer );

			if( (lightBits & VSLIGHT_MASK) == 0 )
				setVertexShader( matfx_env_amb_VS );
			else if( (lightBits & VSLIGHT_MASK) == VSLIGHT_DIRECT )
				setVertexShader( matfx_env_amb_dir_VS );
			else
				setVertexShader( matfx_env_all_VS );

			bool32 texAlpha = GETD3DRASTEREXT( env->tex->raster )->hasAlpha;
			setTexture( 0, m->texture );
			if( m->texture )
				setPixelShader( matfx_env_tex_PS );
			else
				setPixelShader( matfx_env_PS );

			SetRenderState( VERTEXALPHA,
				texAlpha || inst->vertexAlpha || m->color.alpha != 255 );
			drawInst( header, inst );

			SetRenderState( SRCBLEND, BLENDSRCALPHA );
		}

		void
			matfxRenderCB_Shader( Atomic* atomic, InstanceDataHeader* header )
		{
			if( !setStreamSource( 0,
				header->vertexStream[ 0 ].vertexBuffer, 0,
				header->vertexStream[ 0 ].stride ) )
				return;
			if( !setIndices( header->indexBuffer ) )
				return;
			if( !setDefaultVertexDeclaration() )
				return;

			d3d11Globals.context->IASetPrimitiveTopology(
				(D3D11_PRIMITIVE_TOPOLOGY)header->primType );

			lastEnvFrame = nil;

			int32 lightBits = lightingCB_Shader( atomic );
			if( !uploadDefaultMatrices( atomic->getFrame()->getLTM() ) )
				return;

			uint32 flags = atomic->geometry->flags;
			bool32 normals = (flags & Geometry::NORMALS) != 0;
			static const RGBA white = { 255, 255, 255, 255 };

			InstanceData* inst = header->inst;
			for( uint32 i = 0; i < header->numMeshes; i++, inst++ )
			{
				Material* m = inst->material;
				uploadDefaultMaterial(
					(flags & Geometry::MODULATE) ? m->color : white,
					m->surfaceProps );

				MatFX* matfx = MatFX::get( m );
				if( matfx == nil )
					matfxRender_Default( header, inst, lightBits );
				else switch( matfx->type )
				{
				case MatFX::ENVMAP:
					if( normals )
						matfxRender_EnvMap(
							header, inst, lightBits, &matfx->fx[ 0 ].env );
					else
						matfxRender_Default( header, inst, lightBits );
					break;
				case MatFX::NOTHING:
				case MatFX::BUMPMAP:
				case MatFX::BUMPENVMAP:
				case MatFX::DUAL:
				case MatFX::UVTRANSFORM:
				case MatFX::DUALUVTRANSFORM:
					matfxRender_Default( header, inst, lightBits );
					break;
				}
			}
			setTexture( 1, nil );
		}

		static ID3DBlob*
			compileMatFXShader( const char* source, const char* target,
								const D3D_SHADER_MACRO* defines = nil )
		{
			ID3DBlob* shader = nil;
			ID3DBlob* errors = nil;
			HRESULT hr = D3DCompile( source, strlen( source ), nil, defines, nil,
									 "main", target, 0, 0, &shader, &errors );
			if( errors )
			{
				fprintf( stderr, "%s\n", ( const char* )errors->GetBufferPointer() );
				errors->Release();
			}
			if( FAILED( hr ) )
				return nil;
			return shader;
		}

		void
			createMatFXShaders( void )
		{
			static const D3D_SHADER_MACRO ambDirDefines[] = {
				{ "DIRECTIONALS", "1" },
				{ nil, nil }
			};
			static const D3D_SHADER_MACRO allDefines[] = {
				{ "DIRECTIONALS", "1" },
				{ "POINTLIGHTS", "1" },
				{ "SPOTLIGHTS", "1" },
				{ nil, nil }
			};
			static const D3D_SHADER_MACRO texDefines[] = {
				{ "TEX", "1" },
				{ nil, nil }
			};
			ID3DBlob* ambBlob = compileMatFXShader(
				matfx_env_VS_d11_source, "vs_4_0" );
			ID3DBlob* ambDirBlob = compileMatFXShader(
				matfx_env_VS_d11_source, "vs_4_0", ambDirDefines );
			ID3DBlob* allBlob = compileMatFXShader(
				matfx_env_VS_d11_source, "vs_4_0", allDefines );
			ID3DBlob* psBlob = compileMatFXShader(
				matfx_env_PS_d11_source, "ps_4_0" );
			ID3DBlob* texPSBlob = compileMatFXShader(
				matfx_env_PS_d11_source, "ps_4_0", texDefines );
			if( ambBlob == nil || ambDirBlob == nil || allBlob == nil ||
				psBlob == nil || texPSBlob == nil )
				goto fail;

			if( FAILED( d3d11Globals.d3ddevice->CreateVertexShader(
				ambBlob->GetBufferPointer(), ambBlob->GetBufferSize(), nil,
				&matfx_env_amb_VS ) ) )
				goto fail;
			d3d11Globals.numVertexShaders++;
			if( FAILED( d3d11Globals.d3ddevice->CreateVertexShader(
				ambDirBlob->GetBufferPointer(), ambDirBlob->GetBufferSize(), nil,
				&matfx_env_amb_dir_VS ) ) )
				goto fail;
			d3d11Globals.numVertexShaders++;
			if( FAILED( d3d11Globals.d3ddevice->CreateVertexShader(
				allBlob->GetBufferPointer(), allBlob->GetBufferSize(), nil,
				&matfx_env_all_VS ) ) )
				goto fail;
			d3d11Globals.numVertexShaders++;
			if( FAILED( d3d11Globals.d3ddevice->CreatePixelShader(
				psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nil,
				&matfx_env_PS ) ) )
				goto fail;
			d3d11Globals.numPixelShaders++;
			if( FAILED( d3d11Globals.d3ddevice->CreatePixelShader(
				texPSBlob->GetBufferPointer(), texPSBlob->GetBufferSize(), nil,
				&matfx_env_tex_PS ) ) )
				goto fail;
			d3d11Globals.numPixelShaders++;

			ambBlob->Release();
			ambDirBlob->Release();
			allBlob->Release();
			psBlob->Release();
			texPSBlob->Release();
			return;

fail:
			if( ambBlob ) ambBlob->Release();
			if( ambDirBlob ) ambDirBlob->Release();
			if( allBlob ) allBlob->Release();
			if( psBlob ) psBlob->Release();
			if( texPSBlob ) texPSBlob->Release();
			destroyMatFXShaders();
			assert( !"Failed to create D3D11 MatFX shaders" );
		}

		void
			destroyMatFXShaders( void )
		{
			if( matfx_env_tex_PS )
			{
				clearPixelShader( matfx_env_tex_PS );
				matfx_env_tex_PS->Release();
				matfx_env_tex_PS = nil;
				d3d11Globals.numPixelShaders--;
			}
			if( matfx_env_PS )
			{
				clearPixelShader( matfx_env_PS );
				matfx_env_PS->Release();
				matfx_env_PS = nil;
				d3d11Globals.numPixelShaders--;
			}
			if( matfx_env_all_VS )
			{
				clearVertexShader( matfx_env_all_VS );
				matfx_env_all_VS->Release();
				matfx_env_all_VS = nil;
				d3d11Globals.numVertexShaders--;
			}
			if( matfx_env_amb_dir_VS )
			{
				clearVertexShader( matfx_env_amb_dir_VS );
				matfx_env_amb_dir_VS->Release();
				matfx_env_amb_dir_VS = nil;
				d3d11Globals.numVertexShaders--;
			}
			if( matfx_env_amb_VS )
			{
				clearVertexShader( matfx_env_amb_VS );
				matfx_env_amb_VS->Release();
				matfx_env_amb_VS = nil;
				d3d11Globals.numVertexShaders--;
			}
		}

		void
			createMatFXConstantBuffers( void )
		{
			memset( &matfxVSConstants, 0, sizeof( matfxVSConstants ) );
			matfxVSConstants.envColor[ 0 ] = 1.0f;
			matfxVSConstants.envColor[ 1 ] = 1.0f;
			matfxVSConstants.envColor[ 2 ] = 1.0f;
			matfxVSConstants.envColor[ 3 ] = 1.0f;

			D3D11_BUFFER_DESC desc;
			memset( &desc, 0, sizeof( desc ) );
			desc.ByteWidth = sizeof( MatFXVSConstants );
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			if( FAILED( d3d11Globals.d3ddevice->CreateBuffer(
				&desc, nil, &matfxVSConstantBuffer ) ) )
				assert( !"Failed to create D3D11 MatFX constant buffers" );

			desc.ByteWidth = sizeof( MatFXPSConstants );
			if( FAILED( d3d11Globals.d3ddevice->CreateBuffer(
				&desc, nil, &matfxPSConstantBuffer ) ) )
				assert( !"Failed to create D3D11 MatFX constant buffers" );
		}

		void
			destroyMatFXConstantBuffers( void )
		{
			ID3D11Buffer* buffer = nil;
			d3d11Globals.context->PSSetConstantBuffers(
				PSSlotMatFX, 1, &buffer );
			matfxPSConstantBuffer->Release();
			matfxPSConstantBuffer = nil;

			d3d11Globals.context->VSSetConstantBuffers(
				VSlotMatFX, 1, &buffer );
			matfxVSConstantBuffer->Release();
			matfxVSConstantBuffer = nil;
		}

	#endif

		static void*
			matfxOpen( void* o, int32, int32 )
		{
		#ifdef RW_D3D11
			createMatFXShaders();
			createMatFXConstantBuffers();
		#endif

			matFXGlobals.pipelines[ PLATFORM_D3D11 ] = makeMatFXPipeline();
			return o;
		}

		static void*
			matfxClose( void* o, int32, int32 )
		{
		#ifdef RW_D3D11
			destroyMatFXConstantBuffers();
			destroyMatFXShaders();
		#endif

			(( ObjPipeline* )matFXGlobals.pipelines[ PLATFORM_D3D11 ])->destroy();
			matFXGlobals.pipelines[ PLATFORM_D3D11 ] = nil;
			return o;
		}

		void
			initMatFX( void )
		{
			Driver::registerPlugin( PLATFORM_D3D11, 0, ID_MATFX,
									matfxOpen, matfxClose );
		}

		ObjPipeline*
			makeMatFXPipeline( void )
		{
			ObjPipeline* pipe = ObjPipeline::create();
			pipe->instanceCB = defaultInstanceCB;
			pipe->uninstanceCB = defaultUninstanceCB;
			pipe->renderCB = matfxRenderCB_Shader;
			pipe->pluginID = ID_MATFX;
			pipe->pluginData = 0;
			return pipe;
		}

	}
}

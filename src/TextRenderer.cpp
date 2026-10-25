#include "TextRenderer.h"
#include "Platform.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <unordered_map>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

using namespace ffglex;

namespace sorteador
{
namespace
{
constexpr int kAtlasSize    = 2048;
constexpr float kBakeSize   = 64.0f;// pixel height glyphs are baked at
constexpr int kSdfPadding   = 8;    // pixels of distance field around each glyph
constexpr int kSdfOnEdge    = 160;  // atlas value exactly on the glyph outline
constexpr float kSdfDistScale = (float)kSdfOnEdge / kSdfPadding;
constexpr float kEdge         = kSdfOnEdge / 255.0f;
constexpr int kFloatsPerVertex = 8;

const char* kVertexShader = R"(#version 410 core
layout( location = 0 ) in vec2 aPosition;
layout( location = 1 ) in vec2 aUV;
layout( location = 2 ) in vec4 aColor;
uniform vec2 uViewport;
out vec2 vUV;
out vec4 vColor;
void main()
{
	// Pixel coordinates with the origin at the top left.
	gl_Position = vec4( aPosition.x / uViewport.x * 2.0 - 1.0, 1.0 - aPosition.y / uViewport.y * 2.0, 0.0, 1.0 );
	vUV         = aUV;
	vColor      = aColor;
}
)";

const char* kFragmentShader = R"(#version 410 core
in vec2 vUV;
in vec4 vColor;
out vec4 fragColor;
uniform sampler2D uAtlas;
uniform float uEdge;
uniform float uOutline;
uniform vec3 uOutlineColor;
void main()
{
	// Negative uvs mark solid rectangles.
	if( vUV.x < 0.0 )
	{
		fragColor = vec4( vColor.rgb * vColor.a, vColor.a );
		return;
	}
	float d    = texture( uAtlas, vUV ).r;
	float aa   = max( fwidth( d ) * 0.75, 0.0001 );
	float fill = smoothstep( uEdge - aa, uEdge + aa, d );
	vec3 color = vColor.rgb;
	float alpha = fill;
	if( uOutline > 0.0 )
	{
		float edge = uEdge - uOutline;
		alpha = smoothstep( edge - aa, edge + aa, d );
		color = mix( uOutlineColor, vColor.rgb, fill );
	}
	alpha *= vColor.a;
	fragColor = vec4( color * alpha, alpha );
}
)";

std::u32string DecodeUtf8( const std::string& s )
{
	std::u32string out;
	out.reserve( s.size() );
	for( size_t i = 0; i < s.size(); )
	{
		unsigned char c = (unsigned char)s[ i ];
		int extra       = c < 0x80 ? 0 : ( c >> 5 ) == 0x6 ? 1 : ( c >> 4 ) == 0xE ? 2 : ( c >> 3 ) == 0x1E ? 3 : -1;
		if( extra < 0 || ( extra > 0 && i + extra >= s.size() ) )
		{
			out.push_back( 0xFFFD );
			++i;
			continue;
		}
		char32_t cp = extra == 0 ? c : ( c & ( 0x3F >> extra ) );
		bool valid  = true;
		for( int k = 1; k <= extra; ++k )
		{
			unsigned char cc = (unsigned char)s[ i + k ];
			if( ( cc & 0xC0 ) != 0x80 )
				valid = false;
			cp = ( cp << 6 ) | ( cc & 0x3F );
		}
		out.push_back( valid ? cp : 0xFFFD );
		i += valid ? extra + 1 : 1;
	}
	return out;
}

std::vector< char32_t > AtlasCodepoints()
{
	std::vector< char32_t > cps;
	for( char32_t c = 32; c < 127; ++c )
		cps.push_back( c );
	for( char32_t c = 160; c < 0x180; ++c )// Latin-1 and Latin Extended-A: every accent used in Portuguese and Spanish
		cps.push_back( c );
	for( char32_t c : { 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x20AC } )
		cps.push_back( c );
	return cps;
}
}//namespace

struct Glyph
{
	float u0, v0, u1, v1;
	float x0, y0;// top left corner relative to the pen position on the baseline, in baked pixels (y down)
	float width, height;
	float advance;
};

struct BakedFont
{
	std::vector< unsigned char > fontData;// stb keeps pointers into this for kerning lookups
	stbtt_fontinfo info{};
	float scale     = 1.0f;
	float capHeight = 0.0f;
	std::unordered_map< char32_t, Glyph > glyphs;
	std::vector< unsigned char > atlas;

	const Glyph* Find( char32_t cp ) const
	{
		auto it = glyphs.find( cp );
		if( it == glyphs.end() )
			it = glyphs.find( U'?' );
		return it == glyphs.end() ? nullptr : &it->second;
	}

	float Kern( char32_t a, char32_t b ) const
	{
		return stbtt_GetCodepointKernAdvance( &info, (int)a, (int)b ) * scale;
	}
};

namespace
{
std::shared_ptr< const BakedFont > Bake( const std::string& path )
{
	auto font = std::make_shared< BakedFont >();
	if( path.empty() || !ReadFile( PathFromUtf8( path ), font->fontData ) )
		return nullptr;
	int offset = stbtt_GetFontOffsetForIndex( font->fontData.data(), 0 );
	if( offset < 0 || !stbtt_InitFont( &font->info, font->fontData.data(), offset ) )
		return nullptr;

	font->scale = stbtt_ScaleForPixelHeight( &font->info, kBakeSize );
	int x0, y0, x1, y1;
	if( stbtt_GetCodepointBox( &font->info, 'H', &x0, &y0, &x1, &y1 ) )
		font->capHeight = y1 * font->scale;
	else
	{
		int ascent, descent, lineGap;
		stbtt_GetFontVMetrics( &font->info, &ascent, &descent, &lineGap );
		font->capHeight = ascent * font->scale * 0.7f;
	}

	// Simple shelf packing: glyphs fill rows left to right, a new row starts when one is full.
	font->atlas.assign( (size_t)kAtlasSize * kAtlasSize, 0 );
	int penX = 1, penY = 1, rowHeight = 0;
	for( char32_t cp : AtlasCodepoints() )
	{
		if( stbtt_FindGlyphIndex( &font->info, (int)cp ) == 0 && cp != U' ' )
			continue;

		int advance, leftBearing;
		stbtt_GetCodepointHMetrics( &font->info, (int)cp, &advance, &leftBearing );
		Glyph glyph{};
		glyph.advance = advance * font->scale;

		int w = 0, h = 0, xoff = 0, yoff = 0;
		unsigned char* sdf = stbtt_GetCodepointSDF( &font->info, font->scale, (int)cp, kSdfPadding, kSdfOnEdge, kSdfDistScale, &w, &h, &xoff, &yoff );
		if( sdf )
		{
			if( penX + w + 1 > kAtlasSize )
			{
				penX = 1;
				penY += rowHeight + 1;
				rowHeight = 0;
			}
			if( penY + h + 1 > kAtlasSize )
			{
				stbtt_FreeSDF( sdf, nullptr );
				break;
			}
			for( int row = 0; row < h; ++row )
				std::copy( sdf + row * w, sdf + row * w + w, font->atlas.begin() + (size_t)( penY + row ) * kAtlasSize + penX );
			stbtt_FreeSDF( sdf, nullptr );

			glyph.u0     = (float)penX / kAtlasSize;
			glyph.v0     = (float)penY / kAtlasSize;
			glyph.u1     = (float)( penX + w ) / kAtlasSize;
			glyph.v1     = (float)( penY + h ) / kAtlasSize;
			glyph.x0     = (float)xoff;
			glyph.y0     = (float)yoff;
			glyph.width  = (float)w;
			glyph.height = (float)h;
			penX += w + 1;
			rowHeight = std::max( rowHeight, h );
		}
		font->glyphs[ cp ] = glyph;
	}
	return font;
}

// Every instance asks for the same font, so each file is only baked once per Resolume session.
std::shared_future< std::shared_ptr< const BakedFont > > LoadCached( const std::string& path )
{
	static std::mutex mutex;
	static std::map< std::string, std::shared_future< std::shared_ptr< const BakedFont > > > cache;
	std::lock_guard< std::mutex > lock( mutex );
	auto it = cache.find( path );
	if( it != cache.end() )
		return it->second;
	auto future   = std::async( std::launch::async, Bake, path ).share();
	cache[ path ] = future;
	return future;
}
}//namespace

TextRenderer::TextRenderer()  = default;
TextRenderer::~TextRenderer() = default;

bool TextRenderer::InitGL()
{
	if( !shader.Compile( kVertexShader, kFragmentShader ) )
		return false;

	glGenVertexArrays( 1, &vao );
	glGenBuffers( 1, &vbo );
	ScopedVAOBinding vaoBinding( vao );
	ScopedVBOBinding vboBinding( vbo );
	const GLsizei stride = kFloatsPerVertex * sizeof( float );
	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0 );
	glEnableVertexAttribArray( 1 );
	glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, stride, (void*)( 2 * sizeof( float ) ) );
	glEnableVertexAttribArray( 2 );
	glVertexAttribPointer( 2, 4, GL_FLOAT, GL_FALSE, stride, (void*)( 4 * sizeof( float ) ) );
	vaoBinding.EndScope();

	glGenTextures( 1, &texture );
	Scoped2DTextureBinding textureBinding( texture );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_R8, kAtlasSize, kAtlasSize, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr );

	// A new GL context needs the atlas uploaded again.
	if( font )
	{
		auto keep = font;
		font      = nullptr;
		pending   = std::async( std::launch::deferred, [ keep ]() { return keep; } ).share();
		hasPending = true;
	}
	return true;
}

void TextRenderer::DeInitGL()
{
	shader.FreeGLResources();
	if( vbo )
		glDeleteBuffers( 1, &vbo );
	if( vao )
		glDeleteVertexArrays( 1, &vao );
	if( texture )
		glDeleteTextures( 1, &texture );
	vbo = vao = texture = 0;
}

void TextRenderer::RequestFont( const std::string& path )
{
	if( path == requestedPath )
		return;
	requestedPath = path;
	pending       = LoadCached( path );
	hasPending    = true;
}

void TextRenderer::PollFont()
{
	if( !hasPending || pending.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::timeout )
		return;
	hasPending                              = false;
	std::shared_ptr< const BakedFont > baked = pending.get();
	if( !baked )
	{
		// The chosen file could not be read: fall back to the system font if nothing is showing yet.
		std::string fallback = DefaultFontPath();
		if( !font && !fallback.empty() && fallback != requestedPath )
		{
			pending    = LoadCached( fallback );
			hasPending = true;
		}
		return;
	}

	font = baked;
	Scoped2DTextureBinding textureBinding( texture );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, kAtlasSize, kAtlasSize, GL_RED, GL_UNSIGNED_BYTE, font->atlas.data() );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
}

float TextRenderer::Measure( const std::string& utf8, float sizePx ) const
{
	if( !font )
		return 0.0f;
	std::u32string text = DecodeUtf8( utf8 );
	float width         = 0.0f;
	for( size_t i = 0; i < text.size(); ++i )
	{
		if( const Glyph* glyph = font->Find( text[ i ] ) )
			width += glyph->advance;
		if( i + 1 < text.size() )
			width += font->Kern( text[ i ], text[ i + 1 ] );
	}
	return width * sizePx / kBakeSize;
}

void TextRenderer::Begin( float width, float height )
{
	PollFont();
	viewportWidth  = std::max( 1.0f, width );
	viewportHeight = std::max( 1.0f, height );
	vertices.clear();
}

void TextRenderer::PushQuad( float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, const float rgba[ 4 ] )
{
	const float corners[ 6 ][ 4 ] = {
		{ x0, y0, u0, v0 }, { x1, y0, u1, v0 }, { x1, y1, u1, v1 },
		{ x0, y0, u0, v0 }, { x1, y1, u1, v1 }, { x0, y1, u0, v1 },
	};
	for( const auto& c : corners )
	{
		vertices.insert( vertices.end(), { c[ 0 ], c[ 1 ], c[ 2 ], c[ 3 ], rgba[ 0 ], rgba[ 1 ], rgba[ 2 ], rgba[ 3 ] } );
	}
}

void TextRenderer::AddRect( float x0, float y0, float x1, float y1, const float rgba[ 4 ] )
{
	PushQuad( x0, y0, x1, y1, -1.0f, -1.0f, -1.0f, -1.0f, rgba );
}

void TextRenderer::AddText( const std::string& utf8, float centerX, float centerY, float sizePx, const float rgba[ 4 ] )
{
	if( !font || rgba[ 3 ] <= 0.0f || sizePx <= 0.0f )
		return;
	const float s       = sizePx / kBakeSize;
	std::u32string text = DecodeUtf8( utf8 );
	float penX          = centerX - Measure( utf8, sizePx ) * 0.5f;
	const float baseline = centerY + font->capHeight * s * 0.5f;
	for( size_t i = 0; i < text.size(); ++i )
	{
		const Glyph* glyph = font->Find( text[ i ] );
		if( !glyph )
			continue;
		if( glyph->width > 0.0f )
		{
			float x0 = penX + glyph->x0 * s;
			float y0 = baseline + glyph->y0 * s;
			PushQuad( x0, y0, x0 + glyph->width * s, y0 + glyph->height * s, glyph->u0, glyph->v0, glyph->u1, glyph->v1, rgba );
		}
		penX += glyph->advance * s;
		if( i + 1 < text.size() )
			penX += font->Kern( text[ i ], text[ i + 1 ] ) * s;
	}
}

void TextRenderer::Flush( float outline, const float outlineRgb[ 3 ] )
{
	if( vertices.empty() || !shader.IsReady() )
		return;

	ScopedShaderBinding shaderBinding( shader.GetGLID() );
	ScopedVAOBinding vaoBinding( vao );
	ScopedVBOBinding vboBinding( vbo );
	glActiveTexture( GL_TEXTURE0 );
	Scoped2DTextureBinding textureBinding( texture );

	shader.Set( "uViewport", viewportWidth, viewportHeight );
	shader.Set( "uAtlas", 0 );
	shader.Set( "uEdge", kEdge );
	// Leave a little margin so the widest outline never touches the edge of the distance field.
	shader.Set( "uOutline", std::min( 1.0f, std::max( 0.0f, outline ) ) * ( kEdge - 0.06f ) );
	shader.Set( "uOutlineColor", outlineRgb[ 0 ], outlineRgb[ 1 ], outlineRgb[ 2 ] );

	glBufferData( GL_ARRAY_BUFFER, vertices.size() * sizeof( float ), vertices.data(), GL_DYNAMIC_DRAW );
	glEnable( GL_BLEND );
	glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );// the shader outputs premultiplied alpha
	glDrawArrays( GL_TRIANGLES, 0, (GLsizei)( vertices.size() / kFloatsPerVertex ) );
	glDisable( GL_BLEND );
	glBlendFunc( GL_ONE, GL_ZERO );
	vertices.clear();
}

}//namespace sorteador

#pragma once
#include <FFGLSDK.h>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace sorteador
{
struct BakedFont;

enum class Shape
{
	Solid,// filled rectangle
	Dot,  // soft round spot (an ellipse when the sides differ)
	Ring  // soft circle outline
};

// Capital letters for ASCII, Latin-1 and Latin Extended-A, the characters the atlas has. Others are kept as they are.
std::string ToUpperUtf8( const std::string& utf8 );

// Draws UTF-8 text using a signed distance field atlas baked from a TTF/OTF file with stb_truetype.
// Distance fields keep the edges sharp at any size and make outlines nearly free.
class TextRenderer
{
public:
	TextRenderer();
	~TextRenderer();

	bool InitGL();
	void DeInitGL();

	// Baking happens in the background; until it finishes the previous font (or nothing) is drawn. The very first
	// font is waited for, so the first frame (the one Resolume takes the thumbnail from) already shows text.
	void RequestFont( const std::string& path );
	// Starts baking a font before any instance needs it.
	static void Preload( const std::string& path );
	bool HasFont() const { return font != nullptr; }

	// Width in pixels of a single line drawn with the given pixel height.
	float Measure( const std::string& utf8, float sizePx ) const;

	void Begin( float viewportWidth, float viewportHeight );
	void AddRect( float x0, float y0, float x1, float y1, const float rgba[ 4 ] );
	// Draws a line centered on (centerX, centerY), using the cap height so all-caps and mixed text sit at the same spot.
	void AddText( const std::string& utf8, float centerX, float centerY, float sizePx, const float rgba[ 4 ] );
	// Same placement as AddText, drawing only a soft halo around the letters.
	void AddGlowText( const std::string& utf8, float centerX, float centerY, float sizePx, const float rgba[ 4 ] );
	// A shape centered on (centerX, centerY), rotated by angle radians. thickness is used by rings, 0..1 of the radius.
	void AddShape( Shape shape, float centerX, float centerY, float halfWidth, float halfHeight, float angle, const float rgba[ 4 ],
	               float thickness = 0.1f );
	// outline is 0..1 of the maximum outline width the atlas supports.
	void Flush( float outline, const float outlineRgb[ 3 ] );

private:
	void PollFont();
	void AddGlyphs( const std::string& utf8, float centerX, float centerY, float sizePx, const float rgba[ 4 ], float kind );
	void PushQuad( float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, const float rgba[ 4 ], float kind );
	void PushVertex( float x, float y, float u, float v, const float rgba[ 4 ], float kind );

	ffglex::FFGLShader shader;
	GLuint vao     = 0;
	GLuint vbo     = 0;
	GLuint texture = 0;

	std::shared_ptr< const BakedFont > font;
	std::string requestedPath;
	std::shared_future< std::shared_ptr< const BakedFont > > pending;
	bool hasPending = false;

	std::vector< float > vertices;
	float viewportWidth  = 1.0f;
	float viewportHeight = 1.0f;
};

}//namespace sorteador

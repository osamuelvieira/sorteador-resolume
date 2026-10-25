#pragma once
#include <FFGLSDK.h>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace sorteador
{
struct BakedFont;

// Draws UTF-8 text using a signed distance field atlas baked from a TTF/OTF file with stb_truetype.
// Distance fields keep the edges sharp at any size and make outlines nearly free.
class TextRenderer
{
public:
	TextRenderer();
	~TextRenderer();

	bool InitGL();
	void DeInitGL();

	// Baking happens in the background; until it finishes the previous font (or nothing) is drawn.
	void RequestFont( const std::string& path );
	bool HasFont() const { return font != nullptr; }

	// Width in pixels of a single line drawn with the given pixel height.
	float Measure( const std::string& utf8, float sizePx ) const;

	void Begin( float viewportWidth, float viewportHeight );
	void AddRect( float x0, float y0, float x1, float y1, const float rgba[ 4 ] );
	// Draws a line centered on (centerX, centerY), using the cap height so all-caps and mixed text sit at the same spot.
	void AddText( const std::string& utf8, float centerX, float centerY, float sizePx, const float rgba[ 4 ] );
	// outline is 0..1 of the maximum outline width the atlas supports.
	void Flush( float outline, const float outlineRgb[ 3 ] );

private:
	void PollFont();
	void PushQuad( float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, const float rgba[ 4 ] );

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

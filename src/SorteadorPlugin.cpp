#include "SorteadorPlugin.h"
#include "Platform.h"
#include "QrCode.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

namespace sorteador
{
namespace
{
enum ParamId : unsigned int
{
	P_START,
	P_CLEAR,
	P_RESET_HISTORY,
	P_OPEN_PANEL,
	P_CHANNEL,
	P_MODE,
	P_LIST,
	P_NUM_MIN,
	P_NUM_MAX,
	P_NO_REPEAT,
	P_ANIMATION,
	P_DURATION,
	P_SPEED,
	P_DECEL,
	P_NEIGHBORS,
	P_TEXT_SIZE,
	P_POS_Y,
	P_TEXT_R,
	P_TEXT_G,
	P_TEXT_B,
	P_WINNER_R,
	P_WINNER_G,
	P_WINNER_B,
	P_OUTLINE,
	P_OUTLINE_R,
	P_OUTLINE_G,
	P_OUTLINE_B,
	P_BG_OPACITY,
	P_BG_R,
	P_BG_G,
	P_BG_B,
	P_TITLE,
	P_IDLE_TEXT,
	P_SHOW_ADDRESS,
	// Added in 1.2, kept at the end so compositions saved with older versions keep their values.
	P_PREVIEW,
	P_FX_IMPACT,
	P_FX_FLASH,
	P_FX_SHOCKWAVE,
	P_FX_SPARKS,
	P_FX_CONFETTI,
	P_FX_GLOW,
	P_FX_INTENSITY,
	P_UPPERCASE,
	// Added in 1.3.
	P_REDRAW,
	P_TITLE_SIZE,
	P_TITLE_POS_Y,
	P_TITLE_R,
	P_TITLE_G,
	P_TITLE_B,
	P_IDLE_SIZE,
	P_IDLE_R,
	P_IDLE_G,
	P_IDLE_B,
	P_WRAP,
	P_DRUM,
	P_AUTO_CLEAR,
	// Added in 1.4.
	P_DRAW_COUNT,
	P_REVEAL,
	P_REVEAL_GAP,
	P_COUNTDOWN,
	P_BOARD,
	P_BOARD_TITLE,
	P_COUNT
};

static_assert( P_COUNT <= 96, "previousEventValue holds one value per parameter" );

constexpr float kNumberParamLimit = 9999.0f;
const char* kGroupDraw      = "Sorteio";
const char* kGroupAnimation = "Animação";
const char* kGroupLook      = "Visual";
constexpr float kPi         = 3.14159265358979323846f;

CFFGLPluginInfo PluginInfo(
	PluginFactory< SorteadorPlugin >,
	"SRTD",
	"Sorteador",
	2,
	1,
	1,
	4,
	FF_SOURCE,
	"Sorteio de nomes e números com animação, controlado pelo painel do Resolume ou pelo navegador.",
	"Samuel Vieira - Instagram @osamuelvieira.tecnica - github.com/osamuelvieira/sorteador-resolume",
	0,
	nullptr,
	[]() -> FFResult {
		ChannelManager::Get().Startup();
		TextRenderer::Preload( DefaultFontPath() );
		return FF_SUCCESS;
	},
	[]() { ChannelManager::Get().Shutdown(); } );

bool IsEvent( unsigned int index )
{
	return index == P_START || index == P_CLEAR || index == P_RESET_HISTORY || index == P_OPEN_PANEL || index == P_REDRAW ||
	       index == P_BOARD;
}

// Settings of this clip only, not of its channel.
bool IsInstanceParam( unsigned int index )
{
	return index == P_CHANNEL || index == P_PREVIEW;
}

bool IsText( unsigned int index )
{
	return index == P_LIST || index == P_TITLE || index == P_IDLE_TEXT || index == P_BOARD_TITLE;
}

float* ColorComponent( DrawConfig& c, unsigned int index )
{
	switch( index )
	{
	case P_TEXT_R: return &c.textColor.r;
	case P_TEXT_G: return &c.textColor.g;
	case P_TEXT_B: return &c.textColor.b;
	case P_WINNER_R: return &c.winnerColor.r;
	case P_WINNER_G: return &c.winnerColor.g;
	case P_WINNER_B: return &c.winnerColor.b;
	case P_OUTLINE_R: return &c.outlineColor.r;
	case P_OUTLINE_G: return &c.outlineColor.g;
	case P_OUTLINE_B: return &c.outlineColor.b;
	case P_BG_R: return &c.bgColor.r;
	case P_BG_G: return &c.bgColor.g;
	case P_BG_B: return &c.bgColor.b;
	case P_TITLE_R: return &c.titleColor.r;
	case P_TITLE_G: return &c.titleColor.g;
	case P_TITLE_B: return &c.titleColor.b;
	case P_IDLE_R: return &c.idleColor.r;
	case P_IDLE_G: return &c.idleColor.g;
	case P_IDLE_B: return &c.idleColor.b;
	default: return nullptr;
	}
}

bool* EffectFlag( DrawConfig& c, unsigned int index )
{
	switch( index )
	{
	case P_FX_IMPACT: return &c.fxImpact;
	case P_FX_FLASH: return &c.fxFlash;
	case P_FX_SHOCKWAVE: return &c.fxShockwave;
	case P_FX_SPARKS: return &c.fxSparks;
	case P_FX_CONFETTI: return &c.fxConfetti;
	case P_FX_GLOW: return &c.fxGlow;
	default: return nullptr;
	}
}

float ReadParam( const DrawConfig& c, unsigned int index )
{
	switch( index )
	{
	case P_MODE: return (float)c.mode;
	case P_NUM_MIN: return (float)c.numMin;
	case P_NUM_MAX: return (float)c.numMax;
	case P_NO_REPEAT: return c.noRepeat ? 1.0f : 0.0f;
	case P_ANIMATION: return (float)c.animation;
	case P_DURATION: return c.duration;
	case P_SPEED: return c.speed;
	case P_DECEL: return c.decel;
	case P_NEIGHBORS: return (float)c.neighbors;
	case P_TEXT_SIZE: return c.textSize;
	case P_POS_Y: return c.posY;
	case P_OUTLINE: return c.outline;
	case P_BG_OPACITY: return c.bgOpacity;
	case P_SHOW_ADDRESS: return c.showAddress ? 1.0f : 0.0f;
	case P_UPPERCASE: return c.uppercase ? 1.0f : 0.0f;
	case P_FX_INTENSITY: return c.fxIntensity;
	case P_TITLE_SIZE: return c.titleSize;
	case P_TITLE_POS_Y: return c.titlePosY;
	case P_IDLE_SIZE: return c.idleSize;
	case P_WRAP: return c.wrapLongNames ? 1.0f : 0.0f;
	case P_DRUM: return c.drum ? 1.0f : 0.0f;
	case P_AUTO_CLEAR: return c.autoClear;
	case P_DRAW_COUNT: return (float)c.drawCount;
	case P_REVEAL: return (float)c.reveal;
	case P_REVEAL_GAP: return c.revealGap;
	case P_COUNTDOWN: return c.countdown;
	default:
		if( const bool* flag = EffectFlag( const_cast< DrawConfig& >( c ), index ) )
			return *flag ? 1.0f : 0.0f;
		if( const float* component = ColorComponent( const_cast< DrawConfig& >( c ), index ) )
			return *component;
		return 0.0f;
	}
}

void WriteParam( DrawConfig& c, unsigned int index, float value )
{
	switch( index )
	{
	case P_MODE: c.mode = (DrawMode)std::lround( value ); break;
	case P_NUM_MIN: c.numMin = (int)std::lround( value ); break;
	case P_NUM_MAX: c.numMax = (int)std::lround( value ); break;
	case P_NO_REPEAT: c.noRepeat = value > 0.5f; break;
	case P_ANIMATION: c.animation = (Animation)std::lround( value ); break;
	case P_DURATION: c.duration = value; break;
	case P_SPEED: c.speed = value; break;
	case P_DECEL: c.decel = value; break;
	case P_NEIGHBORS: c.neighbors = (int)std::lround( value ); break;
	case P_TEXT_SIZE: c.textSize = value; break;
	case P_POS_Y: c.posY = value; break;
	case P_OUTLINE: c.outline = value; break;
	case P_BG_OPACITY: c.bgOpacity = value; break;
	case P_SHOW_ADDRESS: c.showAddress = value > 0.5f; break;
	case P_UPPERCASE: c.uppercase = value > 0.5f; break;
	case P_FX_INTENSITY: c.fxIntensity = value; break;
	case P_TITLE_SIZE: c.titleSize = value; break;
	case P_TITLE_POS_Y: c.titlePosY = value; break;
	case P_IDLE_SIZE: c.idleSize = value; break;
	case P_WRAP: c.wrapLongNames = value > 0.5f; break;
	case P_DRUM: c.drum = value > 0.5f; break;
	case P_AUTO_CLEAR: c.autoClear = value; break;
	case P_DRAW_COUNT: c.drawCount = (int)std::lround( value ); break;
	case P_REVEAL: c.reveal = (int)std::lround( value ); break;
	case P_REVEAL_GAP: c.revealGap = value; break;
	case P_COUNTDOWN: c.countdown = value; break;
	default:
		if( bool* flag = EffectFlag( c, index ) )
			*flag = value > 0.5f;
		else if( float* component = ColorComponent( c, index ) )
			*component = value;
		break;
	}
	Sanitize( c );
}

// The Resolume text field is a single line, so the list is shown there separated by "; ".
std::string ListForTextParam( const Channel& c )
{
	std::string joined;
	for( const std::string& item : c.Items() )
		joined += ( joined.empty() ? "" : "; " ) + item;
	return joined;
}

std::string ReadText( const Channel& c, unsigned int index )
{
	switch( index )
	{
	case P_LIST: return ListForTextParam( c );
	case P_TITLE: return c.config.title;
	case P_IDLE_TEXT: return c.config.idleText;
	case P_BOARD_TITLE: return c.config.boardTitle;
	default: return {};
	}
}

const std::string& DefaultFont()
{
	static const std::string path = DefaultFontPath();
	return path;
}

float Mix( float a, float b, float t )
{
	return a + ( b - a ) * t;
}

float Clamp01( float v )
{
	return std::min( 1.0f, std::max( 0.0f, v ) );
}

// Deterministic random number in [0, 1), so every instance on a channel draws exactly the same particles.
float Random( uint32_t seed, uint32_t index, uint32_t salt )
{
	uint32_t h = seed ^ ( index * 0x9E3779B9u ) ^ ( salt * 0x85EBCA6Bu );
	h ^= h >> 16;
	h *= 0x7FEB352Du;
	h ^= h >> 15;
	h *= 0x846CA68Bu;
	h ^= h >> 16;
	return (float)( h >> 8 ) / 16777216.0f;
}

// Where the winner landed and how the effects around it should look this frame.
struct EffectFrame
{
	float t;        // seconds since the winner landed
	float intensity;// 0.2..2
	float width, height;
	float x, y;     // center of the winner name
	float textWidth;
	float sizePx;   // configured text size
	float drawnSize;// size the winner is drawn at this frame
	std::vector< std::string > winnerLines;
	float linePitch;// distance between the winner's lines, when it has two
	Color winner;
	uint32_t seed;
	float fade;// the result fading out before going back to the waiting screen
};

// One item laid out on screen: its lines and the size that fits them.
struct TextBlock
{
	std::vector< std::string > lines;
	float size  = 0.0f;
	float width = 0.0f;
};

constexpr float kLinePitch = 1.12f;// between the two lines of a wrapped name, relative to its size

// Long text shrinks to fit. When wrapping is allowed it first tries two lines, split at the space that balances
// them best, and keeps that if the name stays clearly bigger than on one line.
TextBlock FitText( const TextRenderer& text, const std::string& s, float sizePx, float maxWidth, bool mayWrap )
{
	const float measured = text.Measure( s, sizePx );
	if( measured <= maxWidth )
		return { { s }, sizePx, measured };
	const float oneLine = maxWidth / measured;
	if( mayWrap )
	{
		size_t best = std::string::npos;
		float bestWidth = 0.0f;
		for( size_t pos = s.find( ' ' ); pos != std::string::npos; pos = s.find( ' ', pos + 1 ) )
		{
			const float w = std::max( text.Measure( s.substr( 0, pos ), sizePx ), text.Measure( s.substr( pos + 1 ), sizePx ) );
			if( best == std::string::npos || w < bestWidth )
			{
				best      = pos;
				bestWidth = w;
			}
		}
		const float twoLines = best == std::string::npos ? 0.0f : std::min( 1.0f, maxWidth / bestWidth );
		if( twoLines > oneLine * 1.15f )
			return { { s.substr( 0, best ), s.substr( best + 1 ) }, sizePx * twoLines, bestWidth * twoLines };
	}
	return { { s }, sizePx * oneLine, maxWidth };
}

// Draws the lines of a block centered on (x, y).
void AddBlock( TextRenderer& text, const TextBlock& block, float x, float y, float size, const float rgba[ 4 ] )
{
	const float pitch = size * kLinePitch;
	for( size_t i = 0; i < block.lines.size(); ++i )
		text.AddText( block.lines[ i ], x, y + ( (float)i - ( block.lines.size() - 1 ) * 0.5f ) * pitch, size, rgba );
}

// Drawn before the names, so they never cover the winner.
void DrawBackEffects( TextRenderer& text, const DrawConfig& cfg, const EffectFrame& e )
{
	if( cfg.fxConfetti )
	{
		static const float palette[][ 3 ] = {
			{ 1.0f, 0.82f, 0.2f }, { 0.95f, 0.3f, 0.35f }, { 0.25f, 0.65f, 1.0f },
			{ 0.35f, 0.85f, 0.45f }, { 0.75f, 0.45f, 1.0f }, { 1.0f, 1.0f, 1.0f },
		};
		const int count = (int)std::lround( 140.0f * e.intensity );
		for( int i = 0; i < count; ++i )
		{
			// Pieces start at different heights above the screen, so the shower keeps coming for a few seconds.
			const float startY = -e.height * ( 0.03f + 1.1f * Random( e.seed, i, 1 ) );
			const float fall   = e.height * ( 0.22f + 0.2f * Random( e.seed, i, 2 ) );
			const float y      = startY + fall * e.t;
			if( y > e.height * 1.05f )
				continue;
			const float sway  = e.height * 0.03f * std::sin( e.t * ( 1.5f + 2.0f * Random( e.seed, i, 3 ) ) + 6.28f * Random( e.seed, i, 4 ) );
			const float x     = e.width * Random( e.seed, i, 5 ) + sway;
			const float half  = e.height * ( 0.006f + 0.006f * Random( e.seed, i, 6 ) );
			const float angle = 6.28f * Random( e.seed, i, 7 ) + e.t * ( Random( e.seed, i, 8 ) - 0.5f ) * 8.0f;
			// Turning over in the air: one side shrinks and grows again.
			const float flip    = std::max( 0.15f, std::fabs( std::cos( e.t * ( 3.0f + 5.0f * Random( e.seed, i, 9 ) ) + 6.28f * Random( e.seed, i, 10 ) ) ) );
			const float* c      = palette[ (int)( Random( e.seed, i, 11 ) * 6.0f ) % 6 ];
			const float fadeIn  = Clamp01( e.t / 0.15f );
			const float rgba[ 4 ] = { c[ 0 ], c[ 1 ], c[ 2 ], 0.95f * fadeIn * e.fade };
			text.AddShape( Shape::Solid, x, y, half, half * 1.7f * flip, angle, rgba );
		}
	}

	if( cfg.fxShockwave )
	{
		for( int ring = 0; ring < 2; ++ring )
		{
			const float duration = 0.9f;
			const float p        = ( e.t - ring * 0.14f ) / duration;
			if( p <= 0.0f || p >= 1.0f )
				continue;
			const float eased     = 1.0f - std::pow( 1.0f - p, 3.0f );
			const float radius    = e.sizePx * 0.6f + e.width * 0.55f * eased * std::min( 1.4f, 0.6f + 0.4f * e.intensity );
			const float thickness = e.sizePx * 0.18f * ( 1.0f - p ) + 2.0f;
			const float fade      = ( 1.0f - p ) * ( 1.0f - p ) * ( ring == 0 ? 0.9f : 0.55f ) * e.fade;
			const float rgba[ 4 ] = { Mix( e.winner.r, 1.0f, 0.5f ), Mix( e.winner.g, 1.0f, 0.5f ), Mix( e.winner.b, 1.0f, 0.5f ), fade };
			// A little wider than tall, closer to the shape of a name.
			text.AddShape( Shape::Ring, e.x, e.y, radius * 1.35f, radius, 0.0f, rgba, thickness / radius );
		}
	}

	if( cfg.fxGlow )
	{
		const float pulse     = 0.6f + 0.25f * std::sin( e.t * 4.0f );
		const float alpha     = pulse * Clamp01( e.t / 0.3f ) * std::min( 1.0f, 0.5f + 0.5f * e.intensity ) * e.fade;
		const float rgba[ 4 ] = { e.winner.r, e.winner.g, e.winner.b, alpha };
		const float faint[ 4 ] = { e.winner.r, e.winner.g, e.winner.b, alpha * 0.5f };
		const float spread = ( e.winnerLines.size() - 1 ) * e.linePitch;
		text.AddShape( Shape::Dot, e.x, e.y, e.textWidth * 0.75f + e.sizePx, e.sizePx * 1.4f + spread * 0.5f, 0.0f, faint );
		// The halo of a single glyph is short, a slightly larger second copy spreads it further.
		for( size_t i = 0; i < e.winnerLines.size(); ++i )
		{
			const float y = e.y + ( (float)i - ( e.winnerLines.size() - 1 ) * 0.5f ) * e.linePitch;
			text.AddGlowText( e.winnerLines[ i ], e.x, y, e.drawnSize * 1.06f, faint );
			text.AddGlowText( e.winnerLines[ i ], e.x, y, e.drawnSize, rgba );
		}
	}
}

// Drawn over the names.
void DrawFrontEffects( TextRenderer& text, const DrawConfig& cfg, const EffectFrame& e )
{
	if( cfg.fxSparks )
	{
		const int count     = (int)std::lround( 70.0f * e.intensity );
		const float gravity = e.height * 1.3f;
		const float drag    = 0.35f;// seconds, how fast the sparks lose their initial speed
		for( int i = 0; i < count; ++i )
		{
			const float life = 0.8f + 0.9f * Random( e.seed, i, 21 );
			const float age  = e.t / life;
			if( age >= 1.0f )
				continue;
			// Launched from along the name, mostly upwards and sideways.
			const float x0    = e.x + ( Random( e.seed, i, 22 ) - 0.5f ) * e.textWidth * 0.9f;
			const float y0    = e.y + ( Random( e.seed, i, 23 ) - 0.5f ) * e.sizePx * 0.5f;
			const float angle = -kPi * 0.5f + ( Random( e.seed, i, 24 ) - 0.5f ) * kPi * 1.7f;
			const float speed = e.height * ( 0.6f + 1.4f * Random( e.seed, i, 25 ) ) * std::min( 1.5f, 0.7f + 0.3f * e.intensity );
			const float vx = std::cos( angle ) * speed, vy = std::sin( angle ) * speed;
			const float slow = std::exp( -e.t / drag );
			const float x    = x0 + vx * drag * ( 1.0f - slow );
			const float y    = y0 + vy * drag * ( 1.0f - slow ) + 0.5f * gravity * e.t * e.t;
			// Each spark is a short streak pointing where it is going.
			const float cvx = vx * slow, cvy = vy * slow + gravity * e.t;
			const float currentSpeed = std::sqrt( cvx * cvx + cvy * cvy );
			const float length       = std::max( e.sizePx * 0.05f, currentSpeed * 0.04f );
			const float thickness    = e.sizePx * ( 0.03f + 0.02f * Random( e.seed, i, 26 ) );
			const float fade         = std::pow( 1.0f - age, 1.5f ) * e.fade;
			// White hot at first, cooling to gold.
			const float rgba[ 4 ] = { 1.0f, Mix( 1.0f, 0.72f, age ), Mix( 1.0f, 0.25f, age ), fade };
			text.AddShape( Shape::Dot, x, y, length, thickness, std::atan2( cvy, cvx ), rgba );
		}
	}

	if( cfg.fxFlash )
	{
		const float duration = 0.5f;
		if( e.t < duration )
		{
			const float p         = 1.0f - e.t / duration;
			const float rgba[ 4 ] = { Mix( 1.0f, e.winner.r, 0.25f ), Mix( 1.0f, e.winner.g, 0.25f ), Mix( 1.0f, e.winner.b, 0.25f ),
				                      std::min( 1.0f, 0.75f * e.intensity ) * p * p * p * e.fade };
			text.AddRect( 0.0f, 0.0f, e.width, e.height, rgba );
		}
	}
}
// Several names in rows and columns, all at the same size: the column count that lets them be biggest wins.
struct GridLayout
{
	float size      = 0.0f;
	float cellWidth = 0.0f;
	std::vector< std::pair< float, float > > centers;
};

GridLayout LayoutGrid( const TextRenderer& text, const std::vector< std::string >& texts, float maxSize, float x0, float y0, float x1, float y1 )
{
	GridLayout layout;
	const size_t n = texts.size();
	if( n == 0 )
		return layout;
	std::vector< float > unit( n );// width of each text per pixel of size
	for( size_t i = 0; i < n; ++i )
		unit[ i ] = text.Measure( texts[ i ], 100.0f ) / 100.0f;
	size_t columns = 1;
	for( size_t cols = 1; cols <= n; ++cols )
	{
		const size_t rows  = ( n + cols - 1 ) / cols;
		const float cellW  = ( x1 - x0 ) / cols, cellH = ( y1 - y0 ) / rows;
		float size         = std::min( maxSize, cellH * 0.6f );
		for( float u : unit )
			if( u > 0.0f )
				size = std::min( size, cellW * 0.88f / u );
		if( size > layout.size )
		{
			layout.size = size;
			columns     = cols;
		}
	}
	const size_t rows = ( n + columns - 1 ) / columns;
	const float cellW = ( x1 - x0 ) / columns, cellH = ( y1 - y0 ) / rows;
	layout.cellWidth  = cellW;
	for( size_t i = 0; i < n; ++i )
	{
		const size_t row = i / columns, col = i % columns;
		// A shorter last row is centered.
		const size_t inRow = row == rows - 1 ? n - row * columns : columns;
		layout.centers.push_back( { x0 + ( columns - inRow ) * cellW * 0.5f + ( col + 0.5f ) * cellW, y0 + ( row + 0.5f ) * cellH } );
	}
	return layout;
}

// The wheel of fortune: slices in alternating colors with their names along the radius, and a pointer on top.
void DrawWheel( TextRenderer& text, const DrawConfig& cfg, const WheelView& wheel, float centerX, float centerY, float width, float height )
{
	const size_t n = wheel.entries.size();
	if( wheel.alpha <= 0.0f || n == 0 )
		return;
	static const float palette[][ 3 ] = {
		{ 0.85f, 0.25f, 0.3f }, { 0.95f, 0.65f, 0.15f }, { 0.2f, 0.6f, 0.9f },
		{ 0.3f, 0.75f, 0.4f },  { 0.6f, 0.35f, 0.85f },  { 0.15f, 0.7f, 0.7f },
	};
	const float radius = std::min( height * 0.38f, width * 0.42f );
	const float slice  = 2.0f * kPi / (float)n;
	for( size_t i = 0; i < n; ++i )
	{
		// The last slice must not share the color of the first, its neighbor.
		const size_t colorIndex = ( n > 1 && i == n - 1 && i % 6 == 0 ) ? 3 : i % 6;
		const float* c          = palette[ colorIndex ];
		const float dim         = (int)i == wheel.winnerIndex ? 1.0f : 1.0f - 0.6f * wheel.highlight;
		const float rgba[ 4 ]   = { c[ 0 ], c[ 1 ], c[ 2 ], wheel.alpha * dim };
		const float angle       = -kPi * 0.5f + i * slice - wheel.rotation;
		text.AddShape( Shape::Sector, centerX, centerY, radius, radius, angle, rgba, std::max( 0.001f, slice * 0.5f - 0.008f ) );
	}
	const float rim[ 4 ] = { 1.0f, 1.0f, 1.0f, 0.9f * wheel.alpha };
	text.AddShape( Shape::Ring, centerX, centerY, radius * 1.03f, radius * 1.03f, 0.0f, rim, 0.04f );
	text.AddShape( Shape::Sector, centerX, centerY, radius * 0.1f, radius * 0.1f, 0.0f, rim, kPi );

	// Names read from the center outwards, turned upright on the left half.
	const float chord = 2.0f * radius * 0.62f * std::sin( std::min( kPi * 0.5f, slice * 0.5f ) );
	for( size_t i = 0; i < n; ++i )
	{
		const std::string label = cfg.uppercase ? ToUpperUtf8( wheel.entries[ i ] ) : wheel.entries[ i ];
		const float angle       = -kPi * 0.5f + i * slice - wheel.rotation;
		const float unit        = text.Measure( label, 100.0f ) / 100.0f;
		float size              = std::min( radius * 0.12f, chord * 0.75f );
		if( unit > 0.0f )
			size = std::min( size, radius * 0.6f / unit );
		const float dim       = (int)i == wheel.winnerIndex ? 1.0f : 1.0f - 0.6f * wheel.highlight;
		const float rgba[ 4 ] = { 1.0f, 1.0f, 1.0f, wheel.alpha * dim };
		const float upright   = std::cos( angle ) < 0.0f ? angle + kPi : angle;
		text.AddText( label, centerX + std::cos( angle ) * radius * 0.6f, centerY + std::sin( angle ) * radius * 0.6f, size, rgba, upright );
	}

	// The pointer: a wedge whose tip touches the wheel at the top.
	const float pointer[ 4 ] = { cfg.winnerColor.r, cfg.winnerColor.g, cfg.winnerColor.b, std::max( wheel.alpha, 0.6f ) };
	text.AddShape( Shape::Sector, centerX, centerY - radius * 0.94f, radius * 0.2f, radius * 0.2f, -kPi * 0.5f, pointer, 0.32f );
}
}//namespace

SorteadorPlugin::SorteadorPlugin()
{
	SetMinInputs( 0 );
	SetMaxInputs( 0 );

	// Defaults come from the saved channel settings, so adding a new instance never resets what is configured.
	ChannelManager::Get().Startup();
	DrawConfig c = ChannelManager::Get().With( 0, []( Channel& ch ) { return ch.config; } );
	std::string list = ChannelManager::Get().With( 0, []( Channel& ch ) { return ListForTextParam( ch ); } );

	SetParamInfo( P_START, "Iniciar sorteio", FF_TYPE_EVENT, false );
	SetParamInfo( P_CLEAR, "Limpar tela", FF_TYPE_EVENT, false );
	SetParamInfo( P_RESET_HISTORY, "Zerar histórico", FF_TYPE_EVENT, false );
	SetParamInfo( P_OPEN_PANEL, "Abrir painel web", FF_TYPE_EVENT, false );

	SetOptionParamInfo( P_CHANNEL, "Canal", ChannelManager::kNumChannels, 0.0f );
	for( int i = 0; i < ChannelManager::kNumChannels; ++i )
		SetParamElementInfo( P_CHANNEL, i, ( "Canal " + std::to_string( i + 1 ) ).c_str(), (float)i );

	SetOptionParamInfo( P_MODE, "Tipo", 2, (float)c.mode );
	SetParamElementInfo( P_MODE, 0, "Lista de nomes", 0.0f );
	SetParamElementInfo( P_MODE, 1, "Números", 1.0f );
	SetParamGroup( P_MODE, kGroupDraw );
	SetParamInfo( P_LIST, "Lista (separe com ;)", FF_TYPE_TEXT, list.c_str() );
	SetParamGroup( P_LIST, kGroupDraw );
	AddRangeParam( P_NUM_MIN, "Número inicial", (float)c.numMin, -kNumberParamLimit, kNumberParamLimit, kGroupDraw );
	AddRangeParam( P_NUM_MAX, "Número final", (float)c.numMax, -kNumberParamLimit, kNumberParamLimit, kGroupDraw );
	SetParamInfo( P_NO_REPEAT, "Não repetir", FF_TYPE_BOOLEAN, c.noRepeat );
	SetParamGroup( P_NO_REPEAT, kGroupDraw );

	SetOptionParamInfo( P_ANIMATION, "Animação", 5, (float)c.animation );
	SetParamElementInfo( P_ANIMATION, 0, "Rolagem vertical", 0.0f );
	SetParamElementInfo( P_ANIMATION, 1, "Rolagem horizontal", 1.0f );
	SetParamElementInfo( P_ANIMATION, 2, "Troca no lugar", 2.0f );
	SetParamElementInfo( P_ANIMATION, 3, "Roda da fortuna", 3.0f );
	SetParamElementInfo( P_ANIMATION, 4, "Caça-níquel", 4.0f );
	SetParamGroup( P_ANIMATION, kGroupAnimation );
	AddRangeParam( P_DURATION, "Duração (s)", c.duration, 1.0f, 60.0f, kGroupAnimation );
	AddRangeParam( P_SPEED, "Velocidade inicial", c.speed, 2.0f, 100.0f, kGroupAnimation );
	AddRangeParam( P_DECEL, "Desaceleração", c.decel, 1.0f, 8.0f, kGroupAnimation );
	AddRangeParam( P_NEIGHBORS, "Itens ao redor", (float)c.neighbors, 0.0f, (float)kMaxNeighbors, kGroupAnimation );

	AddRangeParam( P_TEXT_SIZE, "Tamanho do texto", c.textSize, 0.02f, 0.6f, kGroupLook );
	AddRangeParam( P_POS_Y, "Posição vertical", c.posY, 0.0f, 1.0f, kGroupLook );
	AddColorParams( P_TEXT_R, "Cor do texto", c.textColor, kGroupLook );
	AddColorParams( P_WINNER_R, "Cor do sorteado", c.winnerColor, kGroupLook );
	AddRangeParam( P_OUTLINE, "Contorno", c.outline, 0.0f, 1.0f, kGroupLook );
	AddColorParams( P_OUTLINE_R, "Cor do contorno", c.outlineColor, kGroupLook );
	AddRangeParam( P_BG_OPACITY, "Opacidade do fundo", c.bgOpacity, 0.0f, 1.0f, kGroupLook );
	AddColorParams( P_BG_R, "Cor do fundo", c.bgColor, kGroupLook );
	SetParamInfo( P_TITLE, "Título", FF_TYPE_TEXT, c.title.c_str() );
	SetParamGroup( P_TITLE, kGroupLook );
	SetParamInfo( P_IDLE_TEXT, "Texto de espera", FF_TYPE_TEXT, c.idleText.c_str() );
	SetParamGroup( P_IDLE_TEXT, kGroupLook );
	SetParamInfo( P_UPPERCASE, "Letras maiúsculas", FF_TYPE_BOOLEAN, c.uppercase );
	SetParamGroup( P_UPPERCASE, kGroupLook );
	SetParamInfo( P_SHOW_ADDRESS, "Mostrar endereço web", FF_TYPE_BOOLEAN, c.showAddress );
	SetParamGroup( P_SHOW_ADDRESS, kGroupLook );

	SetParamInfo( P_PREVIEW, "Modo prévia (teste)", FF_TYPE_BOOLEAN, false );
	SetParamGroup( P_PREVIEW, kGroupDraw );

	const std::pair< unsigned int, const char* > effects[] = {
		{ P_FX_IMPACT, "Impacto" },     { P_FX_FLASH, "Flash" },     { P_FX_SHOCKWAVE, "Onda de choque" },
		{ P_FX_SPARKS, "Faíscas" },     { P_FX_CONFETTI, "Confete" }, { P_FX_GLOW, "Brilho" },
	};
	for( const auto& effect : effects )
	{
		SetParamInfo( effect.first, effect.second, FF_TYPE_BOOLEAN, ReadParam( c, effect.first ) > 0.5f );
		SetParamGroup( effect.first, kGroupAnimation );
	}
	AddRangeParam( P_FX_INTENSITY, "Intensidade dos efeitos", c.fxIntensity, 0.2f, 2.0f, kGroupAnimation );

	SetParamInfo( P_REDRAW, "Ausente, sortear de novo", FF_TYPE_EVENT, false );

	AddRangeParam( P_TITLE_SIZE, "Tamanho do título", c.titleSize, 0.02f, 0.3f, kGroupLook );
	AddRangeParam( P_TITLE_POS_Y, "Posição do título", c.titlePosY, 0.0f, 1.0f, kGroupLook );
	AddColorParams( P_TITLE_R, "Cor do título", c.titleColor, kGroupLook );
	AddRangeParam( P_IDLE_SIZE, "Tamanho do texto de espera", c.idleSize, 0.02f, 0.6f, kGroupLook );
	AddColorParams( P_IDLE_R, "Cor do texto de espera", c.idleColor, kGroupLook );
	SetParamInfo( P_WRAP, "Nomes longos em 2 linhas", FF_TYPE_BOOLEAN, c.wrapLongNames );
	SetParamGroup( P_WRAP, kGroupLook );
	SetParamInfo( P_DRUM, "Efeito tambor", FF_TYPE_BOOLEAN, c.drum );
	SetParamGroup( P_DRUM, kGroupAnimation );
	AddRangeParam( P_AUTO_CLEAR, "Voltar à espera após (s)", c.autoClear, 0.0f, 60.0f, kGroupAnimation );

	AddRangeParam( P_DRAW_COUNT, "Ganhadores por sorteio", (float)c.drawCount, 1.0f, (float)kMaxDrawCount, kGroupDraw );
	SetOptionParamInfo( P_REVEAL, "Revelar ganhadores", 2, (float)c.reveal );
	SetParamElementInfo( P_REVEAL, 0, "Um a um", 0.0f );
	SetParamElementInfo( P_REVEAL, 1, "Todos em grade", 1.0f );
	SetParamGroup( P_REVEAL, kGroupDraw );
	AddRangeParam( P_REVEAL_GAP, "Intervalo entre ganhadores (s)", c.revealGap, 1.0f, 15.0f, kGroupDraw );
	AddRangeParam( P_COUNTDOWN, "Contagem regressiva (s)", c.countdown, 0.0f, 5.0f, kGroupAnimation );
	SetParamInfo( P_BOARD, "Mostrar ganhadores", FF_TYPE_EVENT, false );
	SetParamInfo( P_BOARD_TITLE, "Título dos ganhadores", FF_TYPE_TEXT, c.boardTitle.c_str() );
	SetParamGroup( P_BOARD_TITLE, kGroupLook );
}

void SorteadorPlugin::AddRangeParam( unsigned int index, const char* name, float value, float min, float max, const char* group )
{
	SetParamInfo( index, name, FF_TYPE_STANDARD, value );
	// SetParamInfo clamps standard defaults to 0..1, which is wrong once the parameter has its own range.
	if( ParamInfo* info = FindParamInfo( index ) )
		info->defaultFloatVal = value;
	SetParamRange( index, min, max );
	SetParamGroup( index, group );
}

void SorteadorPlugin::AddColorParams( unsigned int redIndex, const char* name, const Color& color, const char* group )
{
	// Consecutive red/green/blue parameters are shown by Resolume as a single color picker.
	std::string base = name;
	SetParamInfo( redIndex, base.c_str(), FF_TYPE_RED, color.r );
	SetParamInfo( redIndex + 1, ( base + " G" ).c_str(), FF_TYPE_GREEN, color.g );
	SetParamInfo( redIndex + 2, ( base + " B" ).c_str(), FF_TYPE_BLUE, color.b );
	for( unsigned int i = 0; i < 3; ++i )
		SetParamGroup( redIndex + i, group );
}

FFResult SorteadorPlugin::InitGL( const FFGLViewportStruct* vp )
{
	currentViewport = *vp;
	if( !text.InitGL() || !titleText.InitGL() )
	{
		DeInitGL();
		return FF_FAIL;
	}
	return CFFGLPlugin::InitGL( vp );
}

FFResult SorteadorPlugin::DeInitGL()
{
	text.DeInitGL();
	titleText.DeInitGL();
	return FF_SUCCESS;
}

void SorteadorPlugin::RaiseValueEvents()
{
	for( unsigned int i = 0; i < P_COUNT; ++i )
		if( !IsEvent( i ) && !IsInstanceParam( i ) )
			RaiseParamEvent( i, FF_EVENT_FLAG_VALUE );
}

FFResult SorteadorPlugin::ProcessOpenGL( ProcessOpenGLStruct* )
{
	const double now        = ChannelManager::Now();
	uint64_t configRevision = 0;
	FrameView frame         = ChannelManager::Get().With( channel, [ & ]( Channel& c ) {
		configRevision = c.configRevision;
		return c.BuildFrame( now, preview );
	} );
	ChannelManager::Get().ReportFrame( this, channel, preview );

	// Channels named in the web panel show their names in the Resolume channel menu too.
	const std::vector< std::string > names = ChannelManager::Get().Names();
	if( names != channelNames )
	{
		channelNames = names;
		std::vector< std::string > labels;
		std::vector< float > values;
		for( int i = 0; i < (int)names.size(); ++i )
		{
			labels.push_back( names[ i ].empty() ? ChannelManager::Label( names[ i ], i ) : std::to_string( i + 1 ) + " · " + names[ i ] );
			values.push_back( (float)i );
		}
		SetParamElements( P_CHANNEL, labels, values, true );
	}

	// Something else (the web panel or another instance) changed the settings: let Resolume refresh its sliders.
	if( configRevision != seenConfigRevision )
	{
		seenConfigRevision = configRevision;
		RaiseValueEvents();
	}

	const DrawConfig& cfg = frame.config;
	text.RequestFont( cfg.fontPath.empty() ? DefaultFont() : cfg.fontPath );

	const float width  = (float)currentViewport.width;
	const float height = (float)currentViewport.height;
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear( GL_COLOR_BUFFER_BIT );

	text.Begin( width, height );
	if( cfg.bgOpacity > 0.0f )
	{
		const float bg[ 4 ] = { cfg.bgColor.r, cfg.bgColor.g, cfg.bgColor.b, cfg.bgOpacity };
		text.AddRect( 0.0f, 0.0f, width, height, bg );
	}

	const bool idle       = frame.phase == Phase::Idle;
	const float sizePx    = ( idle ? cfg.idleSize : cfg.textSize ) * height * ( frame.countdown ? 1.6f : 1.0f );
	const float centerX   = width * 0.5f;
	const float centerY   = ( 1.0f - cfg.posY ) * height;
	const float maxWidth  = width * 0.92f;
	const bool horizontal = frame.animation == Animation::Horizontal && !idle;
	auto upper            = [ & ]( const std::string& s ) { return cfg.uppercase ? ToUpperUtf8( s ) : s; };

	DrawWheel( text, cfg, frame.wheel, centerX, centerY, width, height );

	std::vector< VisibleItem > items = frame.items;
	const std::string title          = upper( cfg.title );
	const std::string detail         = upper( frame.detail );
	for( VisibleItem& item : items )
		item.text = upper( item.text );
	std::sort( items.begin(), items.end(), []( const VisibleItem& a, const VisibleItem& b ) { return a.offset < b.offset; } );
	std::vector< TextBlock > blocks( items.size() );
	std::vector< float > positions( items.size(), centerX );
	for( size_t i = 0; i < items.size(); ++i )
	{
		// Only a name standing still in the center may take two lines; moving ones would jump between layouts.
		const bool still = idle || frame.animation == Animation::Flip || ( frame.landed && std::fabs( items[ i ].offset ) < 1e-4f );
		blocks[ i ]      = FitText( text, items[ i ].text, sizePx, maxWidth, cfg.wrapLongNames && still && !frame.countdown );
	}

	if( horizontal && !items.empty() )
	{
		// Names sit side by side like a marquee, so short and long ones keep the same size. The item on or just
		// left of the center slot anchors the row and the others follow at their own widths.
		size_t anchor = 0;
		for( size_t i = 0; i < items.size(); ++i )
			if( items[ i ].offset <= 1e-4f )
				anchor = i;
		const float gap = sizePx * 0.9f;
		auto pitch      = [ & ]( size_t a, size_t b ) { return blocks[ a ].width * 0.5f + gap + blocks[ b ].width * 0.5f; };
		if( anchor + 1 < items.size() )
			positions[ anchor ] = centerX + items[ anchor ].offset * pitch( anchor, anchor + 1 );
		for( size_t i = anchor + 1; i < items.size(); ++i )
			positions[ i ] = positions[ i - 1 ] + pitch( i - 1, i );
		for( size_t i = anchor; i-- > 0; )
			positions[ i ] = positions[ i + 1 ] - pitch( i, i + 1 );
	}

	// The winner is the item resting on the center slot once it landed.
	size_t winnerIndex = items.size();
	for( size_t i = 0; frame.landed && i < items.size(); ++i )
		if( std::fabs( items[ i ].offset ) < 1e-4f )
			winnerIndex = i;

	// Grids: several winners at once, everyone after them one by one, or the winners board.
	std::vector< std::string > cellTexts, layoutTexts;
	for( size_t i = 0; i < frame.cells.size(); ++i )
	{
		const std::string number = frame.board ? std::to_string( i + 1 ) + ". " : std::string();
		cellTexts.push_back( upper( number + frame.cells[ i ].text ) );
		layoutTexts.push_back( upper( number + frame.cells[ i ].layoutText ) );
	}
	const float gridTop   = height * ( frame.board ? 0.32f : 0.22f );
	const GridLayout grid = LayoutGrid( text, layoutTexts, cfg.textSize * height, width * 0.04f, gridTop, width * 0.96f, height * 0.93f );

	EffectFrame fx{};
	bool effects = false;
	float shakeX = 0.0f, shakeY = 0.0f;
	if( winnerIndex < items.size() )
	{
		const float t = frame.sinceResult;
		if( cfg.fxImpact )
		{
			// A hard punch that overshoots and settles, while the whole row shakes for a moment.
			const float punch = 0.55f * std::min( 1.5f, cfg.fxIntensity ) * std::exp( -6.0f * t ) * std::cos( 9.0f * t ) * Clamp01( t / 0.04f );
			items[ winnerIndex ].scale = 1.0f + punch;
			const float shake = height * 0.012f * cfg.fxIntensity * std::exp( -9.0f * t );
			shakeX            = shake * std::sin( t * 71.0f + (float)( frame.seed % 97 ) );
			shakeY            = shake * std::sin( t * 59.0f + (float)( frame.seed % 89 ) );
		}
		const TextBlock& block = blocks[ winnerIndex ];
		fx.x           = positions[ winnerIndex ];
		fx.y           = centerY;
		fx.sizePx      = sizePx;
		fx.drawnSize   = block.size * items[ winnerIndex ].scale;
		fx.textWidth   = block.width * items[ winnerIndex ].scale;
		fx.winnerLines = block.lines;
		effects        = true;
	}
	else if( frame.landed && !frame.cells.empty() )
	{
		// The last name of a grid landed: the effects play once, around the whole grid.
		fx.x         = width * 0.5f;
		fx.y         = ( gridTop + height * 0.93f ) * 0.5f;
		fx.sizePx    = grid.size;
		fx.drawnSize = grid.size;
		fx.textWidth = width * 0.6f;
		effects      = true;
	}
	if( effects )
	{
		fx.t         = frame.sinceResult;
		fx.intensity = cfg.fxIntensity;
		fx.width     = width;
		fx.height    = height;
		fx.linePitch = fx.drawnSize * kLinePitch;
		fx.winner    = cfg.winnerColor;
		fx.seed      = frame.seed;
		fx.fade      = frame.fade;
		DrawBackEffects( text, cfg, fx );
	}

	const float lineSpacing = sizePx * 1.35f;
	// A two line winner pushes its fading neighbors apart, so they do not run into it.
	const float winnerExtra = winnerIndex < items.size() ? ( blocks[ winnerIndex ].lines.size() - 1 ) * blocks[ winnerIndex ].size * kLinePitch * 0.5f : 0.0f;
	// The drum bends the reel away from the viewer: neighbors get closer together and smaller towards the edges.
	const bool drum  = cfg.drum && !horizontal && !idle;
	const float step = kPi * 0.5f / ( (float)cfg.neighbors + 1.5f );
	for( size_t i = 0; i < items.size(); ++i )
	{
		const VisibleItem& item = items[ i ];
		float y = centerY, depth = 1.0f;
		if( !horizontal )
		{
			if( drum )
			{
				const float angle = std::min( kPi * 0.5f, std::max( -kPi * 0.5f, item.offset * step ) );
				y                 = centerY + lineSpacing / step * std::sin( angle );
				depth             = 0.55f + 0.45f * std::cos( angle );
			}
			else
				y = centerY + item.offset * lineSpacing;
			if( i != winnerIndex )
				y += item.offset > 0.0f ? winnerExtra : ( item.offset < 0.0f ? -winnerExtra : 0.0f );
		}
		const Color& base     = idle ? cfg.idleColor : cfg.textColor;
		const float color[ 4 ] = {
			Mix( base.r, cfg.winnerColor.r, item.winnerMix ),
			Mix( base.g, cfg.winnerColor.g, item.winnerMix ),
			Mix( base.b, cfg.winnerColor.b, item.winnerMix ),
			item.alpha,
		};
		AddBlock( text, blocks[ i ], positions[ i ] + shakeX, y + shakeY, blocks[ i ].size * item.scale * depth, color );
	}

	// Slot machine: every character in its own column, rolling until it stops.
	if( !frame.columns.empty() )
	{
		std::vector< float > widths;
		float total = 0.0f;
		for( const ColumnView& column : frame.columns )
		{
			widths.push_back( text.Measure( upper( column.finalText ), sizePx ) );
			total += widths.back();
		}
		const float fit  = total > maxWidth ? maxWidth / total : 1.0f;
		const float size = sizePx * fit;
		float x          = centerX - total * fit * 0.5f;
		for( size_t c = 0; c < frame.columns.size(); ++c )
		{
			const float columnX = x + widths[ c ] * fit * 0.5f;
			x += widths[ c ] * fit;
			// A dark window behind each rolling character, like the reels of a real machine.
			if( frame.columns[ c ].finalText != " " )
			{
				const float window[ 4 ] = { 0.0f, 0.0f, 0.0f, 0.45f * frame.fade };
				const float halfW = widths[ c ] * fit * 0.46f, halfH = size * 0.62f;
				text.AddRect( columnX - halfW, centerY - halfH, columnX + halfW, centerY + halfH, window );
			}
			for( const VisibleItem& item : frame.columns[ c ].items )
			{
				const float color[ 4 ] = {
					Mix( cfg.textColor.r, cfg.winnerColor.r, item.winnerMix ),
					Mix( cfg.textColor.g, cfg.winnerColor.g, item.winnerMix ),
					Mix( cfg.textColor.b, cfg.winnerColor.b, item.winnerMix ),
					item.alpha,
				};
				const float depth = 1.0f - 0.25f * std::min( 1.0f, std::fabs( item.offset ) );
				text.AddText( upper( item.text ), columnX, centerY + item.offset * size, size * depth, color );
			}
		}
	}

	if( frame.board )
	{
		const std::string heading = upper( cfg.boardTitle );
		const TextBlock block     = FitText( text, heading, cfg.textSize * height * 0.6f, maxWidth, false );
		const float color[ 4 ]    = { cfg.winnerColor.r, cfg.winnerColor.g, cfg.winnerColor.b, frame.fade };
		AddBlock( text, block, centerX, height * 0.24f, block.size, color );
	}
	for( size_t i = 0; i < frame.cells.size(); ++i )
	{
		const CellView& cell = frame.cells[ i ];
		const bool landed    = cell.sinceLanded >= 0.0f;
		float scale          = 1.0f;
		if( landed && !frame.board && cfg.fxImpact )
		{
			const float t = cell.sinceLanded;
			scale += 0.35f * std::min( 1.5f, cfg.fxIntensity ) * std::exp( -6.0f * t ) * std::cos( 9.0f * t ) * Clamp01( t / 0.04f );
		}
		const float alpha = frame.fade * ( landed ? 1.0f : 0.8f );
		// A rolling name longer than the one it stops on shrinks to stay inside its cell.
		const float unit     = text.Measure( cellTexts[ i ], 100.0f ) / 100.0f;
		const float cellSize = unit > 0.0f ? std::min( grid.size, grid.cellWidth * 0.92f / unit ) : grid.size;
		if( landed && !frame.board && cfg.fxGlow )
		{
			const float glow[ 4 ] = { cfg.winnerColor.r, cfg.winnerColor.g, cfg.winnerColor.b,
				                      alpha * ( 0.6f + 0.25f * std::sin( cell.sinceLanded * 4.0f ) ) * Clamp01( cell.sinceLanded / 0.3f ) };
			text.AddGlowText( cellTexts[ i ], grid.centers[ i ].first, grid.centers[ i ].second, cellSize * scale, glow );
		}
		const float color[ 4 ] = {
			Mix( cfg.textColor.r, cfg.winnerColor.r, cell.winnerMix ),
			Mix( cfg.textColor.g, cfg.winnerColor.g, cell.winnerMix ),
			Mix( cfg.textColor.b, cfg.winnerColor.b, cell.winnerMix ),
			alpha,
		};
		text.AddText( cellTexts[ i ], grid.centers[ i ].first, grid.centers[ i ].second, cellSize * scale, color );
	}

	// Winners revealed one by one wait in a line at the bottom.
	if( !frame.tray.empty() )
	{
		std::string line;
		for( const std::string& name : frame.tray )
			line += ( line.empty() ? "" : "   ·   " ) + upper( name );
		const TextBlock block  = FitText( text, line, cfg.textSize * height * 0.38f, maxWidth, false );
		const float color[ 4 ] = { cfg.winnerColor.r, cfg.winnerColor.g, cfg.winnerColor.b, 0.9f * frame.fade };
		AddBlock( text, block, centerX, height * 0.9f, block.size, color );
	}

	// The detail of a "name | detail" entry, in a smaller line under the winner.
	if( winnerIndex < items.size() && !detail.empty() )
	{
		const VisibleItem& item = items[ winnerIndex ];
		const TextBlock& block  = blocks[ winnerIndex ];
		const TextBlock line    = FitText( text, detail, sizePx * 0.4f, maxWidth, false );
		const float drawn       = block.size * item.scale;
		const float y = centerY + ( block.lines.size() - 1 ) * drawn * kLinePitch * 0.5f + drawn * 0.5f + line.size * 0.75f;
		const float color[ 4 ] = { cfg.textColor.r, cfg.textColor.g, cfg.textColor.b, item.winnerMix * frame.fade };
		AddBlock( text, line, positions[ winnerIndex ] + shakeX, y + shakeY, line.size, color );
	}

	if( effects )
		DrawFrontEffects( text, cfg, fx );

	// The title has its own renderer, so it can use a different font.
	titleText.RequestFont( !cfg.titleFontPath.empty() ? cfg.titleFontPath : ( cfg.fontPath.empty() ? DefaultFont() : cfg.fontPath ) );
	titleText.Begin( width, height );
	if( !title.empty() )
	{
		const TextBlock block  = FitText( titleText, title, cfg.titleSize * height, maxWidth, false );
		const float color[ 4 ] = { cfg.titleColor.r, cfg.titleColor.g, cfg.titleColor.b, 1.0f };
		AddBlock( titleText, block, centerX, ( 1.0f - cfg.titlePosY ) * height, block.size, color );
	}

	if( frame.phase == Phase::Idle && cfg.showAddress )
	{
		std::string address = ChannelManager::Get().WebAddress();
		if( !address.empty() )
		{
			// A QR code in the corner opens the panel on this channel, so nobody has to type the address on a phone.
			const std::string url = address + "/?canal=" + std::to_string( channel + 1 );
			if( url != qrText )
			{
				qrText    = url;
				qrModules = QrModules( url );
			}
			const float margin = height * 0.03f;
			float qrSide       = 0.0f;
			if( !qrModules.empty() )
			{
				// Whole pixels per module keep every square the same size, which scanners prefer.
				const int quiet    = 2;
				const int cells    = (int)qrModules.size() + 2 * quiet;
				const float module = std::max( 2.0f, std::floor( height * 0.2f / cells ) );
				qrSide             = module * cells;
				const float x0 = std::floor( width - margin - qrSide ), y0 = std::floor( height - margin - qrSide );
				const float white[ 4 ] = { 1.0f, 1.0f, 1.0f, 1.0f };
				const float black[ 4 ] = { 0.0f, 0.0f, 0.0f, 1.0f };
				text.AddRect( x0, y0, x0 + qrSide, y0 + qrSide, white );
				for( size_t y = 0; y < qrModules.size(); ++y )
					for( size_t x = 0; x < qrModules[ y ].size(); ++x )
						if( qrModules[ y ][ x ] == '1' )
						{
							const float mx = x0 + ( quiet + x ) * module, my = y0 + ( quiet + y ) * module;
							text.AddRect( mx, my, mx + module, my + module, black );
						}
				const float captionColor[ 4 ] = { 1.0f, 1.0f, 1.0f, 0.75f };
				text.AddText( "Controle pelo celular", x0 + qrSide * 0.5f, y0 - height * 0.025f, height * 0.025f, captionColor );
			}
			// The address stays readable for laptops, and keeps clear of the QR code.
			const std::string label = channel < (int)channelNames.size() ? ChannelManager::Label( channelNames[ channel ], channel ) : ChannelManager::Label( "", channel );
			const std::string line  = "Controle web: " + address + "  (" + label + ")";
			float lineSize         = height * 0.035f;
			const float room       = width - 2.0f * ( qrSide + 2.0f * margin );
			const float measured   = text.Measure( line, lineSize );
			if( room > 0.0f && measured > room )
				lineSize *= room / measured;
			const float color[ 4 ] = { 1.0f, 1.0f, 1.0f, 0.75f };
			text.AddText( line, centerX, height * 0.94f, lineSize, color );
		}
	}

	if( preview )
	{
		// Reminds the operator that this clip shows tests only.
		const std::string tag  = "PRÉVIA - TESTE";
		const float tagSize    = height * 0.04f;
		const float color[ 4 ] = { 1.0f, 0.82f, 0.2f, 0.9f };
		text.AddText( tag, height * 0.03f + text.Measure( tag, tagSize ) * 0.5f, height * 0.05f, tagSize, color );
	}

	const float outline[ 3 ] = { cfg.outlineColor.r, cfg.outlineColor.g, cfg.outlineColor.b };
	text.Flush( cfg.outline, outline );
	titleText.Flush( cfg.outline, outline );
	return FF_SUCCESS;
}

FFResult SorteadorPlugin::SetFloatParameter( unsigned int index, float value )
{
	if( index >= P_COUNT || IsText( index ) )
		return FF_FAIL;

	if( IsEvent( index ) )
	{
		// Buttons send 1 when pressed and 0 when released; act on the press only.
		bool pressed                 = value > 0.5f && previousEventValue[ index ] <= 0.5f;
		previousEventValue[ index ] = value;
		if( pressed && index == P_OPEN_PANEL )
		{
			std::string address = ChannelManager::Get().WebAddress();
			if( !address.empty() )
				OpenInBrowser( address + "/?canal=" + std::to_string( channel + 1 ) );
		}
		else if( pressed )
		{
			ChannelManager::Get().With( channel, [ this, index ]( Channel& c ) {
				if( index == P_START )
					c.Start( ChannelManager::Now(), preview );
				else if( index == P_BOARD )
					c.ShowBoard( ChannelManager::Now(), preview );
				else if( index == P_REDRAW )
				{
					// A test has no history, so there is nobody to mark absent: it just draws again.
					if( preview )
						c.Start( ChannelManager::Now(), true );
					else
						c.Redraw( ChannelManager::Now() );
				}
				else if( index == P_CLEAR )
					c.Clear( ChannelManager::Now(), preview );
				else
					c.ResetHistory();
			} );
		}
		return FF_SUCCESS;
	}

	if( index == P_CHANNEL )
	{
		int newChannel = ChannelManager::ClampChannel( (int)std::lround( value ) );
		if( newChannel != channel )
		{
			channel            = newChannel;
			seenConfigRevision = 0;// forces the other parameters to show the new channel's values
		}
		return FF_SUCCESS;
	}

	if( index == P_PREVIEW )
	{
		preview = value > 0.5f;
		return FF_SUCCESS;
	}

	ChannelManager::Get().With( channel, [ index, value ]( Channel& c ) {
		DrawConfig updated = c.config;
		WriteParam( updated, index, value );
		if( std::fabs( ReadParam( updated, index ) - ReadParam( c.config, index ) ) > 1e-6f )
		{
			c.config = updated;
			c.TouchConfig();
		}
	} );
	return FF_SUCCESS;
}

float SorteadorPlugin::GetFloatParameter( unsigned int index )
{
	if( index == P_CHANNEL )
		return (float)channel;
	if( index == P_PREVIEW )
		return preview ? 1.0f : 0.0f;
	if( IsEvent( index ) || IsText( index ) || index >= P_COUNT )
		return 0.0f;
	return ChannelManager::Get().With( channel, [ index ]( Channel& c ) { return ReadParam( c.config, index ); } );
}

FFResult SorteadorPlugin::SetTextParameter( unsigned int index, const char* value )
{
	if( !IsText( index ) || value == nullptr )
		return FF_FAIL;
	std::string newValue = value;
	ChannelManager::Get().With( channel, [ index, &newValue ]( Channel& c ) {
		if( newValue == ReadText( c, index ) )
			return;
		if( index == P_LIST )
		{
			// Store one item per line, the format the web panel edits.
			c.config.listText = newValue;
			std::vector< std::string > items = c.Items();
			c.config.listText.clear();
			for( const std::string& item : items )
				c.config.listText += ( c.config.listText.empty() ? "" : "\n" ) + item;
		}
		else if( index == P_TITLE )
			c.config.title = newValue;
		else if( index == P_BOARD_TITLE )
			c.config.boardTitle = newValue;
		else
			c.config.idleText = newValue;
		c.TouchConfig();
	} );
	return FF_SUCCESS;
}

char* SorteadorPlugin::GetTextParameter( unsigned int index )
{
	std::string value = ChannelManager::Get().With( channel, [ index ]( Channel& c ) { return ReadText( c, index ); } );
	textBuffer.assign( value.begin(), value.end() );
	textBuffer.push_back( '\0' );
	return textBuffer.data();
}

char* SorteadorPlugin::GetParameterDisplay( unsigned int index )
{
	char text[ 64 ];
	float value = GetFloatParameter( index );
	switch( index )
	{
	case P_NUM_MIN:
	case P_NUM_MAX:
	case P_NEIGHBORS:
		std::snprintf( text, sizeof( text ), "%ld", std::lround( value ) );
		break;
	case P_DURATION:
		std::snprintf( text, sizeof( text ), "%ld s", std::lround( value ) );
		break;
	case P_DRAW_COUNT:
		std::snprintf( text, sizeof( text ), "%ld", std::lround( value ) );
		break;
	case P_REVEAL_GAP:
		std::snprintf( text, sizeof( text ), "%ld s", std::lround( value ) );
		break;
	case P_COUNTDOWN:
		if( std::lround( value ) == 0 )
			std::snprintf( text, sizeof( text ), "sem contagem" );
		else
			std::snprintf( text, sizeof( text ), "%ld s", std::lround( value ) );
		break;
	case P_AUTO_CLEAR:
		if( std::lround( value ) == 0 )
			std::snprintf( text, sizeof( text ), "nunca" );
		else
			std::snprintf( text, sizeof( text ), "%ld s", std::lround( value ) );
		break;
	case P_DECEL:
		std::snprintf( text, sizeof( text ), "%ld", std::lround( value ) );
		break;
	case P_SPEED:
		std::snprintf( text, sizeof( text ), "%.0f itens/s", value );
		break;
	case P_FX_INTENSITY:
		std::snprintf( text, sizeof( text ), "%.0f%%", value * 100.0f );
		break;
	default:
		return CFFGLPlugin::GetParameterDisplay( index );
	}
	displayBuffer.assign( text, text + std::strlen( text ) + 1 );
	return displayBuffer.data();
}

}//namespace sorteador

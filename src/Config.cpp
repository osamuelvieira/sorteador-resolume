#include "Config.h"
#include <algorithm>
#include <cmath>

using nlohmann::json;

namespace sorteador
{
namespace
{
json ColorToJson( const Color& c )
{
	return json::array( { c.r, c.g, c.b } );
}

void ReadColor( const json& j, const char* key, Color& out )
{
	auto it = j.find( key );
	if( it == j.end() || !it->is_array() || it->size() != 3 )
		return;
	out.r = ( *it )[ 0 ].get< float >();
	out.g = ( *it )[ 1 ].get< float >();
	out.b = ( *it )[ 2 ].get< float >();
}

template< typename T >
void Read( const json& j, const char* key, T& out )
{
	auto it = j.find( key );
	if( it != j.end() && !it->is_null() )
		out = it->get< T >();
}

float Clamp01( float v )
{
	return std::min( 1.0f, std::max( 0.0f, v ) );
}

void ClampColor( Color& c )
{
	c.r = Clamp01( c.r );
	c.g = Clamp01( c.g );
	c.b = Clamp01( c.b );
}
}//namespace

void Sanitize( DrawConfig& c )
{
	if( c.mode != DrawMode::List && c.mode != DrawMode::Numbers )
		c.mode = DrawMode::List;
	if( c.animation != Animation::Vertical && c.animation != Animation::Horizontal && c.animation != Animation::Flip )
		c.animation = Animation::Vertical;
	if( c.numMin > c.numMax )
		std::swap( c.numMin, c.numMax );
	c.duration  = std::min( 60.0f, std::max( 1.0f, std::round( c.duration ) ) );
	c.speed     = std::min( 100.0f, std::max( 2.0f, c.speed ) );
	c.decel     = std::min( 8.0f, std::max( 1.0f, std::round( c.decel ) ) );
	c.neighbors = std::min( kMaxNeighbors, std::max( 0, c.neighbors ) );
	c.textSize  = std::min( 0.6f, std::max( 0.02f, c.textSize ) );
	c.posY      = Clamp01( c.posY );
	c.outline   = Clamp01( c.outline );
	c.bgOpacity = Clamp01( c.bgOpacity );
	c.fxIntensity = std::min( 2.0f, std::max( 0.2f, c.fxIntensity ) );
	ClampColor( c.textColor );
	ClampColor( c.winnerColor );
	ClampColor( c.outlineColor );
	ClampColor( c.bgColor );
}

json ToJson( const DrawConfig& c )
{
	return json{
		{ "mode", static_cast< int >( c.mode ) },
		{ "listText", c.listText },
		{ "numMin", c.numMin },
		{ "numMax", c.numMax },
		{ "zeroPad", c.zeroPad },
		{ "noRepeat", c.noRepeat },
		{ "animation", static_cast< int >( c.animation ) },
		{ "duration", c.duration },
		{ "speed", c.speed },
		{ "decel", c.decel },
		{ "neighbors", c.neighbors },
		{ "fontPath", c.fontPath },
		{ "textSize", c.textSize },
		{ "posY", c.posY },
		{ "textColor", ColorToJson( c.textColor ) },
		{ "winnerColor", ColorToJson( c.winnerColor ) },
		{ "outlineColor", ColorToJson( c.outlineColor ) },
		{ "outline", c.outline },
		{ "bgColor", ColorToJson( c.bgColor ) },
		{ "bgOpacity", c.bgOpacity },
		{ "title", c.title },
		{ "idleText", c.idleText },
		{ "uppercase", c.uppercase },
		{ "showAddress", c.showAddress },
		{ "fxImpact", c.fxImpact },
		{ "fxFlash", c.fxFlash },
		{ "fxShockwave", c.fxShockwave },
		{ "fxSparks", c.fxSparks },
		{ "fxConfetti", c.fxConfetti },
		{ "fxGlow", c.fxGlow },
		{ "fxIntensity", c.fxIntensity },
	};
}

void MergeJson( DrawConfig& c, const json& j )
{
	if( !j.is_object() )
		return;

	int mode = static_cast< int >( c.mode );
	Read( j, "mode", mode );
	c.mode = static_cast< DrawMode >( mode );
	Read( j, "listText", c.listText );
	Read( j, "numMin", c.numMin );
	Read( j, "numMax", c.numMax );
	Read( j, "zeroPad", c.zeroPad );
	Read( j, "noRepeat", c.noRepeat );
	int animation = static_cast< int >( c.animation );
	Read( j, "animation", animation );
	c.animation = static_cast< Animation >( animation );
	Read( j, "duration", c.duration );
	Read( j, "speed", c.speed );
	Read( j, "decel", c.decel );
	Read( j, "neighbors", c.neighbors );
	Read( j, "fontPath", c.fontPath );
	Read( j, "textSize", c.textSize );
	Read( j, "posY", c.posY );
	ReadColor( j, "textColor", c.textColor );
	ReadColor( j, "winnerColor", c.winnerColor );
	ReadColor( j, "outlineColor", c.outlineColor );
	Read( j, "outline", c.outline );
	ReadColor( j, "bgColor", c.bgColor );
	Read( j, "bgOpacity", c.bgOpacity );
	Read( j, "title", c.title );
	Read( j, "idleText", c.idleText );
	Read( j, "uppercase", c.uppercase );
	Read( j, "showAddress", c.showAddress );
	Read( j, "fxImpact", c.fxImpact );
	Read( j, "fxFlash", c.fxFlash );
	Read( j, "fxShockwave", c.fxShockwave );
	Read( j, "fxSparks", c.fxSparks );
	Read( j, "fxConfetti", c.fxConfetti );
	Read( j, "fxGlow", c.fxGlow );
	Read( j, "fxIntensity", c.fxIntensity );
	Sanitize( c );
}

}//namespace sorteador

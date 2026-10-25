#include "SorteadorPlugin.h"
#include "Platform.h"
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
	P_COUNT
};

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
	2,
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
	return index == P_START || index == P_CLEAR || index == P_RESET_HISTORY || index == P_OPEN_PANEL;
}

// Settings of this clip only, not of its channel.
bool IsInstanceParam( unsigned int index )
{
	return index == P_CHANNEL || index == P_PREVIEW;
}

bool IsText( unsigned int index )
{
	return index == P_LIST || index == P_TITLE || index == P_IDLE_TEXT;
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
	std::string winnerText;
	Color winner;
	uint32_t seed;
};

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
			const float rgba[ 4 ] = { c[ 0 ], c[ 1 ], c[ 2 ], 0.95f * fadeIn };
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
			const float fade      = ( 1.0f - p ) * ( 1.0f - p ) * ( ring == 0 ? 0.9f : 0.55f );
			const float rgba[ 4 ] = { Mix( e.winner.r, 1.0f, 0.5f ), Mix( e.winner.g, 1.0f, 0.5f ), Mix( e.winner.b, 1.0f, 0.5f ), fade };
			// A little wider than tall, closer to the shape of a name.
			text.AddShape( Shape::Ring, e.x, e.y, radius * 1.35f, radius, 0.0f, rgba, thickness / radius );
		}
	}

	if( cfg.fxGlow )
	{
		const float pulse     = 0.6f + 0.25f * std::sin( e.t * 4.0f );
		const float alpha     = pulse * Clamp01( e.t / 0.3f ) * std::min( 1.0f, 0.5f + 0.5f * e.intensity );
		const float rgba[ 4 ] = { e.winner.r, e.winner.g, e.winner.b, alpha };
		const float faint[ 4 ] = { e.winner.r, e.winner.g, e.winner.b, alpha * 0.5f };
		text.AddShape( Shape::Dot, e.x, e.y, e.textWidth * 0.75f + e.sizePx, e.sizePx * 1.4f, 0.0f, faint );
		// The halo of a single glyph is short, a slightly larger second copy spreads it further.
		text.AddGlowText( e.winnerText, e.x, e.y, e.drawnSize * 1.06f, faint );
		text.AddGlowText( e.winnerText, e.x, e.y, e.drawnSize, rgba );
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
			const float fade         = std::pow( 1.0f - age, 1.5f );
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
				                      std::min( 1.0f, 0.75f * e.intensity ) * p * p * p };
			text.AddRect( 0.0f, 0.0f, e.width, e.height, rgba );
		}
	}
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

	SetOptionParamInfo( P_ANIMATION, "Animação", 3, (float)c.animation );
	SetParamElementInfo( P_ANIMATION, 0, "Rolagem vertical", 0.0f );
	SetParamElementInfo( P_ANIMATION, 1, "Rolagem horizontal", 1.0f );
	SetParamElementInfo( P_ANIMATION, 2, "Troca no lugar", 2.0f );
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
	if( !text.InitGL() )
	{
		DeInitGL();
		return FF_FAIL;
	}
	return CFFGLPlugin::InitGL( vp );
}

FFResult SorteadorPlugin::DeInitGL()
{
	text.DeInitGL();
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

	const float sizePx    = cfg.textSize * height;
	const float centerX   = width * 0.5f;
	const float centerY   = ( 1.0f - cfg.posY ) * height;
	const float maxWidth  = width * 0.92f;
	const bool horizontal = cfg.animation == Animation::Horizontal && frame.phase != Phase::Idle;

	std::vector< VisibleItem > items = frame.items;
	const std::string title          = cfg.uppercase ? ToUpperUtf8( cfg.title ) : cfg.title;
	if( cfg.uppercase )
		for( VisibleItem& item : items )
			item.text = ToUpperUtf8( item.text );
	std::sort( items.begin(), items.end(), []( const VisibleItem& a, const VisibleItem& b ) { return a.offset < b.offset; } );
	std::vector< float > sizes( items.size() ), widths( items.size() ), positions( items.size(), centerX );
	for( size_t i = 0; i < items.size(); ++i )
	{
		// Long names shrink to fit instead of running off the screen.
		float measured = text.Measure( items[ i ].text, sizePx );
		float fit      = measured > maxWidth ? maxWidth / measured : 1.0f;
		sizes[ i ]     = sizePx * fit;
		widths[ i ]    = measured * fit;
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
		auto pitch      = [ & ]( size_t a, size_t b ) { return widths[ a ] * 0.5f + gap + widths[ b ] * 0.5f; };
		if( anchor + 1 < items.size() )
			positions[ anchor ] = centerX + items[ anchor ].offset * pitch( anchor, anchor + 1 );
		for( size_t i = anchor + 1; i < items.size(); ++i )
			positions[ i ] = positions[ i - 1 ] + pitch( i - 1, i );
		for( size_t i = anchor; i-- > 0; )
			positions[ i ] = positions[ i + 1 ] - pitch( i, i + 1 );
	}

	// The winner is the item resting on the center slot once the result is in.
	const bool hasResult = frame.phase == Phase::Result && frame.sinceResult >= 0.0f;
	size_t winnerIndex   = items.size();
	for( size_t i = 0; hasResult && i < items.size(); ++i )
		if( std::fabs( items[ i ].offset ) < 1e-4f )
			winnerIndex = i;

	EffectFrame fx{};
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
		fx.t          = t;
		fx.intensity  = cfg.fxIntensity;
		fx.width      = width;
		fx.height     = height;
		fx.x          = positions[ winnerIndex ];
		fx.y          = centerY;
		fx.sizePx     = sizePx;
		fx.drawnSize  = sizes[ winnerIndex ] * items[ winnerIndex ].scale;
		fx.textWidth  = widths[ winnerIndex ] * items[ winnerIndex ].scale;
		fx.winnerText = items[ winnerIndex ].text;
		fx.winner     = cfg.winnerColor;
		fx.seed       = frame.seed;
		DrawBackEffects( text, cfg, fx );
	}

	const float lineSpacing = sizePx * 1.35f;
	for( size_t i = 0; i < items.size(); ++i )
	{
		const VisibleItem& item = items[ i ];
		float y                 = horizontal ? centerY : centerY + item.offset * lineSpacing;
		const float color[ 4 ]  = {
			Mix( cfg.textColor.r, cfg.winnerColor.r, item.winnerMix ),
			Mix( cfg.textColor.g, cfg.winnerColor.g, item.winnerMix ),
			Mix( cfg.textColor.b, cfg.winnerColor.b, item.winnerMix ),
			item.alpha,
		};
		text.AddText( item.text, positions[ i ] + shakeX, y + shakeY, sizes[ i ] * item.scale, color );
	}

	if( winnerIndex < items.size() )
		DrawFrontEffects( text, cfg, fx );

	if( !title.empty() )
	{
		float titleSize = sizePx * 0.45f;
		float measured  = text.Measure( title, titleSize );
		if( measured > width * 0.92f )
			titleSize *= width * 0.92f / measured;
		const float color[ 4 ] = { cfg.textColor.r, cfg.textColor.g, cfg.textColor.b, 1.0f };
		text.AddText( title, centerX, height * 0.12f, titleSize, color );
	}

	if( frame.phase == Phase::Idle && cfg.showAddress )
	{
		std::string address = ChannelManager::Get().WebAddress();
		if( !address.empty() )
		{
			const float color[ 4 ] = { 1.0f, 1.0f, 1.0f, 0.75f };
			text.AddText( "Controle web: " + address + "  (canal " + std::to_string( channel + 1 ) + ")", centerX, height * 0.94f,
			              height * 0.035f, color );
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
				else if( index == P_CLEAR )
					c.Clear( preview );
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

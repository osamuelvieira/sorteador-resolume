#include "SorteadorPlugin.h"
#include "Platform.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

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
	P_COUNT
};

constexpr float kNumberParamLimit = 9999.0f;
const char* kGroupDraw      = "Sorteio";
const char* kGroupAnimation = "Animação";
const char* kGroupLook      = "Visual";

CFFGLPluginInfo PluginInfo(
	PluginFactory< SorteadorPlugin >,
	"SRTD",
	"Sorteador",
	2,
	1,
	1,
	0,
	FF_SOURCE,
	"Sorteio de nomes e números com animação, controlado pelo painel do Resolume ou pelo navegador.",
	"Samuel Vieira - Instagram @osamuelvieira.tecnica - github.com/osamuelvieira/sorteador-resolume",
	0,
	nullptr,
	[]() -> FFResult {
		ChannelManager::Get().Startup();
		return FF_SUCCESS;
	},
	[]() { ChannelManager::Get().Shutdown(); } );

bool IsEvent( unsigned int index )
{
	return index == P_START || index == P_CLEAR || index == P_RESET_HISTORY || index == P_OPEN_PANEL;
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
	default:
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
	default:
		if( float* component = ColorComponent( c, index ) )
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
	AddRangeParam( P_DURATION, "Duração (s)", c.duration, 0.5f, 60.0f, kGroupAnimation );
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
	SetParamInfo( P_SHOW_ADDRESS, "Mostrar endereço web", FF_TYPE_BOOLEAN, c.showAddress );
	SetParamGroup( P_SHOW_ADDRESS, kGroupLook );
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
		if( !IsEvent( i ) && i != P_CHANNEL )
			RaiseParamEvent( i, FF_EVENT_FLAG_VALUE );
}

FFResult SorteadorPlugin::ProcessOpenGL( ProcessOpenGLStruct* )
{
	const double now        = ChannelManager::Now();
	uint64_t configRevision = 0;
	FrameView frame         = ChannelManager::Get().With( channel, [ & ]( Channel& c ) {
		configRevision = c.configRevision;
		return c.BuildFrame( now );
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
		text.AddText( item.text, positions[ i ], y, sizes[ i ] * item.scale, color );
	}

	if( !cfg.title.empty() )
	{
		float titleSize = sizePx * 0.45f;
		float measured  = text.Measure( cfg.title, titleSize );
		if( measured > width * 0.92f )
			titleSize *= width * 0.92f / measured;
		const float color[ 4 ] = { cfg.textColor.r, cfg.textColor.g, cfg.textColor.b, 1.0f };
		text.AddText( cfg.title, centerX, height * 0.12f, titleSize, color );
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
			ChannelManager::Get().With( channel, [ index ]( Channel& c ) {
				if( index == P_START )
					c.Start( ChannelManager::Now() );
				else if( index == P_CLEAR )
					c.Clear();
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
		std::snprintf( text, sizeof( text ), "%.1f s", value );
		break;
	case P_SPEED:
		std::snprintf( text, sizeof( text ), "%.0f itens/s", value );
		break;
	default:
		return CFFGLPlugin::GetParameterDisplay( index );
	}
	displayBuffer.assign( text, text + std::strlen( text ) + 1 );
	return displayBuffer.data();
}

}//namespace sorteador

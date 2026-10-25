// Minimal FFGL host for macOS, used to see the plugin working without Resolume.
// It loads the bundle, renders into an offscreen OpenGL 4.1 context and saves frames as PPM images.
//
//   sorteador_test_host <Sorteador.bundle> <output dir> [seconds to keep serving the web panel]
#include <ffgl/FFGL.h>
#include <OpenGL/OpenGL.h>
#include <chrono>
#include <cstdio>
#include <dlfcn.h>
#include <string>
#include <thread>
#include <vector>

typedef FFMixed ( *PlugMainFn )( FFUInt32, FFMixed, FFInstanceID );

static const int kWidth  = 1280;
static const int kHeight = 720;
static PlugMainFn pluginMain;
static FFInstanceID instance;

static FFMixed Call( FFUInt32 code, FFMixed input = FFMixed{ 0 } )
{
	return pluginMain( code, input, instance );
}

static void SetFloat( unsigned int index, float value )
{
	SetParameterStruct sp;
	sp.ParameterNumber             = index;
	sp.NewParameterValue.UIntValue = *(FFUInt32*)&value;
	FFMixed in;
	in.PointerValue = &sp;
	Call( FF_SET_PARAMETER, in );
}

static void Render()
{
	glClearColor( 0, 0, 0, 0 );
	glClear( GL_COLOR_BUFFER_BIT );
	ProcessOpenGLStruct pgl{};
	FFMixed in;
	in.PointerValue = &pgl;
	Call( FF_PROCESS_OPENGL, in );
}

// Saves the frame composited over a dark checkerboard, so transparency is visible.
static void Save( const std::string& path )
{
	std::vector< unsigned char > pixels( kWidth * kHeight * 4 );
	glReadPixels( 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data() );
	FILE* f = std::fopen( path.c_str(), "wb" );
	std::fprintf( f, "P6\n%d %d\n255\n", kWidth, kHeight );
	for( int y = kHeight - 1; y >= 0; --y )
		for( int x = 0; x < kWidth; ++x )
		{
			const unsigned char* p = &pixels[ ( y * kWidth + x ) * 4 ];
			int checker            = ( ( x / 32 + y / 32 ) % 2 ) ? 40 : 60;
			for( int c = 0; c < 3; ++c )
			{
				// The plugin outputs premultiplied alpha.
				int v = p[ c ] + checker * ( 255 - p[ 3 ] ) / 255;
				std::fputc( v > 255 ? 255 : v, f );
			}
		}
	std::fclose( f );
	std::printf( "salvo %s\n", path.c_str() );
}

static void RenderFor( double seconds )
{
	auto end = std::chrono::steady_clock::now() + std::chrono::duration< double >( seconds );
	while( std::chrono::steady_clock::now() < end )
	{
		Render();
		std::this_thread::sleep_for( std::chrono::milliseconds( 16 ) );
	}
}

int main( int argc, char** argv )
{
	if( argc < 3 )
	{
		std::printf( "uso: %s <Sorteador.bundle> <pasta de saída> [segundos servindo]\n", argv[ 0 ] );
		return 1;
	}
	std::string out = argv[ 2 ];

	CGLPixelFormatAttribute attributes[] = {
		kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core,
		kCGLPFAColorSize, (CGLPixelFormatAttribute)24,
		kCGLPFAAlphaSize, (CGLPixelFormatAttribute)8,
		kCGLPFAAllowOfflineRenderers,
		(CGLPixelFormatAttribute)0
	};
	CGLPixelFormatObj format;
	GLint numFormats;
	CGLContextObj context;
	if( CGLChoosePixelFormat( attributes, &format, &numFormats ) != kCGLNoError || CGLCreateContext( format, nullptr, &context ) != kCGLNoError )
	{
		std::printf( "não foi possível criar o contexto OpenGL\n" );
		return 1;
	}
	CGLSetCurrentContext( context );

	GLuint fbo, color;
	glGenFramebuffers( 1, &fbo );
	glGenTextures( 1, &color );
	glBindTexture( GL_TEXTURE_2D, color );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
	glBindTexture( GL_TEXTURE_2D, 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0 );
	glViewport( 0, 0, kWidth, kHeight );

	std::string binary = std::string( argv[ 1 ] ) + "/Contents/MacOS/Sorteador";
	void* lib          = dlopen( binary.c_str(), RTLD_NOW );
	if( !lib || !( pluginMain = (PlugMainFn)dlsym( lib, "plugMain" ) ) )
	{
		std::printf( "não foi possível carregar %s: %s\n", binary.c_str(), dlerror() );
		return 1;
	}

	Call( FF_INITIALISE_V2 );
	FFGLViewportStruct viewport = { 0, 0, kWidth, kHeight };
	FFMixed in;
	in.PointerValue = &viewport;
	instance        = (FFInstanceID)Call( FF_INSTANTIATE_GL, in ).PointerValue;
	if( !instance || instance == (FFInstanceID)FF_FAIL )
	{
		std::printf( "falha ao instanciar o plugin\n" );
		return 1;
	}

	// The font is baked in the background, give it a moment.
	RenderFor( 2.0 );
	Save( out + "/0-espera.ppm" );

	if( argc >= 4 )
	{
		// Keep running so the web panel can be exercised; a frame is saved every second.
		int seconds = std::atoi( argv[ 3 ] );
		for( int s = 0; s < seconds; ++s )
		{
			RenderFor( 1.0 );
			Save( out + "/serve-" + std::to_string( s ) + ".ppm" );
		}
	}
	else
	{
		SetFloat( 0, 1.0f );// "Iniciar sorteio" pressed...
		SetFloat( 0, 0.0f );// ...and released
		double marks[] = { 0.4, 1.5, 3.0, 5.5, 6.3, 7.5 };
		double elapsed = 0.0;
		for( double mark : marks )
		{
			RenderFor( mark - elapsed );
			elapsed = mark;
			char name[ 64 ];
			std::snprintf( name, sizeof( name ), "/1-sorteio-%.1fs.ppm", mark );
			Save( out + name );
		}
	}

	Call( FF_DEINSTANTIATE_GL );
	instance = nullptr;
	Call( FF_DEINITIALISE );
	CGLDestroyContext( context );
	return 0;
}

// Tests for the draw logic. Runs without Resolume or OpenGL: build and run `sorteador_tests`.
#include "Channel.h"
#include <cmath>
#include <cstdio>
#include <set>

using namespace sorteador;

static int failures = 0;
#define CHECK( cond )                                                    \
	do                                                                   \
	{                                                                    \
		if( !( cond ) )                                                  \
		{                                                                \
			std::printf( "FALHOU %s:%d  %s\n", __FILE__, __LINE__, #cond ); \
			++failures;                                                  \
		}                                                                \
	} while( 0 )

static const VisibleItem* CenterItem( const FrameView& frame )
{
	const VisibleItem* best = nullptr;
	for( const VisibleItem& item : frame.items )
		if( !best || std::fabs( item.offset ) < std::fabs( best->offset ) )
			best = &item;
	return best;
}

// Runs one full draw and returns the winner.
static std::string Draw( Channel& c, double& now )
{
	if( !c.Start( now ) )
		return {};
	now += c.config.duration + 0.01;
	c.Tick( now );
	return c.GetWinner();
}

static void TestItems()
{
	Channel c;
	c.config.listText = "  Ana \n\nBruno\r\n Carla ";
	CHECK( c.Items() == std::vector< std::string >( { "Ana", "Bruno", "Carla" } ) );
	c.config.listText = "Ana; Bruno ;Carla;;";
	CHECK( c.Items() == std::vector< std::string >( { "Ana", "Bruno", "Carla" } ) );
}

static void TestFullDraw()
{
	Channel c;
	c.config.listText = "Ana\nBruno\nCarla\nDaniel";
	c.config.duration = 3.0f;
	double now        = 100.0;
	CHECK( c.Start( now ) );
	CHECK( c.GetPhase() == Phase::Spinning );
	CHECK( !c.Start( now + 0.5 ) );// already running

	// Just before the end the reel must already sit on the winner, so the stop has no jump.
	FrameView almost = c.BuildFrame( now + 2.999 );
	CHECK( CenterItem( almost ) && CenterItem( almost )->text == c.GetWinner() );

	c.Tick( now + 3.01 );
	CHECK( c.GetPhase() == Phase::Result );
	CHECK( c.GetHistory().size() == 1 && c.GetHistory()[ 0 ] == c.GetWinner() );
	FrameView result = c.BuildFrame( now + 4.0 );
	CHECK( CenterItem( result )->text == c.GetWinner() );
	CHECK( CenterItem( result )->winnerMix == 1.0f );

	c.Clear();
	CHECK( c.GetPhase() == Phase::Idle );
	CHECK( c.BuildFrame( now + 5 ).items.size() == 1 );
	CHECK( c.BuildFrame( now + 5 ).items[ 0 ].text == c.config.idleText );
}

static void TestNoRepeatList()
{
	Channel c;
	c.config.listText = "A\nB\nC\nD\nE\nA";// "A" twice: may come out twice
	c.config.duration = 1.0f;
	double now        = 0.0;
	std::multiset< std::string > drawn;
	for( int i = 0; i < 6; ++i )
		drawn.insert( Draw( c, now ) );
	CHECK( drawn.count( "A" ) == 2 );
	CHECK( drawn.count( "E" ) == 1 );
	CHECK( c.AvailableCount() == 0 );
	CHECK( !c.Start( now ) );
	CHECK( !c.GetMessage().empty() );

	c.ResetHistory();
	CHECK( c.AvailableCount() == 6 );
}

static void TestNumbers()
{
	Channel c;
	c.config.mode     = DrawMode::Numbers;
	c.config.numMin   = 1;
	c.config.numMax   = 5;
	c.config.duration = 1.0f;
	double now        = 0.0;
	std::set< std::string > drawn;
	for( int i = 0; i < 5; ++i )
		drawn.insert( Draw( c, now ) );
	CHECK( drawn == std::set< std::string >( { "1", "2", "3", "4", "5" } ) );
	CHECK( !c.Start( now ) );

	c.ResetHistory();
	c.config.numMax  = 100;
	c.config.zeroPad = true;
	std::string n    = Draw( c, now );
	CHECK( n.size() == 3 );
	CHECK( std::stoi( n ) >= 1 && std::stoi( n ) <= 100 );

	// Huge ranges use rejection sampling instead of listing every number.
	c.config.numMin  = 0;
	c.config.numMax  = 2000000000;
	c.config.zeroPad = false;
	CHECK( !Draw( c, now ).empty() );
}

static void TestRepeatAllowed()
{
	Channel c;
	c.config.listText = "Só um";
	c.config.noRepeat = false;
	c.config.duration = 1.0f;
	double now        = 0.0;
	CHECK( Draw( c, now ) == "Só um" );
	CHECK( Draw( c, now ) == "Só um" );
	CHECK( c.GetHistory().size() == 2 );
}

static void TestEmptyList()
{
	Channel c;
	c.config.listText = " \n ";
	CHECK( !c.Start( 0.0 ) );
	CHECK( c.GetPhase() == Phase::Idle );
}

static void TestJson()
{
	DrawConfig a;
	a.listText  = "X\nY";
	a.duration  = 12.5f;
	a.animation = Animation::Flip;
	a.textColor = { 0.1f, 0.2f, 0.3f };
	DrawConfig b;
	MergeJson( b, ToJson( a ) );
	CHECK( b.listText == "X\nY" );
	CHECK( b.duration == 12.5f );
	CHECK( b.animation == Animation::Flip );
	CHECK( std::fabs( b.textColor.g - 0.2f ) < 1e-6f );

	// Partial updates leave the rest alone, out of range values are clamped.
	MergeJson( b, { { "duration", 999 }, { "numMin", 50 }, { "numMax", 10 } } );
	CHECK( b.duration == 60.0f );
	CHECK( b.numMin == 10 && b.numMax == 50 );
	CHECK( b.listText == "X\nY" );
}

int main()
{
	TestItems();
	TestFullDraw();
	TestNoRepeatList();
	TestNumbers();
	TestRepeatAllowed();
	TestEmptyList();
	TestJson();
	if( failures == 0 )
		std::printf( "Todos os testes passaram.\n" );
	return failures == 0 ? 0 : 1;
}

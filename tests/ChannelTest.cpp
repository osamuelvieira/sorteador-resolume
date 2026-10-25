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
	CHECK( c.GetHistory().size() == 1 && c.GetHistory()[ 0 ].text == c.GetWinner() && c.GetHistory()[ 0 ].time > 0 );
	FrameView result = c.BuildFrame( now + 4.0 );
	CHECK( CenterItem( result )->text == c.GetWinner() );
	CHECK( CenterItem( result )->winnerMix == 1.0f );

	c.Clear( now + 4.5 );
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

static void TestTestDraw()
{
	Channel c;
	c.config.listText = "Ana\nBruno";
	c.config.duration = 1.0f;
	double now        = 0.0;
	Draw( c, now );
	Draw( c, now );
	CHECK( c.AvailableCount() == 0 );

	// A test still runs when everyone was drawn, and leaves the real draw and the history alone.
	CHECK( c.Start( now, true ) );
	CHECK( c.GetPhase( true ) == Phase::Spinning );
	CHECK( c.GetPhase() == Phase::Result );
	c.Tick( now + 1.01 );
	CHECK( c.GetPhase( true ) == Phase::Result );
	CHECK( c.GetHistory().size() == 2 );
	CHECK( CenterItem( c.BuildFrame( now + 2.0, true ) )->text == c.GetWinner( true ) );
	CHECK( c.BuildFrame( now + 2.0, true ).sinceResult > 0.9f );
	CHECK( c.BuildFrame( now + 2.0 ).items.back().text == c.GetWinner() );

	c.Clear( now, true );
	CHECK( c.GetPhase( true ) == Phase::Idle );
	CHECK( c.GetPhase() == Phase::Result );
}

static void TestRedraw()
{
	Channel c;
	c.config.listText = "Ana\nBruno\nCarla";
	c.config.duration = 1.0f;
	double now        = 0.0;
	CHECK( !c.Redraw( now ) );// nothing on screen yet
	CHECK( !c.GetMessage().empty() );

	std::string first = Draw( c, now );
	CHECK( c.Redraw( now ) );
	CHECK( c.GetPhase() == Phase::Spinning );
	CHECK( c.GetHistory().size() == 1 && c.GetHistory()[ 0 ].absent );
	CHECK( c.GetMessage().empty() );
	now += 1.01;
	c.Tick( now );
	// The absent one keeps counting as drawn, so the new winner is someone else.
	CHECK( c.GetWinner() != first );
	CHECK( c.GetHistory().size() == 2 && !c.GetHistory()[ 1 ].absent );
	CHECK( c.AvailableCount() == 1 );
	CHECK( HistoryToJson( c.GetHistory() )[ 0 ][ "absent" ] == true );

	// Only once per winner, and not during a spin.
	CHECK( c.Redraw( now ) );
	CHECK( !c.Redraw( now ) );
	c.Clear( now );
	CHECK( !c.Redraw( now ) );
}

static void TestUndoReset()
{
	Channel c;
	c.config.listText = "Ana\nBruno";
	c.config.duration = 1.0f;
	double now        = 0.0;
	CHECK( !c.UndoReset() );
	Draw( c, now );
	c.ResetHistory();
	CHECK( c.CanUndoReset() );
	c.ResetHistory();// a second reset must not lose the first one
	CHECK( c.UndoReset() );
	CHECK( c.GetHistory().size() == 1 );
	CHECK( !c.CanUndoReset() );

	// Once someone is drawn after the reset, the old history is gone for good.
	c.ResetHistory();
	Draw( c, now );
	CHECK( !c.CanUndoReset() );
	CHECK( !c.UndoReset() );
	CHECK( c.GetHistory().size() == 1 );
}

static void TestDetails()
{
	CHECK( ItemName( "Ana Beatriz | Recife" ) == "Ana Beatriz" );
	CHECK( ItemDetail( "Ana Beatriz | Recife" ) == "Recife" );
	CHECK( ItemName( "Bruno" ) == "Bruno" && ItemDetail( "Bruno" ).empty() );

	Channel c;
	c.config.listText = "Ana | Recife\nBruno | Olinda\nCarla";
	c.config.duration = 1.0f;
	double now        = 0.0;
	CHECK( c.Start( now ) );
	// The reel only shows names.
	for( const std::string& entry : c.GetRun().segments[ 0 ].reels[ 0 ].entries )
		CHECK( entry.find( '|' ) == std::string::npos );
	now += 1.01;
	c.Tick( now );
	FrameView frame = c.BuildFrame( now + 0.5 );
	CHECK( CenterItem( frame )->text == ItemName( c.GetWinner() ) );
	CHECK( frame.detail == ItemDetail( c.GetWinner() ) );
	// The history keeps the whole line, details included.
	CHECK( c.GetHistory()[ 0 ].text == c.GetWinner() );
}

static void TestAutoClear()
{
	Channel c;
	c.config.listText  = "Ana\nBruno";
	c.config.duration  = 1.0f;
	c.config.autoClear = 3.0f;
	double now         = 0.0;
	Draw( c, now );// result at 1.0
	CHECK( c.BuildFrame( 2.0 ).fade == 1.0f );
	CHECK( c.BuildFrame( 3.7 ).fade < 1.0f );// fading out before going back
	c.Tick( 3.9 );
	CHECK( c.GetPhase() == Phase::Result );
	c.Tick( 4.02 );
	CHECK( c.GetPhase() == Phase::Idle );
	// The waiting text fades in from the moment the result left.
	CHECK( c.BuildFrame( 4.1 ).fade < 1.0f );
	CHECK( c.BuildFrame( 5.0 ).fade == 1.0f );

	c.config.autoClear = 0.0f;// never
	Draw( c, now );
	c.Tick( now + 100.0 );
	CHECK( c.GetPhase() == Phase::Result );
}

static void TestMigration()
{
	DrawConfig c;
	c.textSize  = 0.2f;
	c.textColor = { 1.0f, 0.0f, 0.0f };
	MigrateTo13( c );
	CHECK( std::fabs( c.titleSize - 0.09f ) < 1e-6f );
	CHECK( c.titleColor.g == 0.0f && c.idleColor.g == 0.0f );
	CHECK( c.idleSize == 0.2f );
	CHECK( !c.drum && !c.wrapLongNames );
}

static std::string Joined( const std::vector< ColumnView >& columns )
{
	std::string text;
	for( const ColumnView& c : columns )
		text += c.finalText;
	return text;
}

static void TestSeveralWinners()
{
	Channel c;
	c.config.listText  = "A\nB\nC\nD\nE\nF";
	c.config.duration  = 2.0f;
	c.config.drawCount = 4;
	c.config.reveal    = 1;// grid
	double now         = 0.0;
	CHECK( c.Start( now ) );
	const Run& r = c.GetRun();
	CHECK( r.segments.size() == 4 && r.IsGrid() );
	std::set< std::string > winners;
	for( const Segment& seg : r.segments )
		winners.insert( seg.winner );
	CHECK( winners.size() == 4 );// all different
	CHECK( c.GetWinner().empty() );
	// They stop one after another, each one going to the history as it lands.
	c.Tick( r.segments[ 0 ].Landing() + 0.01 );
	CHECK( c.GetHistory().size() == 1 && c.GetPhase() == Phase::Spinning );
	FrameView middle = c.BuildFrame( r.segments[ 0 ].Landing() + 0.01 );
	CHECK( middle.cells.size() == 4 && middle.cells[ 0 ].sinceLanded >= 0.0f && middle.cells[ 3 ].sinceLanded < 0.0f && !middle.landed );
	c.Tick( r.EndTime() + 0.01 );
	CHECK( c.GetHistory().size() == 4 && c.GetPhase() == Phase::Result );
	CHECK( c.BuildFrame( r.EndTime() + 0.5 ).landed );
	CHECK( !c.Redraw( now ) );// "absent" is for single winners

	// More than are left: draws the rest and says so.
	c.Clear( 10.0 );
	CHECK( c.Start( 10.0 ) );
	CHECK( c.GetRun().segments.size() == 2 );
	CHECK( !c.GetMessage().empty() );
}

static void TestOneByOne()
{
	Channel c;
	c.config.listText  = "A\nB\nC";
	c.config.duration  = 2.0f;
	c.config.drawCount = 3;
	c.config.revealGap = 3.0f;
	CHECK( c.Start( 0.0 ) );
	const Run& r = c.GetRun();
	CHECK( r.segments[ 1 ].start == 5.0 && r.segments[ 2 ].start == 10.0 );
	// While the second spins, the first waits in the tray.
	FrameView second = c.BuildFrame( 6.0 );
	CHECK( second.tray.size() == 1 && second.tray[ 0 ] == r.segments[ 0 ].winner && !second.landed );
	FrameView landed = c.BuildFrame( 7.5 );
	CHECK( landed.landed && CenterItem( landed )->text == r.segments[ 1 ].winner );
	c.Tick( 12.01 );
	CHECK( c.GetPhase() == Phase::Result && c.GetHistory().size() == 3 );
	// After the last one's moment alone, all of them together.
	CHECK( c.BuildFrame( 13.0 ).cells.empty() );
	CHECK( c.BuildFrame( 15.5 ).cells.size() == 3 );
}

static void TestNumbersSeveral()
{
	Channel c;
	c.config.mode      = DrawMode::Numbers;
	c.config.numMin    = 1;
	c.config.numMax    = 3;
	c.config.noRepeat  = false;
	c.config.drawCount = 5;
	CHECK( c.Start( 0.0 ) );
	// Even when numbers may repeat across draws, one draw never gives the same number twice.
	std::set< std::string > winners;
	for( const Segment& seg : c.GetRun().segments )
		winners.insert( seg.winner );
	CHECK( winners.size() == 3 && c.GetRun().segments.size() == 3 );
}

static void TestSlot()
{
	Channel c;
	c.config.mode      = DrawMode::Numbers;
	c.config.numMin    = 1;
	c.config.numMax    = 500;
	c.config.zeroPad   = true;
	c.config.animation = Animation::Slot;
	c.config.duration  = 4.0f;
	CHECK( c.Start( 0.0 ) );
	const Segment& seg = c.GetRun().segments[ 0 ];
	CHECK( seg.reels.size() == 3 );// one per digit
	CHECK( seg.reels[ 0 ].stop < seg.reels[ 1 ].stop && seg.reels[ 1 ].stop < seg.reels[ 2 ].stop );
	CHECK( std::fabs( seg.reels[ 2 ].stop - 4.0 ) < 1e-9 );
	FrameView spinning = c.BuildFrame( 1.0 );
	CHECK( spinning.columns.size() == 3 && Joined( spinning.columns ) == seg.winner );
	// Left digit already stopped, right one still rolling.
	FrameView partly = c.BuildFrame( seg.reels[ 0 ].stop + 0.01 );
	CHECK( partly.columns[ 0 ].items.size() == 1 && partly.columns[ 2 ].items.size() > 1 );
	FrameView done = c.BuildFrame( 4.5 );
	CHECK( done.columns.empty() && done.landed && CenterItem( done )->text == seg.winner );

	// Names keep their spaces still.
	Channel n;
	n.config.listText  = "Ana Lu\nBia Sá";
	n.config.animation = Animation::Slot;
	CHECK( n.Start( 0.0 ) );
	const Segment& name = n.GetRun().segments[ 0 ];
	CHECK( name.reels.size() == 6 && name.reels[ 3 ].stop == 0.0 );
	CHECK( Joined( n.BuildFrame( 0.5 ).columns ) == name.winner );
}

static void TestWheel()
{
	Channel c;
	c.config.listText  = "A\nB\nC\nD\nE";
	c.config.animation = Animation::Wheel;
	c.config.duration  = 3.0f;
	CHECK( c.Start( 0.0 ) );
	const Reel& wheel = c.GetRun().segments[ 0 ].reels[ 0 ];
	CHECK( wheel.entries.size() == 5 );
	FrameView end = c.BuildFrame( 3.0 );
	// The slice under the pointer at the top is the winner: slice i sits at -90 degrees + i * slice - rotation.
	const double slice = 2.0 * 3.14159265358979 / 5.0;
	const double angle = std::fmod( end.wheel.winnerIndex * slice - end.wheel.rotation, 2.0 * 3.14159265358979 );
	CHECK( std::fabs( angle ) < 1e-3 || std::fabs( std::fabs( angle ) - 2.0 * 3.14159265358979 ) < 1e-3 );
	CHECK( end.wheel.entries[ (size_t)end.wheel.winnerIndex ] == c.GetWinner() );
	CHECK( end.wheel.rotation > 2.0f * 2.0f * 3.14159f );// at least two full turns
	CHECK( c.BuildFrame( 1.0 ).items.empty() && c.BuildFrame( 1.0 ).wheel.alpha == 1.0f );
	CHECK( CenterItem( c.BuildFrame( 3.5 ) )->text == c.GetWinner() );
}

static void TestCountdown()
{
	Channel c;
	c.config.listText  = "A\nB";
	c.config.countdown = 3.0f;
	c.config.duration  = 2.0f;
	CHECK( c.Start( 0.0 ) );
	CHECK( c.GetRun().StartTime() == 3.0 );
	FrameView three = c.BuildFrame( 0.1 );
	CHECK( three.countdown && three.items[ 0 ].text == "3" );
	CHECK( c.BuildFrame( 1.5 ).items[ 0 ].text == "2" );
	CHECK( c.BuildFrame( 2.9 ).items[ 0 ].text == "1" );
	CHECK( !c.BuildFrame( 3.1 ).countdown );
	CHECK( c.Progress( 2.0 ) == 0.0 );
	c.Tick( 5.01 );
	CHECK( c.GetHistory().size() == 1 );
}

static void TestBoard()
{
	Channel c;
	c.config.listText = "A\nB\nC";
	c.config.duration = 1.0f;
	double now        = 0.0;
	Draw( c, now );
	Draw( c, now );
	c.Redraw( now );// second one absent: stays out of the board
	now += 1.01;
	c.Tick( now );
	CHECK( c.ShowBoard( now ) );
	FrameView board = c.BuildFrame( now + 1.0 );
	CHECK( board.board && board.cells.size() == 2 );
	c.Clear( now + 2.0 );
	CHECK( c.GetPhase() == Phase::Idle );
	c.config.noRepeat = false;// everyone was drawn
	CHECK( c.Start( now + 3.0 ) );
	CHECK( !c.ShowBoard( now + 3.1 ) );// not while spinning
}

static void TestJson()
{
	DrawConfig a;
	a.listText  = "X\nY";
	a.duration  = 12.0f;
	a.uppercase = true;
	a.animation = Animation::Flip;
	a.textColor = { 0.1f, 0.2f, 0.3f };
	DrawConfig b;
	a.fxConfetti  = true;
	a.fxFlash     = false;
	a.fxIntensity = 1.5f;
	MergeJson( b, ToJson( a ) );
	CHECK( b.fxConfetti && !b.fxFlash && b.fxIntensity == 1.5f );
	CHECK( b.listText == "X\nY" );
	CHECK( b.duration == 12.0f );
	CHECK( b.uppercase );

	// Duration and deceleration are whole numbers.
	MergeJson( b, { { "duration", 7.6 }, { "decel", 2.4 } } );
	CHECK( b.duration == 8.0f && b.decel == 2.0f );
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
	TestTestDraw();
	TestRedraw();
	TestUndoReset();
	TestDetails();
	TestAutoClear();
	TestMigration();
	TestSeveralWinners();
	TestOneByOne();
	TestNumbersSeveral();
	TestSlot();
	TestWheel();
	TestCountdown();
	TestBoard();
	TestJson();
	if( failures == 0 )
		std::printf( "Todos os testes passaram.\n" );
	return failures == 0 ? 0 : 1;
}

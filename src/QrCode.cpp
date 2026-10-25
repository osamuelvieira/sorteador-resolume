#include "QrCode.h"
#include <qrcodegen/qrcodegen.hpp>

namespace sorteador
{
std::vector< std::string > QrModules( const std::string& text )
{
	std::vector< std::string > rows;
	try
	{
		// Medium error correction still reads well when the screen is filmed from a distance.
		const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText( text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM );
		for( int y = 0; y < qr.getSize(); ++y )
		{
			std::string row;
			for( int x = 0; x < qr.getSize(); ++x )
				row += qr.getModule( x, y ) ? '1' : '0';
			rows.push_back( row );
		}
	}
	catch( const std::exception& )
	{
		rows.clear();
	}
	return rows;
}

}//namespace sorteador

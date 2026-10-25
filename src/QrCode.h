#pragma once
#include <string>
#include <vector>

namespace sorteador
{
// QR code of the text, one string per row with '1' for a dark module. No quiet zone; empty if it cannot be encoded.
std::vector< std::string > QrModules( const std::string& text );

}//namespace sorteador

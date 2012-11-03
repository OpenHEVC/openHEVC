/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.  
 *
 * Copyright (c) 2010-2012, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <cassert>
#include "ExtractBitrates.h"

/// In out, finds the first occurence of toFind and replaces it with "e"
/// \pre toFind must have a size of 2
/// \pre The first character in toFind muts be 'e'
/// \pre out must contain toFind
void replaceWithE( std::string &out, const std::string& toFind )
{
  assert( 2 == toFind.size( ) );
  assert( 'e' == toFind[ 0 ] );
  
  std::string::size_type pos( out.find( toFind ) );
  assert( pos != std::string::npos );
  out.erase( pos + 1, 1 );
}

/// Formatted output for a double with appropriate formatting applied (correct number of digits, etc.)
void outputDouble( std::ostream& left, double right )
{
  std::ostringstream oss;
  oss.precision( 6 );
  oss << std::scientific << right;
  std::string s( oss.str( ) );
  
  replaceWithE( s, "e+" );
  replaceWithE( s, "e0" );
  
  left << s;
}

int main( int, char** )
{
  try
  {
    std::vector< double > result( extractBitratesForTemporalLayers( std::cin ) );  // Extract the bitrate vector
    
    // Output the bitrate vector
    if( 0 < result.size( ) )
    {
      std::vector< double >::const_iterator iter( result.begin( ) );
      outputDouble( std::cout, *iter );
      for( ; ; )
      {
        ++iter;
        if( result.end( ) == iter )
        {
          break;
        }
        else
        {
          std::cout << " ";
          outputDouble( std::cout, *iter );
        }
      }
    }
    
    return 0;
  }
  catch( std::exception& e )
  {
    std::cerr << e.what( ) << std::endl;
    return 1;
  }
}

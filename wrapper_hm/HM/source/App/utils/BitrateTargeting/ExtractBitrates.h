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

#ifndef EXTRACT_BITRATES_H
#define EXTRACT_BITRATES_H

#include "RuntimeError.h"
#include <vector>

/// An error occured while parsing a POC line from within a log file
class POCParseException: public RuntimeError
{
  public:
    POCParseException( const std::string& pocLine ): m_pocLine( pocLine ) { }
    virtual ~POCParseException( ) throw ( ) { }
  
  protected:
    void outputWhat( std::ostream& o ) const { o << "POC parse exception: " << m_pocLine; }
  
  private:
    std::string m_pocLine;
};

/// The QP set from the log file was not contiguous.  The QP set must be contiguous to be able to convert the results into a vector.
class NonContiguousQPSetException: public RuntimeError
{
  public:
    virtual ~NonContiguousQPSetException( ) throw( ) { }
  
  protected:
    void outputWhat( std::ostream& o ) const { o << "Non-contiguous QP set exception"; }
};

/// Extracts the average bitrates for each of the temporal layers from the given log
/// \param i The input stream that represents the log
/// \return A vector of doubles that contains the average bitrates for each temporal layer
/// \throw POCParseException if an error occured while parsing a POC line
/// \throw NonContiguousQPSetException if the QP set from the log file was not contiguous
std::vector< double > extractBitratesForTemporalLayers( std::istream& i );

#endif

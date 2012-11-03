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

#ifndef DIRECTORY_LIB_RUNTIME_ERROR_H
#define DIRECTORY_LIB_RUNTIME_ERROR_H

#include <sstream>
#include <stdexcept>

/// This class serves the same purpose as std::runtime_error, but it can be more convenient to use
class RuntimeError: public std::runtime_error
{
  public:
    RuntimeError( ): std::runtime_error( "" ), m_firstWhat( true ) { }
    virtual ~RuntimeError( ) throw ( ) { }
    
    /// Implementation of the std::exception::what method
    const char * what( ) const throw( )
    {
      if( m_firstWhat )
      {
        std::ostringstream o;
        outputWhat( o );
        m_what = o.str( );
        m_firstWhat = false;
      }
      return m_what.c_str( );
    }
    
  protected:
    /// The implementing class implements this method to customize the what output
    /// \param o The what stream is outputted to this parameter
    virtual void outputWhat( std::ostream & o ) const =0;
  
  private:
    mutable bool m_firstWhat;  ///< True i.f.f. the what method has not yet been called
    mutable std::string m_what;  ///< Contains the what string.  Populated by the first call to the what method.
};

/// Convenient formatted output operator that just outputs the what string
inline std::ostream& operator<<( std::ostream& left, const RuntimeError& right )
{
  return left << right.what( );
}

#endif

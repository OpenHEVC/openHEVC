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

#ifndef CALCULATE_LAMBDA_MODIFIER_H
#define CALCULATE_LAMBDA_MODIFIER_H

#include "RuntimeError.h"
#include <vector>
#include <map>
#include <list>
#include <set>
#include <limits>

/// Thrown if there is an error parsing the initial adjustment parameter
class InitialAdjustmentParameterParseException: public RuntimeError
{
  public:
    virtual ~InitialAdjustmentParameterParseException( ) throw( ) { }
  protected:
    void outputWhat( std::ostream& o ) const { o << "Error parsing the initial-adjustment parameter"; }
};

/// Thrown if there is an error parsing the targets
class TargetsParseException: public RuntimeError
{
  public:
    virtual ~TargetsParseException( ) throw( ) { }
  protected:
    void outputWhat( std::ostream& o ) const { o << "Error parsing targets"; }
};

/// Thrown if there is an error parsing the meta-log
class MetaLogParseException: public RuntimeError
{
  public:
    virtual ~MetaLogParseException( ) throw( ) { }
  protected:
    void outputWhat( std::ostream& o ) const { o << "Error parsing meta log"; }
};

/// Thrown if there is a mismatch in the vector sizes or the Lambda-modifier indexes
class MismatchedIndexesException: public RuntimeError
{
  public:
    virtual ~MismatchedIndexesException( ) throw( ) { }
  protected:
    void outputWhat( std::ostream& o ) const { o << "Mismatched vector sizes or lambda modifier indexes"; }
};

/// Full meta-log entry
template< typename TLambdaModifier >
struct MetaLogEntry
{
  TLambdaModifier lambdaModifiers;
  std::vector< double > bitrateVector;
};

/// Contains a Lambda-modifier and bitrate for only a single index
struct Point
{
  double lambdaModifier;
  double bitrate;
};

/// Performs interpolation/extrapolation to guess a single Lambda-modifier
/// \param target The target bitrate value that this Lambda-modifier is trying to reach
/// \param point1 One of the two previously tried points where first is the Lambda-modifier and second is the obtained bitrate
/// \param point2 One of the two previously tried points where first is the Lambda-modifier and second is the obtained bitrate
/// \return The interpolated Lambda-modifier guess
/// \pre Both given points must contain only positive non-zero values for first and second
/// \pre The given points must have different first values and different second values.  If either the first values are the same or the second values are the same, then we have either a vertical or horizontal slope, and thus, interpolation cannot be performed.
double polateLambdaModifier( double targetBitrate, const Point& point1, const Point& point2 );

/// Guesses a single Lambda-modifier
/// \param initialAdjustmentParameter If interpolation/extrapolation cannot be performed, then this parameter is used in the "increment" process.
/// \param targetBitrate The target bitrate value that this Lambda-modifier is trying to reach
/// \param pointList The list of points that correspond with this index
/// \param interDampeningFactor This factor is obtained based on guessed Lambda-modifiers for previous temporal layers.  In some cases, this factor will scale down the change of this Lambda-modifier so that we are not making too many severe Lambda-modifier changes for a single encoder run.
/// \return The Lambda-modifier guess
/// \pre pointList cannot be empty
/// \pre interDampeningFactor must be greater than zero and less than or equal to 1 (0 < interDampeningFactor <= 1)
double guessLambdaModifier(
    double initialAdjustmentParameter,
    double targetBitrate,
    const std::list< Point >& pointList,
    double interDampeningFactor );

/// Guesses all of the Lambda-modifiers
/// \param initialAdjustmentParameter If interpolation/extrapolation cannot be performed, then this parameter is used in the "increment" process.
/// \param targetBitrateVector The target bitrate values that we are trying to reach
/// \param metaLogEntryList All of the previously run Lambda-modifiers and their corresponding bitrates from the meta-log
/// \return Vector containing all of the guessed Lambda-modifiers
/// \pre targetBitrateVector cannot be empty
/// \pre metaLogEntryList cannot be empty
/// \pre The size of targetBitrateVector must be the same as the size of bitrateVector in every item in metaLogEntryList
/// \pre The size of targetBitrateVector must be the same as the size of lambdaModifiers in every item in metaLogEntryList
std::vector< double > guessLambdaModifiers(
    double initialAdjustmentParameter,
    const std::vector< double > &targetBitrateVector,
    const std::list< MetaLogEntry< std::vector< double > > >& metaLogEntryList );

/// Guesses all of the Lambda-modifiers
/// This function performs all of the necessary input parsing.  It ends up calling the other guessLambdaModifiers overload to perform the actual calculations.
/// \param o The output stream to write the guessed Lambda-modifiers to
/// \param initialAdjustmentParameterIstream The input stream that contains the initial adjustment parameter
/// \param targetIstream The input stream that contains the target bitrates
/// \param metaLogIstream The input stream that contains the meta-log
/// \throw InitialAdjustmentParameterParseException if there is an error parsing the initial adjustment parameter
/// \throw TargetsParseException if there is an error parsing the target bitrates
/// \throw MetaLogParseException if there is an error parsing the meta-log
/// \throw MismatchedIndexesException if there is a mismatch in the vector sizes or the Lambda-modifier indexes
void guessLambdaModifiers(
    std::ostream& o,
    std::istream& initialAdjustmentParameterIstream,
    std::istream& targetsIstream,
    std::istream& metaLogIstream );

#endif

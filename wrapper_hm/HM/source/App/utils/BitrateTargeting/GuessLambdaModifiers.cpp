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

#include "GuessLambdaModifiers.h"
#include <limits>
#include <cassert>
#include <cmath>

namespace
{
  /// Formatted input for a bitrate vector
  /// \param left The input stream that contains the bitrate vector
  /// \param right The vector to be written to
  /// \pre right must be empty
  void parseBitrateVector( std::istream& left, std::vector< double >& right )
  {
    assert( right.empty( ) );
    
    for( ; ; )
    {
      assert( left.good( ) );
      
      double bitrate;
      left >> bitrate;
      if( left.fail( ) ) break;
      if( bitrate <= ( double )0.0 )
      {
        left.setstate( std::istream::failbit );
      }
      else
      {
        right.push_back( bitrate );
      }
      if( !left.good( ) ) break;
      
      if( left.peek( ) == ' ' )
      {
        left.ignore( );
      }
      else
      {
        break;
      }
    }
  }
  
  /// Makes a next guess for a single Lambda-modifier based on only one previous guess
  /// \param initialAdjustmentParameter The proportionality to use between the target bitrate and the previous guess
  /// \param target The target bitrate value that this Lambda-modifier is trying to reach
  /// \param previousPoint The previous guess
  /// \return The Lambda-modifier guess
  /// \pre The given point must contain only positive non-zero values
  double incrementLambdaModifier(
      double initialAdjustmentParameter,
      double targetBitrate,
      const Point& previousPoint )
  {
    assert( ( double )0.0 < previousPoint.lambdaModifier );
    assert( ( double )0.0 < previousPoint.bitrate );
    
    double extrapolated( previousPoint.lambdaModifier * targetBitrate / previousPoint.bitrate );
    return previousPoint.lambdaModifier + initialAdjustmentParameter * ( extrapolated - previousPoint.lambdaModifier );
  }
}

double polateLambdaModifier( double targetBitrate, const Point& point1, const Point& point2 )
{
  assert( 0.0 < point1.lambdaModifier );
  assert( 0.0 < point2.lambdaModifier );
  assert( 0.0 < point1.bitrate );
  assert( 0.0 < point2.bitrate );
  assert( point1.lambdaModifier != point2.lambdaModifier );
  assert( point1.bitrate != point2.bitrate );
  
  // Calculate and return the result
  double denominator( point1.bitrate - point2.bitrate );
  double result( point1.lambdaModifier
      + ( point1.lambdaModifier - point2.lambdaModifier ) / denominator * ( targetBitrate - point1.bitrate ) );
  return result;
}

double guessLambdaModifier(
    double initialAdjustmentParameter,
    double targetBitrate,
    const std::list< Point >& pointList,
    double interDampeningFactor )
{
  assert( ( double )0.0 < interDampeningFactor );
  assert( interDampeningFactor <= ( double )1.0 );
  assert( !pointList.empty( ) );
  
  double preliminaryResult;
  
  if( 1 == pointList.size( ) )  // If there is only one prevous point, then we cannot interpolate, so we call incrementLambdaModifier
  {
    preliminaryResult = incrementLambdaModifier( initialAdjustmentParameter, targetBitrate, pointList.back( ) );
  }
  else  // If there are at least two previous points, then we may be able to interpolate
  {
    std::list< Point >::const_reverse_iterator i( pointList.rbegin( ) );
    Point point1 = *i;
    ++i;
    Point point2 = *i;
    
    // If the slope is either horizontal or vertical, we cannot interpolate
    if( point1.lambdaModifier == point2.lambdaModifier || point1.bitrate == point2.bitrate )
    {
      preliminaryResult = incrementLambdaModifier( initialAdjustmentParameter, targetBitrate, pointList.back( ) );
    }
    else  // If the slope is not horizontal and not vertical, we can interpolate
    {
      preliminaryResult = polateLambdaModifier( targetBitrate, point1, point2 );
    }
  }
  
  double previousResult( pointList.back( ).lambdaModifier );
  
  // Apply "intra dampening"
  {
    double intermediate( std::log( ( double )1.0 + std::abs( preliminaryResult - previousResult ) / previousResult ) );
    assert( ( double )0.0 <= intermediate );
    if( ( preliminaryResult - previousResult ) < 0.0 )
    {
      preliminaryResult = previousResult * ( ( double )1.0 - intermediate );
    }
    else
    {
      preliminaryResult = previousResult * ( ( double )1.0 + intermediate );
    }
  }
  
  // Apply "inter dampening factor".  If necessary, reduce the factor until a positive result is acheived.
  double result;
  do
  {
    result = previousResult + interDampeningFactor * ( preliminaryResult - previousResult );
    interDampeningFactor /= ( double )2.0;
  } while( result <= ( double )0.0 );
  return result;
}

namespace
{
  /// Extracts a single point at the given index from a full meta-log entry
  Point pointFromFullMetaLogEntry( unsigned char index, const MetaLogEntry< std::vector< double > >& fullEntry )
  {
    Point result;
    result.lambdaModifier = fullEntry.lambdaModifiers[ index ];
    result.bitrate = fullEntry.bitrateVector[ index ];
    return result;
  }
  
  /// Calculates the inter dampening factor based
  /// \param parameter The inter dampening parameter which determines how severely the inter dampening factor is affected by Lambda-modifier changes at previous temporal layers
  /// \param cumulativeDelta The sum of the percentage changes of the Lambda-modifiers at the previous temporal layers
  /// \return The calculated inter dampening factor
  /// \pre cumulativeDelta must be non-negative
  /// \pre parameter must be non-negative
  double interDampeningFactor( double parameter, double cumulativeDelta )
  {
    assert( 0.0 <= cumulativeDelta );
    assert( 0.0 <= parameter );
    return ( double )1.0 / ( parameter * cumulativeDelta + ( double )1.0 );
  }
}

std::vector< double > guessLambdaModifiers(
    double initialAdjustmentParameter,
    const std::vector< double > &targetBitrateVector,
    const std::list< MetaLogEntry< std::vector< double > > >& metaLogEntryList )
{
  assert( !targetBitrateVector.empty( ) );
  assert( !metaLogEntryList.empty( ) );
  
  double cumulativeDelta( 0.0 );
  std::vector< double > resultVector;
  for( unsigned char i( 0 ); i < targetBitrateVector.size( ); ++i )
  {
    // Populate pointList with up to two of the previous points
    std::list< Point > pointList;
    std::list< MetaLogEntry< std::vector< double > > >::const_reverse_iterator j( metaLogEntryList.rbegin( ) );
    pointList.push_front( pointFromFullMetaLogEntry( i, *j ) );
    ++j;
    if( j != metaLogEntryList.rend( ) ) pointList.push_front( pointFromFullMetaLogEntry( i, *j ) );
    
    // Calculate the new Lambda-modifier guess and add it to the result vector
    const double newLambdaModifier( guessLambdaModifier(
        initialAdjustmentParameter,
        targetBitrateVector[ i ],  // target bitrate
        pointList,
        interDampeningFactor( 50.0, cumulativeDelta ) ) );
    resultVector.push_back( newLambdaModifier );
    
    // Increment the cumulativeDelta
    const double oldLambdaModifier( pointList.back( ).lambdaModifier );
    cumulativeDelta += std::abs( newLambdaModifier - oldLambdaModifier ) / oldLambdaModifier;
  }
  
  return resultVector;
}

namespace
{
  /// Ignores all of the the characters up to and including a given character
  /// \param i The active input stream
  /// \param character The character to ignore up to
  /// \throw MetaLogParseException if the stream goes bad before character is encountered or just after character is encountered
  void ignoreUpTo( std::istream& i, char character )
  {
    while( i.good( ) && character != i.get( ) )
      ;
    if( !i.good( ) ) throw MetaLogParseException( );
  }
  
  /// Parses a Lambda-modifier map
  /// \param right The map to write the output to
  void parseLambdaModifierMap( std::istream& left, std::map< unsigned char, double >& right )
  {
    for( ; ; )
    {
      assert( left.good( ) );
      
      // Ignore the "-LM"
      if( '-' != left.get( ) ) left.setstate( std::istream::failbit );
      if( !left.good( ) ) break;
      if( 'L' != left.get( ) ) left.setstate( std::istream::failbit );
      if( !left.good( ) ) break;
      if( 'M' != left.get( ) ) left.setstate( std::istream::failbit );
      if( !left.good( ) ) break;
      
      // Parse the index
      long indexLong;
      left >> indexLong;
      if( !left.good( ) ) break;
      if( indexLong < std::numeric_limits< unsigned char >::min( ) ) left.setstate( std::istream::failbit );
      if( std::numeric_limits< unsigned char >::max( ) < indexLong ) left.setstate( std::istream::failbit );
      if( !left.good( ) ) break;
      unsigned char index( ( unsigned char )indexLong );
      
      if( ' ' != left.get( ) ) left.setstate( std::istream::failbit );
      if( !left.good( ) ) break;
      
      // Parse the Lambda-modifier
      double lambdaModifier;
      left >> lambdaModifier;
      if( lambdaModifier <= ( double )0.0 || ( !right.empty( ) && ( right.count( index ) != 0 || index <= right.rbegin( )->first ) ) )
      {
        left.setstate( std::istream::failbit );
      }
      else
      {
        right[ index ] = lambdaModifier;
      }
      if( !left.good( ) ) break;
      
      // If we peek and see a space, then there should be more Lambda-modifiers to parse.  Otherwise, we are finished.
      if( left.peek( ) == ' ' )
      {
        left.ignore( );
      }
      else
      {
        break;
      }
    }
  }
  
  /// Extracts the indexes from the given maps
  /// \return The set of indexes
  std::set< unsigned char > indexSetFromMap( const std::map< unsigned char, double >& in )
  {
    std::set< unsigned char > result;
    for( typename std::map< unsigned char, double >::const_iterator i( in.begin( ) ); i != in.end( ); ++i )
    {
      result.insert( i->first );
    }
    return result;
  }
}

void guessLambdaModifiers(
    std::ostream& o,
    std::istream& initialAdjustmentParameterIstream,
    std::istream& targetsIstream,
    std::istream& metaLogIstream )
{
  // Parse the initialAdjustmentParameter
  double initialAdjustmentParameter;
  initialAdjustmentParameterIstream >> initialAdjustmentParameter;
  if( initialAdjustmentParameterIstream.fail( ) || initialAdjustmentParameterIstream.good( ) )
  {
    throw InitialAdjustmentParameterParseException( );
  }
  
  // Parse the targets
  std::vector< double > targetVector;
  parseBitrateVector( targetsIstream, targetVector );
  if( targetVector.empty( ) || targetsIstream.fail( ) || targetsIstream.good( ) ) throw TargetsParseException( );
  
  // Parse the metalog
  std::list< MetaLogEntry< std::map< unsigned char, double > > > metaLogEntryList;
  do
  {
    // Parse the Lambda-modifiers
    MetaLogEntry< std::map< unsigned char, double > > entry;
    parseLambdaModifierMap( metaLogIstream, entry.lambdaModifiers );
    if( !metaLogIstream.good( ) ) throw MetaLogParseException( );
    
    // Skip the ';'
    if( ';' != metaLogIstream.get( ) ) throw MetaLogParseException( );
    if( !metaLogIstream.good( ) ) throw MetaLogParseException( );
    
    // Parse the bitrates
    parseBitrateVector( metaLogIstream, entry.bitrateVector );
    if( metaLogIstream.fail( ) ) throw MetaLogParseException( );
    metaLogEntryList.push_back( entry );
    
    if( !metaLogIstream.good( ) ) break;
    if( metaLogIstream.get( ) != '\n' ) throw MetaLogParseException( );
    metaLogIstream.peek( );
  } while( metaLogIstream.good( ) );
  if( metaLogEntryList.empty( ) ) throw MetaLogParseException( );  // The meta-log should not be empty
  
  // Initialize firstIndexVector and check that the sizes and indexes match
  std::set< unsigned char > firstIndexSet( indexSetFromMap( metaLogEntryList.front( ).lambdaModifiers ) );
  if( firstIndexSet.size( ) != targetVector.size( ) ) throw MismatchedIndexesException( );
  for( std::list< MetaLogEntry< std::map< unsigned char, double > > >::const_iterator i( metaLogEntryList.begin( ) );
      i != metaLogEntryList.end( );
      ++i )
  {
    if( indexSetFromMap( i->lambdaModifiers ) != firstIndexSet ) throw MismatchedIndexesException( );
    if( i->bitrateVector.size( ) != targetVector.size( ) ) throw MismatchedIndexesException( );
  }
  
  // Initialize simplifiedMetaLogEntryList
  std::list< MetaLogEntry< std::vector< double > > > simplifiedMetaLogEntryList;
  for( std::list< MetaLogEntry< std::map< unsigned char, double > > >::const_iterator i( metaLogEntryList.begin( ) );
      i != metaLogEntryList.end( );
      ++i )
  {
    simplifiedMetaLogEntryList.push_back( MetaLogEntry< std::vector< double > >( ) );
    for( std::map< unsigned char, double >::const_iterator j( i->lambdaModifiers.begin( ) ); j != i->lambdaModifiers.end( ); ++j )
    {
      simplifiedMetaLogEntryList.back( ).lambdaModifiers.push_back( j->second );
    }
    simplifiedMetaLogEntryList.back( ).bitrateVector = i->bitrateVector;
  }
  
  // Run the calculations
  std::vector< double > resultVector( guessLambdaModifiers( initialAdjustmentParameter, targetVector, simplifiedMetaLogEntryList ) );
  
  // Output the results
  std::set< unsigned char >::const_iterator indexIter( firstIndexSet.begin( ) );
  std::vector< double >::const_iterator resultIter( resultVector.begin( ) );
  do
  {
    if( indexIter != firstIndexSet.begin( ) ) o << " ";
    o << "-LM" << ( long )( *indexIter ) << " ";
    o.setf( std::ostream::fixed, std::ostream::floatfield );
    o.precision( 7 );
    o << ( *resultIter );
    
    ++indexIter;
    ++resultIter;
  } while( indexIter != firstIndexSet.end( ) );
  assert( resultIter == resultVector.end( ) );  // The index set and the result vector should be the same size
}

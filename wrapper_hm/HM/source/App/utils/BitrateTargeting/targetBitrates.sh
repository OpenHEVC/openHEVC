#! /bin/sh

# The copyright in this software is being made available under the BSD
# License, included below. This software may be subject to other third party
# and contributor rights, including patent rights, and no such rights are
# granted under this license.  
#
# Copyright (c) 2010-2012, ITU/ISO/IEC
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  * Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
#    be used to endorse or promote products derived from this software without
#    specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.


SUB_TOOLS_DIRECTORY=$(echo "$0" | sed -e 's/[^\/]*$//')
. ${SUB_TOOLS_DIRECTORY}encode.shl

LAMBDA_MODIFIER_PREFIX="LM"

RESUME_MODE_OPTION="-rm"
TARGET_BITRATES_OPTION="-tb"
INITIAL_LAMBDA_MODIFIERS_OPTION="-il"
ENCODE_COMMAND_ARGS_OPTION="-ca"

function outputUsageAndExit {
  local TARGET_BITRATES_USAGE_STRING=targetBitrates
  local INITIAL_LAMBDA_MODIFIERS_USAGE_STRING=initialLambdaModifiers
  local ENCODE_COMMAND_ARGS_USAGE_STRING=encodeCommandArgs
  
  echo "Usage: $0 [$RESUME_MODE_OPTION] $CONFIGURATION_IDENTIFIER_OPTION $CONFIGURATION_IDENTIFIER_USAGE_STRING $Q_OPTION $Q_USAGE_STRING $TARGET_BITRATES_OPTION $TARGET_BITRATES_USAGE_STRING [$INITIAL_LAMBDA_MODIFIERS_OPTION $INITIAL_LAMBDA_MODIFIERS_USAGE_STRING] [$ENCODE_COMMAND_ARGS_OPTION $ENCODE_COMMAND_ARGS_USAGE_STRING] [$EXTRA_ARGUMENTS_OPTION $EXTRA_ARGUMENTS_USAGE_STRING] $OUTPUT_DIRECTORY_OPTION $OUTPUT_DIRECTORY_USAGE_STRING $INPUT_NAME_USAGE_STRING" >&2
  echo "${USAGE_INDENT}$RESUME_MODE_OPTION engages resume mode which allows the user to resume an execution that was interrupted before completion."
  outputConfigurationIdentifierUsage
  outputQUsage
  echo "${USAGE_INDENT}$TARGET_BITRATES_USAGE_STRING is the target bitrates.  For example: \"23:35 24:3473242 etc...\"." >&2
  echo "${USAGE_INDENT}$INITIAL_LAMBDA_MODIFIERS_USAGE_STRING is the Lambda-modifiers to use for the first guess.  For example: \"$-{LAMBDA_MODIFIER_PREFIX}23 1e0 $-{LAMBDA_MODIFIER_PREFIX}24 0.98 etc...\"" >&2
  echo "${USAGE_INDENT}$ENCODE_COMMAND_ARGS_USAGE_STRING is the extra arguments to be passed to encodeCommand.sh.  The common arguments that are available to both $0 and encodeCommand.sh should not be passed though this argument.  For example, don't pass $Q_OPTION here because it is an option of $0.  $EXECUTABLE_OPTION and ($CONFIGURATION_PATH_OPTION or $CONFIGURATION_DIRECTORY_OPTION) must be passed through this argument.  For example, \"$ENCODE_COMMAND_ARGS_OPTION '$EXECUTABLE_OPTION ~/bin/encode.exe $CONFIGURATION_DIRECTORY_OPTION ~/cfg/'\"." >&2
  echo "${USAGE_INDENT}$EXTRA_ARGUMENTS_USAGE_STRING specifies extra arguments to be passed directly to the encoder (not to encodeCommand.sh)." >&2
  outputOutputDirectoryUsage
  outputInputNameUsage
  
  exit 1
}

NORMAL_MODE="NORMAL_MODE"
RESUME_MODE="RESUME_MODE"
mode="$NORMAL_MODE"

# For every argument $1
while [ "" != "$*" ] ; do
  case $1 in
    $RESUME_MODE_OPTION)
      mode="$RESUME_MODE"
    ;;
    -*)
      checkDollarTwo "$1" "$2"
      case $1 in
        $EXTRA_ARGUMENTS_OPTION) extraArguments=$2 ;;
        $Q_OPTION) q=$2 ;;
        $OUTPUT_DIRECTORY_OPTION) outputDirectory=$2 ;;
        $CONFIGURATION_IDENTIFIER_OPTION) configurationIdentifier=$2 ;;
        $TARGET_BITRATES_OPTION) targetBitrates=$2 ;;
        $INITIAL_LAMBDA_MODIFIERS_OPTION) initialLambdaModifiers=$2 ;;
        $ENCODE_COMMAND_ARGS_OPTION) encodeCommandArgs=$2 ;;
        *)
          printf "You entered an invalid option: \"$1\".\n" >&2
          outputUsageAndExit
        ;;
      esac
      shift
    ;;
    *)
      if [[ "" == $inputName ]] ; then
        inputName=$1
      else
        printf "You entered too many arguments.\n" >&2
        outputUsageAndExit
      fi
    ;;
  esac
  
  shift
done

verifyProvided "$Q_STRING" "$q"
verifyQ $q

verifyProvided "$OUTPUT_DIRECTORY_STRING" "$outputDirectory"
verifyDirectory "$OUTPUT_DIRECTORY_STRING" "$outputDirectory"

verifyProvided "$CONFIGURATION_IDENTIFIER_STRING" "$configurationIdentifier"
verifyConfigurationIdentifier "$configurationIdentifier"

verifyProvided "target bitrates ($TARGET_BITRATES_OPTION)" "$targetBitrates"
verifyProvided "$INPUT_NAME_STRING" "$inputName"

outputPathBegin="${outputDirectory}${inputName}_${configurationIdentifier}_q${q}"
logPath="$outputPathBegin.log"
metaLogPath="${outputPathBegin}_meta.log"

TRUE=true

# Outputs "$TRUE" if the given file exist.  Outputs nothing if the given file does not exist.  The first argument is the path to the supposed file.
function doesFileExist {
  ls $1 &> /dev/null
  if [[ 0 == "$?" ]] ; then
    echo "$TRUE"
  fi
}

# Validate the mode (normal or resume) based on whether or not the meta-log file already exists
if [[ "$TRUE" == "$(doesFileExist "$metaLogPath")" ]] ; then
  if [[ "$NORMAL_MODE" == "$mode" ]] ; then
    echo "$metaLogPath already exists.  Consider using resume-mode." >&2
    outputUsageAndExit
  else  # Resume-mode
    cat "$metaLogPath"  # Output the pre-existing meta-log so we can resume where we left off
  fi
else  # Meta-log file does not exist
  if [[ "$RESUME_MODE" == "$mode" ]] ; then
    echo "$metaLogPath does not exist and resume-mode is enabled." >&2
    outputUsageAndExit
  fi
fi

# Outputs the number of elements in the given bitrate vector
function bitrateVectorSize {
  echo "$1" | sed -e 's/[^ ]//g' | wc -c | sed -e 's/^ *//'
}

# Initialize targetBitrateVectorSize
targetBitrateVectorSize="$(bitrateVectorSize "$targetBitrates")"

# Outputs the product of the two input values
function multiply {
  echo | awk "{print $1 * $2}"
}

# Extracts the bitrate at the given index from the given bitrate vector.  The first argument is the given index and the second argument in the given bitrate vector.
function extractBitrateFromVector {
  local localIndex=$(expr "$1" "+" "1")
  echo "$2" | awk "{ print \$$localIndex }"
}

# Outputs a bitrate vector by multiplying the $targetBitrates vector by a given scalar.  The first argument is the given scalar.
function populateBitrates {
  local lI=0
  local lResult=""
  while true ; do
    local lTargetBitrate=$(extractBitrateFromVector "$lI" "$targetBitrates")
    if [[ "" == "$lTargetBitrate" ]] ; then
      break;
    fi
    local lNew=$(multiply "$lTargetBitrate" "$1")
    lResult="$lResult $lNew"
    ((++lI))
  done
  echo "$lResult" | sed -e 's/^ //'
}

# Initialize the ranges
outerRangeMins=$(populateBitrates "0.980")
innerRangeMins=$(populateBitrates "0.985")
innerRangeMaxs=$(populateBitrates "1.015")
outerRangeMaxs=$(populateBitrates "1.020")

# Outputs the given string to the meta-log (both the file and stdout) with no newline character.  The first argument is the string to output.
function outputToMetaLogNoNewline {
  toPrint=`echo $1 | sed -e 's/%/%%/g'`
  printf -- "$toPrint"
  printf -- "$toPrint" >> $metaLogPath
}

# Outputs the given string to the meta-log (both the file and stdout) with a newline character.  The first argument is the string to output.
function outputToMetaLogWithNewline {
  echo "$1"
  echo "$1" >> $metaLogPath
}

# Extracts the Lambda-modifier at the given index from $lambdaModifiers.  The first argument is the given index.
function extractLambdaModifier {
  printf -- "$lambdaModifiers" | sed -e 's/^-//' | sed -e 's/ -/\
/g' | grep "${LAMBDA_MODIFIER_PREFIX}$1 " | sed -e 's/^[^ ]* //'
}

# Outputs the given Lambda-modifier with a fixed number of decimal points.  The first argument is the given Lambda-modifier.
function formatLambdaModifier {
  printf "%.7f" "$1"
}

# Outputs $lambdaModifiers to the meta-log with proper formatting
function outputLambdaModifiersToMetaLog {
  local lI=0
  local lLambdaModifier=$(extractLambdaModifier "$lI")
  local lLambdaModifier=$(formatLambdaModifier "$lLambdaModifier")
  local lOutput="-${LAMBDA_MODIFIER_PREFIX}$lI $lLambdaModifier"
  while true ; do
    ((++lI))
    local lLambdaModifier=$(extractLambdaModifier "$lI")
    if [[ "" == "$lLambdaModifier" ]] ; then
      break
    fi
    local lLambdaModifier=$(formatLambdaModifier "$lLambdaModifier")
    local lOutput="$lOutput -${LAMBDA_MODIFIER_PREFIX}$lI $lLambdaModifier"
  done
  outputToMetaLogNoNewline "$lOutput;"
}

# Initialize lambdaModifiers and output it to the meta-log
if [[ "$RESUME_MODE" == "$mode" ]] ; then
  if [[ "" == "$initialLambdaModifiers" ]] ; then  # If no initial lambda-modifiers provided, use default value
    lambdaModifiers=$(tail -n 1 < "$metaLogPath" | sed -e 's/;$//')
  else  # Initial lambda-modifiers provided
    echo "You cannot use $RESUME_MODE and specify the initial lambda-modifiers.  In resume-mode, the lambda-modifiers will be retreived from the last line of the meta-log." >&2
    outputUsageAndExit
  fi
else
  if [[ "" == "$initialLambdaModifiers" ]] ; then  # If no initial lambda-modifiers provided, use default value
    lambdaModifiers="-${LAMBDA_MODIFIER_PREFIX}0 1"
    for (( i=1; i<"$targetBitrateVectorSize"; ++i )); do
      lambdaModifiers="$lambdaModifiers -${LAMBDA_MODIFIER_PREFIX}${i} 1"
    done
  else  # Initial lambda-modifiers provided
    lambdaModifiers="$initialLambdaModifiers"
  fi
  outputLambdaModifiersToMetaLog
fi

# Calculates the difference percentage between the given target bitrate and the given bitrate, appropriately formats this difference percentage, and then outputs it.  The first argument is the given target bitrate and the second argument is the given bitrate.
function calculateAndFormatDifferencePercentage {
  # Calculate the result and format it with the right number of decimal places
  local result=$(echo | awk "{print 100*($2-$1)/$1}")
  local result=$(printf "%.3f" "$result")
  
  # Separate the sign from the result
  local sign=$(echo "$result" | sed -e 's/[^-]*$//')
  if [[ "$sign" != "-" ]] ; then
    local sign="+"
  fi
  local result=$(echo "$result" | sed -e 's/^-//')
  
  # Pad leading zereos to make two digits before the decimal point
  if [[ 2 == $(echo "$result" | sed -e 's/\..*$//' | wc -c | sed -e 's/^ *//') ]] ; then
    local result="0$result"
  fi
  
  # Output the result including the sign and the percent sign
  echo "${sign}${result}%"
}

# Outputs $TRUE i.f.f. the first argument is less than the second argument
function lessOrEqual {
  echo | awk "{ if($1 < $2) print \"$TRUE\" }"
}

# Outputs $TRUE i.f.f. the second argument is greater than the first argument and less than the third argument ($1 < $2 < $3)
function isInRange {
  if [[ "$TRUE" == $(lessOrEqual "$1" "$2") ]] ; then
    if [[ "$TRUE" == $(lessOrEqual "$2" "$3") ]] ; then
      echo "$TRUE"
    fi
  fi
}

# From the given bitrate vector, outputs the "bad" bitrates by filtering out the "good" bitrates.  The first argument is the index of the last good bitrate and the second argument is the given bitrate vector.  If the first argument is -1, then the given bitrate vector is outputted in its entirety.
function filterOutGoodBitrates {
  local result="$2"
  for (( i=0; i<="$1"; ++i )); do
    result=$(echo "$result" | sed -e 's/^[^ ]* //')
  done
  echo "$result"
}

# Outputs a given line from the given variable.  The first argument is the line number to output and the second argument is the given variable to extract the line from.
function outputLine {
  echo "$2" | head -n "$(expr "$1" + 1)" | tail -n 1
}

# Initialize iterationCount
if [[ "$RESUME_MODE" == "$mode" ]] ; then
  iterationCount=$(wc -l < "$metaLogPath" | sed -e 's/^ *//')
else
  iterationCount=0
fi

ITERATION_COUNT_LIMIT=50  # The number of attempts to make before giving up

while true ; do  # The main loop
  
  # Run the encoder
  sh ${SUB_TOOLS_DIRECTORY}encodeCommand.sh $inputName $encodeCommandArgs $CONFIGURATION_IDENTIFIER_OPTION $configurationIdentifier $Q_OPTION $q $OUTPUT_DIRECTORY_OPTION $outputDirectory -ea "$extraArguments $lambdaModifiers" | sh > $logPath
  if [[ $? != 0 ]] ; then
    printf "Unexpected exit status from encodeCommand.sh\n" >&2
    exit 1
  fi
  
  # Extract and output the bitrates
  bitrates=`${SUB_TOOLS_DIRECTORY}extractBitrates.exe < $logPath`
  outputToMetaLogNoNewline "$bitrates;"
  
  # Make sure that the index set of the extracted bitrates matches the index set of the target bitrates
  if [[ "$targetBitrateVectorSize" != "$(bitrateVectorSize "$bitrates")" ]] ; then
    echo "Index set from the extracted bitrates does not match the index set from the target bitrates" >&2
    exit 1
  fi
  
  # Calculate the bitrate difference percentages and output them to the meta-log
  percentages="$(calculateAndFormatDifferencePercentage "$(extractBitrateFromVector "0" "$targetBitrates")" "$(extractBitrateFromVector "0" "$bitrates")")"
  for (( i=1; i<"$targetBitrateVectorSize"; ++i )); do
    percentages="$percentages $(calculateAndFormatDifferencePercentage "$(extractBitrateFromVector "$i" "$targetBitrates")" "$(extractBitrateFromVector "$i" "$bitrates")")"
  done
  outputToMetaLogNoNewline "$percentages;"
  
  # Initialize and output areBitratesSatismodifiery
  areBitratesSatisfactory=yes
  for (( i=0; ; ++i )) ; do
    outerRangeMin=$(extractBitrateFromVector "$i" "$outerRangeMins")
    bitrate=$(extractBitrateFromVector "$i" "$bitrates")
    outerRangeMax=$(extractBitrateFromVector "$i" "$outerRangeMaxs")
    if [[ "" == "$bitrate" ]] ; then
      break
    fi
    if [[ $(isInRange "$outerRangeMin" "$bitrate" "$outerRangeMax") != "$TRUE" ]] ; then
      areBitratesSatisfactory=no
      break
    fi
  done
  outputToMetaLogWithNewline "$areBitratesSatisfactory"
  
  # Exit if we are finished or if we have iterated too many times
  if [[ yes == $areBitratesSatisfactory ]] ; then
    mv "$logPath" "${outputPathBegin}_final.log"
    exit 0
  else
    # Rename the deprecated log
    countString="$iterationCount"
    if [[ 1 == `printf -- "$countString" | wc -c | sed -e 's/^ *//'` ]] ; then
      countString="0$countString"
    fi
    mv "$logPath" "${outputPathBegin}_dep${countString}.log"
    
    ((++iterationCount))
    if [[ "$ITERATION_COUNT_LIMIT" == "$iterationCount" ]] ; then
      outputToMetaLogWithNewline "Could not reach target bitrates"
      exit 1
    fi
  fi
  
  filteredMetaLog=$(sed -e 's/;[^;]*$//' < $metaLogPath | sed -e 's/;[^;]*$//')
  bitratesFromMetaLog=$(printf -- "$filteredMetaLog" | sed -e 's/^[^;]*;//')
  
  # Initialize goodIndex
  goodIndex=-1
  for (( i=0; i<"$targetBitrateVectorSize"; ++i )); do
    innerRangeMin=$(extractBitrateFromVector "$i" "$innerRangeMins")
    bitrate=$(extractBitrateFromVector "$i" "$bitrates")
    innerRangeMax=$(extractBitrateFromVector "$i" "$innerRangeMaxs")
    if [[ "$TRUE" == $(isInRange "$innerRangeMin" "$bitrate" "$innerRangeMax") ]] ; then
      goodIndex="$i"
    else
      break
    fi
  done
  
  badBitrates=$(filterOutGoodBitrates "$goodIndex" "$bitratesFromMetaLog")
  lambdaModifiersFromMetaLog=`printf -- "$filteredMetaLog" | sed -e 's/;[^;]*$//'`
  badLambdaModifiers=`printf -- "$lambdaModifiersFromMetaLog" | sed -e "s/^.*-${LAMBDA_MODIFIER_PREFIX}$goodIndex [^ ]* //"`
  lineCount=`printf -- "$badBitrates\n" | wc -l | sed -e 's/^ *//'`
  
  # Initialize guessLambdaModifiersIn
  guessLambdaModifiersIn="$(outputLine 0 "$badLambdaModifiers");$(outputLine 0 "$badBitrates")"
  for (( i=1; i<"$lineCount"; ++i )); do
    guessLambdaModifiersIn="$(printf -- "$guessLambdaModifiersIn\n$(outputLine "$i" "$badLambdaModifiers");$(outputLine "$i" "$badBitrates")")"
  done
  
  # Run guessLambdaModifiers
  guessedLambdaModifiers=$(printf -- "$guessLambdaModifiersIn" | ${SUB_TOOLS_DIRECTORY}guessLambdaModifiers.exe "-.5" "$(filterOutGoodBitrates "$goodIndex" "$targetBitrates")")
  if [[ $? != 0 ]] ; then
    printf "Unexpected exit status from guessLambdaModifiers.exe\n" >&2
    exit 1
  fi
  
  # Initialize lambdaModifiers and output them to the meta-log
  lastLambdaModifiersFromMetaLog=`printf -- "$filteredMetaLog" | tail -n 1 | sed -e "s/;[^;]*$//"`
  goodLambdaModifiersFromMetaLog=`printf -- "$lastLambdaModifiersFromMetaLog" | tail -n 1 | sed -e "s/-${LAMBDA_MODIFIER_PREFIX}$(expr "$goodIndex" + 1).*$//"`
  lambdaModifiers="${goodLambdaModifiersFromMetaLog}${guessedLambdaModifiers}"
  outputLambdaModifiersToMetaLog
  
done

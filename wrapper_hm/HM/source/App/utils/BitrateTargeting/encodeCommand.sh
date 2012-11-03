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

# Outputs a command to run the encoder for a given set of arguments.  The encoder typically requires a lot of arguments to run, so this script can be more convenient than running the encoder direcly because it automatically generates many of the arguments.

# "Include" encode.shl which contains common shell script code related to encoding
SUB_TOOLS_DIRECTORY=$(echo "$0" | sed -e 's/[^\/]*$//')
. ${SUB_TOOLS_DIRECTORY}encode.shl

USE_LOG_FILE_OPTION="-l"
INPUT_DIRECTORY_OPTION="-id"
NUM_FRAMES_OPTION="-f"

function outputUsageAndExit {
  local EXECUTABLE_USAGE_STRING=executable
  local INPUT_DIRECTORY_USAGE_STRING=inputDirectory
  local CONFIGURATION_PATH_USAGE_STRING=configurationPath
  local CONFIGURATION_DIRECTORY_USAGE_STRING=configurationDirectory
  local NUM_FRAMES_USAGE_STRING=numFrames
  
  echo "Usage: $0 $CONFIGURATION_IDENTIFIER_OPTION $CONFIGURATION_IDENTIFIER_USAGE_STRING ($CONFIGURATION_PATH_OPTION $CONFIGURATION_PATH_USAGE_STRING -or- $CONFIGURATION_DIRECTORY_OPTION $CONFIGURATION_DIRECTORY_USAGE_STRING) $Q_OPTION $Q_USAGE_STRING $EXECUTABLE_OPTION $EXECUTABLE_USAGE_STRING [$NUM_FRAMES_OPTION $NUM_FRAMES_USAGE_STRING] [$EXTRA_ARGUMENTS_OPTION $EXTRA_ARGUMENTS_USAGE_STRING] $OUTPUT_DIRECTORY_OPTION $OUTPUT_DIRECTORY_USAGE_STRING [$USE_LOG_FILE_OPTION] [$INPUT_DIRECTORY_OPTION $INPUT_DIRECTORY_USAGE_STRING] $INPUT_NAME_USAGE_STRING" >&2
  outputConfigurationIdentifierUsage
  echo "${USAGE_INDENT}$CONFIGURATION_PATH_USAGE_STRING is the path of the configuration file to use.  Either this or $CONFIGURATION_DIRECTORY_USAGE_STRING must be specified (but not both)." >&2
  echo "${USAGE_INDENT}$CONFIGURATION_DIRECTORY_USAGE_STRING is the path of the directory that contains the configuration files.  The particular file will be chosen based on $CONFIGURATION_IDENTIFIER_USAGE_STRING.  Either this or $CONFIGURATION_PATH_USAGE_STRING must be specified (but not both)." >&2
  outputQUsage
  echo "${USAGE_INDENT}$EXECUTABLE_USAGE_STRING is the path of the encoder executable." >&2
  echo "${USAGE_INDENT}$NUM_FRAMES_USAGE_STRING is the number of frames to encode.  If omitted, the entire sequence will be encoded." >&2
  echo "${USAGE_INDENT}$EXTRA_ARGUMENTS_USAGE_STRING is any extra arguments that should be passed on to the encoder." >&2
  outputOutputDirectoryUsage
  echo "${USAGE_INDENT}If $USE_LOG_FILE_OPTION is specified, the encoder will output to a log file.  Otherwise it will output to the standard output." >&2
  echo "${USAGE_INDENT}$INPUT_DIRECTORY_USAGE_STRING is the directory that contains the sequences.  The default value is the SEQUENCE_DIR environment variable." >&2
  outputInputNameUsage
  
  exit 1
}

# Used to lookup the width, height, number of frames, and frame rate for a given sequence
table=`printf "${table}\nNebutaFestival_2560x1600_60_10bit_crop          2560   1600   300        60"`
table=`printf "${table}\nSteamLocomotiveTrain_2560x1600_60_10bit_crop    2560   1600   300        60"`
table=`printf "${table}\nTraffic_2560x1600_30_crop                       2560   1600   150        30"`
table=`printf "${table}\nPeopleOnStreet_2560x1600_30_crop                2560   1600   150        30"`
table=`printf "${table}\nBQTerrace_1920x1080_60                          1920   1080   600        60"`
table=`printf "${table}\nBasketballDrive_1920x1080_50                    1920   1080   500        50"`
table=`printf "${table}\nCactus_1920x1080_50                             1920   1080   500        50"`
table=`printf "${table}\nKimono1_1920x1080_24                            1920   1080   240        24"`
table=`printf "${table}\nParkScene_1920x1080_24                          1920   1080   240        24"`
table=`printf "${table}\nvidyo1_720p_60                                  1280    720   600        60"`
table=`printf "${table}\nvidyo3_720p_60                                  1280    720   600        60"`
table=`printf "${table}\nvidyo4_720p_60                                  1280    720   600        60"`
table=`printf "${table}\nRaceHorses_832x480_30                            832    480   300        30"`
table=`printf "${table}\nBQMall_832x480_60                                832    480   600        60"`
table=`printf "${table}\nPartyScene_832x480_50                            832    480   500        50"`
table=`printf "${table}\nBasketballDrill_832x480_50                       832    480   500        50"`
table=`printf "${table}\nRaceHorses_416x240_30                            416    240   300        30"`
table=`printf "${table}\nBQSquare_416x240_60                              416    240   600        60"`
table=`printf "${table}\nBlowingBubbles_416x240_50                        416    240   500        50"`
table=`printf "${table}\nBasketballPass_416x240_50                        416    240   500        50"`
table=`printf "${table}\nBasketballDrillText_832x480_50                   832    480   500        50"`
table=`printf "${table}\nChinaspeed_1024x768_30                          1024    768   500        30"`
table=`printf "${table}\nSlideEditing_1280x720_30                        1280    720   300        30"`
table=`printf "${table}\nSlideShow_1280x720_20                           1280    720   500        20"`

EXECUTABLE_STRING="executable ($EXECUTABLE_OPTION)"
INPUT_DIRECTORY_STRING="input directory ($INPUT_DIRECTORY_OPTION)"
CONFIGURATION_PATH_STRING="configuration path ($CONFIGURATION_PATH_OPTION)"
CONFIGURATION_DIRECTORY_STRING="configuration directory ($CONFIGURATION_DIRECTORY_OPTION)"

inputDirectory="$SEQUENCE_DIR"  # The default input directory is taken from this environment variable

# For every argument $1
while [ "" != "$*" ] ; do
  case $1 in
    $USE_LOG_FILE_OPTION)
      useLogFile=$1
    ;;
    -*)
      checkDollarTwo "$1" "$2"
      case $1 in
        $EXECUTABLE_OPTION) executable=$2 ;;
        $INPUT_DIRECTORY_OPTION) inputDirectory=$2 ;;
        $EXTRA_ARGUMENTS_OPTION) extraArguments=$2 ;;
        $Q_OPTION) q=$2 ;;
        $OUTPUT_DIRECTORY_OPTION) outputDirectory=$2 ;;
        $CONFIGURATION_IDENTIFIER_OPTION) configurationIdentifier=$2 ;;
        $CONFIGURATION_PATH_OPTION) configurationPath=$2 ;;
        $CONFIGURATION_DIRECTORY_OPTION) configurationDirectory=$2 ;;
        $NUM_FRAMES_OPTION) numFrames=$2 ;;
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

verifyProvided "$EXECUTABLE_STRING" "$executable"
verifyNotDirectory "$EXECUTABLE_STRING" "$executable"

verifyDirectory "$INPUT_DIRECTORY_STRING" "$inputDirectory"

verifyProvided "$Q_STRING" "$q"
verifyQ $q

verifyProvided "$OUTPUT_DIRECTORY_STRING" "$outputDirectory"
verifyDirectory "$OUTPUT_DIRECTORY_STRING" "$outputDirectory"

verifyProvided "$CONFIGURATION_IDENTIFIER_STRING" "$configurationIdentifier"
verifyConfigurationIdentifier "$configurationIdentifier"

# Validate $configurationPath or $configurationDirectory
if [[ $configurationPath != "" ]] ; then
  verifyNotDirectory "$CONFIGURATION_PATH_STRING" "$configurationPath"
else
  if [[ "" == $configurationDirectory ]] ; then
    printf "You must enter a $CONFIGURATION_PATH_STRING or $CONFIGURATION_DIRECTORY_STRING.\n" >&2
    outputUsageAndExit
  else
    verifyDirectory "$CONFIGURATION_DIRECTORY_STRING" "$configurationDirectory"
  fi
fi

verifyProvided "$INPUT_NAME_STRING" "$inputName"
verifyNotDirectory "$INPUT_NAME_STRING" "$inputName"

# If configurationPath is not already populated, populate it based on the configuration directory and the configuration identifier
if [[ "" == $configurationPath ]] ; then
  configurationPath="${configurationDirectory}encoder_"
  case $configurationIdentifier in
    ldLC) configurationPath="${configurationPath}lowdelay_loco" ;;
    raLC) configurationPath="${configurationPath}randomaccess_loco" ;;
    inLC) configurationPath="${configurationPath}intra_loco" ;;
    ldHE) configurationPath="${configurationPath}lowdelay" ;;
    raHE) configurationPath="${configurationPath}randomaccess" ;;
    *)    configurationPath="${configurationPath}intra" ;;  # inHE
  esac
  configurationPath="${configurationPath}.cfg"
fi

# Validate the input name and populate masterLine which contains the line from the table for the given sequence
masterLine=`printf "$table" | grep -i "^$inputName "`
if [[ "" == $masterLine ]] ; then
  printf "Invalid input name.\n" >&2
  outputUsageAndExit
fi

# If numFrames is not yet initialized, initialize it by looking up the values in the table
if [[ "" == $numFrames ]] ; then
  numFrames=`printf -- "$masterLine" | awk '{ print $4 }'`
fi

# Initialize these variables by looking up the values in the table
frameRate=`printf -- "$masterLine" | awk '{ print $5 }'`
width=`printf -- "$masterLine" | awk '{ print $2 }'`
height=`printf -- "$masterLine" | awk '{ print $3 }'`

# Initialize intraPeriod
case $configurationIdentifier in
  ld*)
    intraPeriod="-1"
  ;;
  ra*)
    if [[ 20 == "$frameRate" ]] ; then
      intraPeriod="16"
    else
      intraPeriod=$(expr "$frameRate" + 4)
      intraPeriod=$(expr "$intraPeriod" / 8)
      intraPeriod=$(expr "$intraPeriod" \* 8)
    fi
  ;;
  in*)
    intraPeriod="1"
  ;;
  *)
    outputConfigurationIdentifierErrorAndExit
  ;;
esac

# Initialize tenBit if the given sequence is 10-bit
printf -- "$inputName" | grep -i '10bit' > /dev/null
case $? in
  0)  tenBit="--InputBitDepth=10 "
    ;;
  1)  ;;
  *)  exit $?
    ;;
esac

outputPathBegin="${outputDirectory}${inputName}_${configurationIdentifier}_q${q}"

# Output the command
printf -- "$executable "
printf -- "-c $configurationPath "
printf -- "-i $inputDirectory$inputName.yuv "
printf -- "-f $numFrames "
printf -- "-fr $frameRate "
printf -- "-wdt $width "
printf -- "-hgt $height "
printf -- "-ip $intraPeriod "
printf -- "$tenBit"
if [[ $tenBit != "" ]] ; then
  printf " "
fi
printf -- "$extraArguments"
if [[ $extraArguments != "" ]] ; then
  printf -- " "
fi
printf -- "-q $q "
printf -- "-b $outputPathBegin.bin "
printf -- "-o $outputPathBegin.yuv "
if [[ "" != $useLogFile ]] ; then
  printf -- "&> $outputPathBegin.log"
fi
printf "\n"

exit 0

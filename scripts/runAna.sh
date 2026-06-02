#!/bin/bash

export SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source $SCRIPTS_DIR/init.sh

# Set the default input and output directories, to be overwritten by the settings
inputDirectory="" 
outputDirectory=$HOME_DIR"/output/"

# Initialize variables
settingsFile=""
fileName=""
noCompile=false
cleanCompile=false
nsigma=5 # Default value for nSigma
calRun="" # Added for explicit cal run

clean_output_files() {
  local baseName="$1"
  local outDir="$2"

  if [ -z "$baseName" ] || [ -z "$outDir" ]; then
    return 0
  fi

  echo "Cleaning output files in $outDir for base name $baseName"
  rm -f "${outDir}/${baseName}.cal"
  rm -f "${outDir}/${baseName}.root"
  rm -f "${outDir}/${baseName}_converted.root"
  rm -f "${outDir}/${baseName}_formatted.root"
  rm -f "${outDir}/${baseName}_clusters.root"
  rm -f "${outDir}/${baseName}"*_report.pdf
}

ensure_calibration_file() {
  local calRunPath="$1"
  local outDir="$2"

  if [ -z "$calRunPath" ] || [ -z "$outDir" ]; then
    return 1
  fi

  local calFileName
  calFileName=$(basename "$calRunPath")
  calFileName=${calFileName%.*}
  local calFile="${outDir}/${calFileName}.cal"
  local calRootFile="${outDir}/${calFileName}.root"

  if [ -f "$calFile" ]; then
    return 0
  fi

  echo "Calibration file $calFile is missing. Generating it from $calRunPath."

  if [ ! -f "$calRootFile" ]; then
    local convert_cal="./PAPERO_convert ${calRunPath} ${calRootFile} --dune"
    echo "Executing command: $convert_cal"
    $convert_cal
    if [ $? -ne 0 ]; then
      echo "Error: Data conversion failed for calibration run $calRunPath."
      return 1
    fi
  fi

  local extract_calibration="./calibration ${calRootFile} --output ${outDir}/${calFileName} --dune --fast"
  echo "Executing command: $extract_calibration"
  $extract_calibration
  if [ $? -ne 0 ]; then
    echo "Error: calibration failed for calibration run $calRunPath."
    return 1
  fi

  return 0
}

# Function to print help message
print_help() {
    echo "*****************************************************************************"
    echo "Usage: $0 -f <first_run> [-l <last_run>] -j <settings_file> [-h]"
    echo "  -p | --home-path      path to the code home directory (/your/path/tof-reco, no / at the end). There is no default."
    echo "  -r | --run-name       full run name, with ot without .dat"
    echo "  -s | --n-sigma       number of sigma to use for the analysis. Will overwrite whatever is in json settings."
    echo "  -f | --first-run      first run number"
    echo "  -l | --last-run       last run number" 
    echo "  -j | --json-settings  json settings file to run this app"
    echo "  --cal-run             explicitly provide the calibration run number"
    echo "  --no-compile          do not recompile the code. Default is to recompile the code"
    echo "  --clean-compile       clean and recompile the code. Default is to recompile the code without cleaning"
    echo "  --clean-files       clean the output files before running the analysis. Default is to not clean the output files"
    echo "  --source-lxplus       source the lxplus environment. Default is to not source the lxplus environment"
    echo "  -h | --help           print this help message"
    echo "*****************************************************************************"
    exit 0
}

# Parse command-line arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
      -p|--home-path)      HOME_DIR="$2"; shift 2 ;;
      -s|--n-sigma)        nsigma="$2"; shift 2 ;;
      -r|--run-name)       fileName="$2"; shift 2 ;;
      -f|--first-run)      firstRun="$2"; shift 2 ;;
      -l|--last-run)       lastRun="$2"; shift 2 ;;
      -j|--json-settings)  settingsFile="$2"; shift 2 ;;
      --cal-run)           calRun="$2"; shift 2 ;;
      --no-compile)        noCompile=true; shift ;;
      --clean-compile)     cleanCompile=true; shift ;;
      --clean-files)      cleanFiles=true; shift ;;
      --source-lxplus)     sourceLxplus=true; shift ;;
      -h|--help)           print_help ;;
      *)                   shift ;;
    esac
done

if [ -z "$settingsFile" ]
then
  echo "Please specify the settings file, using flag -j <settings_file>. Stopping execution."
  exit 0
fi

####################
# Find the settings file
echo "Looking for settings file $settingsFile. If execution stops, it means that the file was not found."
findSettings_command="$SCRIPTS_DIR/findSettings.sh -j $settingsFile"
# last line of the output of findSettings.sh is the full path of the settings file
settingsFile=$(. $findSettings_command | tail -n 1)
echo -e "Settings file found, full path is: $settingsFile \n"

echo "Home is ${HOME_DIR}."
echo "Currently in $(pwd), moving to ${HOME_DIR} to run the script." # might not be necessary if using always absolute paths
cd $HOME_DIR

# read from settings file the input and output paths
inputDirectory=$(awk -F'"' '/inputDirectory/{print $4}' "$settingsFile")
outputDirectory=$(awk -F'"' '/outputDirectory/{print $4}' "$settingsFile")
verbose=$(awk -F'"' '/verboseMode/{print $4}' "$settingsFile")
debug=$(awk -F'"' '/debugMode/{print $4}' "$settingsFile")

# Only read nsigma from settings if not provided via command line
if [ -z "$nsigma" ]; then
    nsigma=$(awk -F'"' '/nSigma/{print $4}' "$settingsFile") # TODO add fallback if not defined
fi

# Check if the user has selected a run name or number(s)
if [ -z "$fileName" ] 
then
    if [ -z "$firstRun" ]
    then
    echo "Please select a run number (or an interval) from command line to convert the data into ROOT format. 
    You can select only the first run, or both the first and the last run.
    Usage: 
     $0 -f <first_run> [-l <last_run>] -j <json_settings_file> [-h]"
    exit 0
    fi
    if [ -z "$lastRun" ]
    then
    echo "I have received only the number of the first run. Reading only run $firstRun"
    lastRun=$firstRun
    fi
else
    echo "I have received the run name: $fileName"
    # if it has the extension, remove it
    fileName=$(echo $fileName | awk -F'.' '{print $1}')
    runit_from_name=$(echo $fileName | sed -n 's/.*RUN\([0-9]\{5,\}\).*/\1/p' | sed 's/^0*//')
    if [ -n "$runit_from_name" ]; then
        firstRun=$runit_from_name
        lastRun=$runit_from_name
        fileName="" # Unset fileName to use the loop logic
    fi
fi

################################################
# Compile the code if requested
compile_command="$SCRIPTS_DIR/compile.sh -p $HOME_DIR --no-compile $noCompile --clean-compile $cleanCompile"
echo "Compiling the code with the following command:"
echo $compile_command
. $compile_command
if [ $? -ne 0 ]; then
    echo "Error: Compilation failed. Stopping execution."
    exit 1
fi

################################################
# Execute the code
cd $BUILD_DIR # executable is here, for now

if [ -n "$fileName" ]
then
  # just to get in the loop below
  firstRun=0
  lastRun=0
fi

# Iterate over all the selected runs
availableRuns=()
if [ -z "$fileName" ]; then
  while IFS= read -r runValue; do
    if [ -n "$runValue" ]; then
      availableRuns+=("$runValue")
    fi
  done < <(find "$inputDirectory" -type f -name "SCD_RUN*.dat" 2>/dev/null \
      | sed -n 's|.*/SCD_RUN\([0-9]\{5\}\)_.*|\1|p' \
      | sed 's/^0*//' \
      | awk -v min="$firstRun" -v max="$lastRun" '($1>=min && $1<=max){print $1}' \
      | sort -n -u)
else
  availableRuns+=("$firstRun")
fi

if [ ${#availableRuns[@]} -eq 0 ]; then
  echo "No runs found in $inputDirectory for range $firstRun-$lastRun. Stopping execution."
  exit 0
fi

for runit in "${availableRuns[@]}"
do
  if [ -z $fileName ]
  then

    echo "Looking for files by run number"
    echo "runit: $runit"
    runit_5d=$(printf "%05d" "$runit")
    # Prefer shared finder utility
    filePath=$($SCRIPTS_DIR/findRun.sh -r "$runit" -i "$inputDirectory" | tail -n 1)
    
    # Stop execution if the selected run is not present in the input directory
    if [ -z "$filePath" ]
    then
        echo "Run "$runit " not found. Stopping execution."
        continue
    fi

    echo "For this run number, found the following file(s):"
    echo "$filePath"

    echo "Dealing with file: $filePath"
    echo "outputDirectory: $outputDirectory"

    # in case it doesn't exist, creating output directory
    mkdir -p "$outputDirectory"

    echo ""
    echo "Currently we are in"
    pwd
    echo ""

    fileName=$(basename "$filePath")
    fileName=${fileName%.*}
    echo "File name is: $fileName"
  else
    echo "Received a single file path ${fileName}"
    filePath="${inputDirectory}/${fileName}.dat"
    runit=$(echo $fileName | sed -n 's/.*RUN\([0-9]*\).*/\1/p' | sed 's/^0*//')
    if [ -z "$runit" ]; then
        runit=$fileName
    fi
  fi

  # clean output files if requested
  if [ "$cleanFiles" = true ]
  then
      clean_output_files "$fileName" "$outputDirectory"
  fi

  if echo "$fileName" | grep -qi "CAL"; then
    # If CAL, just run calibration
    # Only run calibration if the .cal file does not exist
    calFile="${outputDirectory}/${fileName}.cal"
    if [ -f "$calFile" ]; then
      echo "Calibration file $calFile already exists. Skipping calibration for this run."
      fileName="" # reset fileName for the next iteration
      continue
    else
      echo "CAL run detected. Running calibration only."
      convert_data="./PAPERO_convert ${filePath} ${outputDirectory}/${fileName}.root --dune"
      echo "Executing command: "$convert_data
      $convert_data
      if [ $? -ne 0 ]; then
        echo "Error: Data conversion failed for run ${runit}. Skipping calibration for this run."
        fileName="" # reset fileName for the next iteration
        continue
      fi


      extract_calibration="./calibration ${outputDirectory}/${fileName}.root --output ${outputDirectory}/${fileName} --dune --fast"
      echo "Executing command: "$extract_calibration
      $extract_calibration
      if [ $? -ne 0 ]; then
        echo "Error: calibration failed for run ${runit}."
        fileName="" # reset fileName for the next iteration
        continue
      fi
      fileName="" # reset fileName for the next iteration
    fi
  elif echo "$fileName" | grep -qi "BEAM"; then
    # BEAM
    echo "BEAM run detected. Running full analysis chain."
    
    # Check if formatted and clustered files already exist (skip entire chain if so, unless cleanFiles is requested)
    formattedFile="${outputDirectory}/${fileName}_formatted.root"
    clusteredFile="${outputDirectory}/${fileName}_clusters.root"
    
    skipEntireChain=false
    if [ "$cleanFiles" != true ]; then
        if [ -f "${formattedFile}" ] && [ -f "${clusteredFile}" ]; then
            # Both files exist, validate they have required trees
            if root -l -q "${formattedFile}" -e "TFile f(\"${formattedFile}\"); bool hasTree = (f.Get(\"detector_0\") != nullptr || f.Get(\"detector0\") != nullptr); if (hasTree) { cout << \"TREE_EXISTS\" << endl; } else { cout << \"TREE_MISSING\" << endl; }" 2>/dev/null | grep -q "TREE_EXISTS"; then
                if root -l -q "${clusteredFile}" -e "TFile f(\"${clusteredFile}\"); if (f.Get(\"clusters\")) { cout << \"TREE_EXISTS\" << endl; } else { cout << \"TREE_MISSING\" << endl; }" 2>/dev/null | grep -q "TREE_EXISTS"; then
                    echo "Formatted and clustered files already exist with valid trees. Skipping conversion/formatting/clustering chain."
                    skipEntireChain=true
                fi
            fi
        fi
    fi
    
    needsConversion=false
    if [ "$skipEntireChain" = false ]; then
        # Always validate that converted.root has required detector trees
        if [ -f "${outputDirectory}/${fileName}_converted.root" ]; then
            # Check if the converted file has the required detector trees
            if root -l -q "${outputDirectory}/${fileName}_converted.root" -e "TFile f(\"${outputDirectory}/${fileName}_converted.root\"); if (f.Get(\"raw_detector0\")) { cout << \"TREE_EXISTS\" << endl; } else { cout << \"TREE_MISSING\" << endl; }" 2>/dev/null | grep -q "TREE_EXISTS"; then
                if [ "$cleanFiles" != true ]; then
                    echo "File ${outputDirectory}/${fileName}_converted.root already exists with valid trees. Skipping conversion."
                else
                    echo "File ${outputDirectory}/${fileName}_converted.root exists but cleanFiles requested. Will re-convert."
                    rm -f "${outputDirectory}/${fileName}_converted.root"
                    needsConversion=true
                fi
            else
                echo "Converted file exists but is missing required detector trees. Will re-convert."
                rm -f "${outputDirectory}/${fileName}_converted.root"
                needsConversion=true
            fi
        else
            needsConversion=true
        fi
    fi
    
    if [ "$skipEntireChain" = false ] && [ "$needsConversion" = true ]; then
      # Determine calibration strategy: use previous CAL run's .cal (fallback: next CAL; if current is CAL, use its own)
      currentBaseName=$(basename "$filePath")
      currentIsCAL=false
      if echo "$currentBaseName" | grep -qi "CAL"; then
        currentIsCAL=true
      fi

      calRunPath=""
      if [ -n "$calRun" ]; then
          calRunPath=$($SCRIPTS_DIR/findRun.sh -r "$calRun" -i "$inputDirectory" 2>/dev/null | tail -n 1)
      else
          # Search backwards for the most recent previous run whose filename contains "CAL"
          prevRun=$((runit-1))
          while [ $prevRun -ge 0 ]; do
            prevPaths=$($SCRIPTS_DIR/findRun.sh -r "$prevRun" -i "$inputDirectory" 2>/dev/null | tail -n 1)
            if [ -n "$prevPaths" ]; then
              for p in $prevPaths; do
                if basename "$p" | grep -qi "CAL"; then
                  calRunPath="$p"
                  break
                fi
              done
            fi
            if [ -n "$calRunPath" ]; then
              break
            fi
            prevRun=$((prevRun-1))
          done
      fi

      if [ -z "$calRunPath" ]; then
        echo "Error: No previous CAL run found before run $runit. Stopping execution."
        exit 1
      fi

      echo "For calibration, using run: $calRunPath"

        # Ensure calibration file exists (generate it if missing)
        if ! ensure_calibration_file "$calRunPath" "$outputDirectory"; then
          echo "Error: Calibration file could not be generated for run $runit. Stopping execution."
          exit 1
        fi

        calFileName=$(basename "$calRunPath")
        calFileName=${calFileName%.*}
        calFile="${outputDirectory}/${calFileName}.cal"
        echo "Using calibration file: $calFile"

      # Execute the conversion
      convert_data="./flat_convert ${filePath} ${outputDirectory}/${fileName}_converted.root --dune"
      echo "Executing command: "$convert_data
      $convert_data
      if [ $? -ne 0 ]; then
          echo "Error: Data conversion failed for run ${runit}. Skipping analysis for this run."
          fileName="" # reset fileName to exit loop for single file processing
          continue
      fi
      
      # Verify the converted file has valid trees
      if root -l -q "${outputDirectory}/${fileName}_converted.root" -e "TFile f(\"${outputDirectory}/${fileName}_converted.root\"); if (f.Get(\"raw_detector0\")) { cout << \"TREE_EXISTS\" << endl; } else { cout << \"TREE_MISSING\" << endl; }" 2>/dev/null | grep -q "TREE_MISSING"; then
          echo "Error: Conversion completed but output file is missing detector trees."
          echo "This usually means the source .dat file is corrupted or empty."
          echo "Removing bad converted file and skipping run ${runit}."
          rm -f "${outputDirectory}/${fileName}_converted.root"
          fileName="" # reset fileName to exit loop for single file processing
          continue
      fi
      echo "Conversion successful and verified: ${outputDirectory}/${fileName}_converted.root has valid detector trees."
    fi

    # Create a formatted file from the converted ROOT and feed it to clustering
    # Only proceed if we haven't skipped the entire chain
    if [ "$skipEntireChain" = false ]; then
        formattedFile="${outputDirectory}/${fileName}_formatted.root"

        if [ -f "${formattedFile}" ]; then
          # Validate that the formatted file has detector trees before skipping formatting
          if root -l -q "${formattedFile}" -e "TFile f(\"${formattedFile}\"); bool hasTree = (f.Get(\"detector_0\") != nullptr || f.Get(\"detector0\") != nullptr); if (hasTree) { cout << \"TREE_EXISTS\" << endl; } else { cout << \"TREE_MISSING\" << endl; }" 2>/dev/null | grep -q "TREE_EXISTS"; then
            echo "Formatted file already exists: ${formattedFile}. Skipping formatting step."
          else
            echo "Formatted file exists but is missing required detector trees. Removing and will re-format."
            rm -f "${formattedFile}"
      fi
    fi

    if [ ! -f "${formattedFile}" ]; then
      formatting_command="./formatting --cal-file ${calFile} --dune ${outputDirectory}/${fileName}_converted.root ${formattedFile}"
      echo "Executing command: ${formatting_command}"
      $formatting_command
      if [ $? -ne 0 ]; then
        echo "Error: Formatting failed for run ${runit}. Skipping clustering and report generation for this run."
        # If formatting fails, the converted file might be corrupted - remove it
        echo "Removing potentially corrupted converted file: ${outputDirectory}/${fileName}_converted.root"
        rm -f "${outputDirectory}/${fileName}_converted.root"
        fileName="" # reset fileName to exit loop for single file processing
        continue
      fi
    fi

    clustering_command="./clustering -i ${formattedFile} -o ${outputDirectory}/${fileName}_clusters.root -s ${nsigma}"

    echo "Executing command: ${clustering_command}"
    $clustering_command
    if [ $? -ne 0 ]; then
      echo "Error: Clustering failed for run ${runit}. Removing bad formatted file and skipping report generation for this run."
      rm -f "${formattedFile}"
      fileName="" # reset fileName to exit loop for single file processing
      continue
    fi
    fi  # End of skipEntireChain conditional

    # Generate report (always try if clusters file exists)
    clusteredFile="${outputDirectory}/${fileName}_clusters.root"
    if [ -f "${clusteredFile}" ]; then
        runReport_command="./runReport -i ${clusteredFile} -o ${outputDirectory} -s ${nsigma} -v"

        echo "Executing command: "$runReport_command
        $runReport_command
        if [ $? -ne 0 ]; then
            echo "Error: Report generation failed for run ${runit}."
            # Don't continue here as this is the last step
        fi
    else
        echo "Warning: Clustered file not found: ${clusteredFile}. Cannot generate report."
    fi

    fileName="" # reset fileName for the next iteration
  fi
 
done

echo "All runs have been analyzed. Exiting."
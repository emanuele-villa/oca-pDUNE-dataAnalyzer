// groupBeam.cpp

#include "cppLibs.h"
#include "rootLibs.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

#include "CmdLineParser.h"
#include "Logger.h"

LoggerInit([]{
  Logger::getUserHeader() << "[" << FILENAME << "]";
});

// Structure to hold beam configuration
struct BeamConfig {
    std::string date;        // YYYY-MM-DD
    std::string time;        // HH:MM
    double energy;           // GeV (+ or -)
    std::string target;
    std::string details;
    
    // Convert to timestamp (seconds since epoch) + offset within day
    std::pair<time_t, int> getTimestamp() const {
        std::tm tm = {};
        std::istringstream ss(date + " " + time);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M");
        return {std::mktime(&tm), tm.tm_hour * 3600 + tm.tm_min * 60};
    }
    
    // Get energy string for filename
    std::string getEnergyString() const {
        std::ostringstream oss;
        if (energy > 0) oss << "+";
        oss << std::fixed << std::setprecision(1) << energy << "GeV";
        return oss.str();
    }
};

// Function to parse beam_settings.dat
std::vector<BeamConfig> parseBeamSettings(const std::string& filename) {
    std::vector<BeamConfig> configs;
    std::ifstream file(filename);
    std::string line;
    
    if (!file.is_open()) {
        LogError << "Cannot open beam settings file: " << filename << std::endl;
        return configs;
    }
    
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#' || line.find("Date") != std::string::npos || 
            line.find("---") != std::string::npos) continue;
            
        std::istringstream iss(line);
        std::string token;
        std::vector<std::string> tokens;
        
        // Split by | delimiter
        while (std::getline(iss, token, '|')) {
            // Trim whitespace
            token.erase(0, token.find_first_not_of(" \t"));
            token.erase(token.find_last_not_of(" \t") + 1);
            tokens.push_back(token);
        }
        
        if (tokens.size() >= 7) {
            BeamConfig config;
            config.date = tokens[0];
            config.time = tokens[1];
            try {
                config.energy = std::stod(tokens[2]);
            } catch (...) {
                continue; // Skip malformed entries
            }
            config.target = tokens[3];
            config.details = tokens[6];
            configs.push_back(config);
        }
    }
    
    LogInfo << "Loaded " << configs.size() << " beam configurations" << std::endl;
    return configs;
}

// Function to parse filename and extract base UTC time
std::tm parseFilenameTime(const std::string& filename) {
    std::tm baseTime = {};
    
    // Extract date and time from filename like SCD_RUN00500_BEAM_20250902_121516
    size_t datePos = filename.find("_BEAM_");
    if (datePos != std::string::npos) {
        datePos += 6; // Skip "_BEAM_"
        if (datePos + 15 <= filename.length()) {
            std::string dateStr = filename.substr(datePos, 8);     // YYYYMMDD
            std::string timeStr = filename.substr(datePos + 9, 6); // HHMMSS
            
            baseTime.tm_year = std::stoi(dateStr.substr(0, 4)) - 1900;
            baseTime.tm_mon = std::stoi(dateStr.substr(4, 2)) - 1;
            baseTime.tm_mday = std::stoi(dateStr.substr(6, 2));
            baseTime.tm_hour = std::stoi(timeStr.substr(0, 2));
            baseTime.tm_min = std::stoi(timeStr.substr(2, 2));
            baseTime.tm_sec = std::stoi(timeStr.substr(4, 2));
            
            LogInfo << "Parsed filename time: " << std::put_time(&baseTime, "%Y-%m-%d %H:%M:%S UTC") << std::endl;
        }
    }
    
    return baseTime;
}

// Function to convert internal timestamp + base time to CEST
std::tm eventToTime(const std::tm& baseTimeUTC, Long64_t internalTimestamp) {
    // Convert base time to seconds since epoch (UTC)
    std::tm baseTimeCopy = baseTimeUTC;
    time_t baseSeconds = timegm(&baseTimeCopy); // timegm for UTC
    
    // Add internal timestamp (20ns ticks to seconds)
    double tickSeconds = internalTimestamp * 20e-9;
    time_t eventSecondsUTC = baseSeconds + (time_t)tickSeconds;
    
    // Convert to CEST (UTC + 2 hours)
    time_t eventSecondsCEST = eventSecondsUTC + 2 * 3600;
    
    std::tm* eventTime = std::gmtime(&eventSecondsCEST);
    return *eventTime;
}

// Get date string in YYYYMMDD format from filename (not timestamp)
std::string getDateFromFilename(const std::string& filename) {
    size_t datePos = filename.find("_BEAM_");
    if (datePos != std::string::npos) {
        datePos += 6; // Skip "_BEAM_"
        if (datePos + 8 <= filename.length()) {
            return filename.substr(datePos, 8); // YYYYMMDD
        }
    }
    return "unknown";
}

// Find beam configuration for given event time
BeamConfig findBeamConfig(const std::tm& eventTime, const std::vector<BeamConfig>& configs) {
    time_t eventTimestamp = timegm(const_cast<std::tm*>(&eventTime));
    
    // Find the most recent configuration before this timestamp
    BeamConfig bestConfig;
    time_t bestTime = 0;
    
    for (const auto& config : configs) {
        auto [configTime, configOffset] = config.getTimestamp();
        if (configTime <= eventTimestamp && configTime > bestTime) {
            bestTime = configTime;
            bestConfig = config;
        }
    }
    
    return bestConfig;
}

// Structure to hold event data for copying
struct EventData {
    Int_t event_index;
    Long64_t evt_size, fw_version, trigger_number, board_id, timestamp, ext_timestamp, trigger_id, file_offset;
    std::vector<std::vector<float>> detectorData;
    std::vector<std::vector<float>> rawDetectorData;
    
    // Clusters data
    std::vector<int> cluster_detector;
    std::vector<int> cluster_start_ch;
    std::vector<int> cluster_end_ch;
    std::vector<int> cluster_size;
    std::vector<float> cluster_amplitude;
    
    EventData() : detectorData(4), rawDetectorData(4) {}
};

int main(int argc, char* argv[]) {
    CmdLineParser clp;

    clp.getDescription() << "> This program groups _clusters.root files by date and beam energy based on timestamps." << std::endl;

    clp.addDummyOption("Main options");
    clp.addOption("inputDir", {"-i", "--input"}, "Input directory containing _clusters.root files");
    clp.addOption("outputDir", {"-o", "--output"}, "Output directory for grouped files");
    clp.addOption("beamSettings", {"-b", "--beam-settings"}, "Beam settings file (default: parameters/beam_settings.dat)");

    clp.addDummyOption("Triggers");
    clp.addTriggerOption("verboseMode", {"-v"}, "Run in verbose mode");
    clp.addTriggerOption("forceOverwrite", {"-f", "--force"}, "Force overwrite existing grouped ROOT files");

    clp.addDummyOption();

    LogInfo << clp.getDescription().str() << std::endl;
    LogInfo << "Usage: " << std::endl;
    LogInfo << clp.getConfigSummary() << std::endl << std::endl;

    clp.parseCmdLine(argc, argv);

    LogThrowIf(clp.isNoOptionTriggered(), "No option was provided.");

    LogInfo << "Provided arguments: " << std::endl;
    LogInfo << clp.getValueSummary() << std::endl << std::endl;

    bool verbose = clp.isOptionTriggered("verboseMode");
    bool forceOverwrite = clp.isOptionTriggered("forceOverwrite");
    
    std::string inputDir = clp.getOptionVal<std::string>("inputDir");
    std::string outputDir = clp.getOptionVal<std::string>("outputDir");
    std::string beamSettingsFile = "parameters/beam_settings.dat";
    if (clp.isOptionTriggered("beamSettings")) {
        beamSettingsFile = clp.getOptionVal<std::string>("beamSettings");
    }

    // Create output directory
    std::filesystem::create_directories(outputDir);

    // Parse beam configurations
    auto beamConfigs = parseBeamSettings(beamSettingsFile);
    if (beamConfigs.empty()) {
        LogError << "No beam configurations loaded!" << std::endl;
        return 1;
    }

    // Find all _clusters.root files
    std::vector<std::string> inputFiles;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(inputDir)) {
        if (entry.is_regular_file() && entry.path().string().find("_clusters.root") != std::string::npos) {
            // Skip CAL files, only process BEAM files
            if (entry.path().string().find("_BEAM_") != std::string::npos) {
                inputFiles.push_back(entry.path().string());
            }
        }
    }

    std::sort(inputFiles.begin(), inputFiles.end());
    LogInfo << "Found " << inputFiles.size() << " formatted ROOT files" << std::endl;

    // Map to store output files: date_energy -> TFile*
    std::map<std::string, TFile*> outputFiles;
    std::map<std::string, TTree*> outputTrees_eventInfo;
    std::map<std::string, TTree*> outputTrees_clusters;
    std::map<std::string, std::vector<TTree*>> outputTrees_detectors;
    std::map<std::string, std::vector<TTree*>> outputTrees_rawDetectors;
    std::map<std::string, std::vector<TTree*>> outputTrees_pedestals;
    std::map<std::string, std::vector<TTree*>> outputTrees_sigmas;
    
    // Track output keys that should be skipped (file exists and not forcing overwrite)
    std::set<std::string> skippedOutputKeys;

    const int nDetectors = 4;
    const int nChannels = 384;

    struct OutputStaticData {
        std::array<std::vector<float>, nDetectors> pedestals;
        std::array<std::vector<float>, nDetectors> sigmas;
    };

    std::map<std::string, OutputStaticData> staticDataMap;

    // Persist event buffers across files so ROOT branches always see valid memory
    EventData eventData;

    // Process each input file
    for (const auto& inputFile : inputFiles) {
        // Skip known problematic files (contain corrupted data causing ROOT buffer overflow or segfaults)
        // Note: More files may need to be added as they are discovered during processing
        if (inputFile.find("SCD_RUN00274") != std::string::npos ||
            inputFile.find("SCD_RUN00276") != std::string::npos ||
            inputFile.find("SCD_RUN00278") != std::string::npos ||
            inputFile.find("SCD_RUN00280") != std::string::npos ||
            inputFile.find("SCD_RUN00282") != std::string::npos ||
            inputFile.find("SCD_RUN00284") != std::string::npos ||
            inputFile.find("SCD_RUN00286") != std::string::npos ||
            inputFile.find("SCD_RUN00288") != std::string::npos ||
            inputFile.find("SCD_RUN00290") != std::string::npos ||
            inputFile.find("SCD_RUN00292") != std::string::npos ||
            inputFile.find("SCD_RUN00294") != std::string::npos ||
            inputFile.find("SCD_RUN00296") != std::string::npos ||
            inputFile.find("SCD_RUN00298") != std::string::npos) {
            LogWarning << "Skipping known problematic file: " << inputFile << std::endl;
            continue;
        }
        
        try {
            LogInfo << "Processing: " << inputFile << std::endl;
            
            TFile* inFile = new TFile(inputFile.c_str(), "READ");
            if (!inFile || !inFile->IsOpen() || inFile->IsZombie()) {
                LogError << "Cannot open or corrupted file: " << inputFile << std::endl;
                if (inFile) delete inFile;
                continue;
            }
            
            // Validate file integrity
            if (inFile->TestBit(TFile::kRecovered)) {
                LogWarning << "File was recovered, may be corrupted: " << inputFile << std::endl;
                inFile->Close();
                delete inFile;
                continue;
            }

            // Get trees
            TTree* eventInfoTree = (TTree*)inFile->Get("event_info");
            if (!eventInfoTree) {
                LogError << "No event_info tree in file: " << inputFile << std::endl;
                inFile->Close();
                delete inFile;
                continue;
            }

            TTree* clustersTree = (TTree*)inFile->Get("clusters");
            if (!clustersTree) {
                LogError << "No clusters tree in file: " << inputFile << std::endl;
                inFile->Close();
                delete inFile;
                continue;
            }
            
            // Validate tree entries
            Long64_t nEntries = eventInfoTree->GetEntries();
            if (nEntries <= 0 || nEntries > 10000000) {
                LogError << "Suspicious entry count (" << nEntries << ") in file: " << inputFile << std::endl;
                inFile->Close();
                delete inFile;
                continue;
            }

    std::vector<TTree*> detectorTrees(nDetectors), rawDetectorTrees(nDetectors);
    std::vector<TTree*> pedestalTrees(nDetectors), sigmaTrees(nDetectors);
    const size_t expectedChannels = static_cast<size_t>(nChannels);
    std::vector<bool> detSizeWarned(nDetectors, false);
    std::vector<bool> rawSizeWarned(nDetectors, false);
        
        for (int d = 0; d < nDetectors; ++d) {
            detectorTrees[d] = (TTree*)inFile->Get(Form("detector%d", d));
            rawDetectorTrees[d] = (TTree*)inFile->Get(Form("raw_detector%d", d));
            pedestalTrees[d] = (TTree*)inFile->Get(Form("pedestal%d", d));
            sigmaTrees[d] = (TTree*)inFile->Get(Form("sigma%d", d));
        }

    // Set up branches for reading
        eventInfoTree->SetBranchAddress("event_index", &eventData.event_index);
        eventInfoTree->SetBranchAddress("evt_size", &eventData.evt_size);
        eventInfoTree->SetBranchAddress("fw_version", &eventData.fw_version);
        eventInfoTree->SetBranchAddress("trigger_number", &eventData.trigger_number);
        eventInfoTree->SetBranchAddress("board_id", &eventData.board_id);
        eventInfoTree->SetBranchAddress("timestamp", &eventData.timestamp);
        eventInfoTree->SetBranchAddress("ext_timestamp", &eventData.ext_timestamp);
        eventInfoTree->SetBranchAddress("trigger_id", &eventData.trigger_id);
        eventInfoTree->SetBranchAddress("file_offset", &eventData.file_offset);

        // Set up clusters tree branches with pointers
        std::vector<int>* detectorPtr = nullptr;
        std::vector<int>* startChPtr = nullptr;
        std::vector<int>* endChPtr = nullptr;
        std::vector<int>* sizePtr = nullptr;
        std::vector<float>* amplitudePtr = nullptr;
        
        clustersTree->SetBranchAddress("detector", &detectorPtr);
        clustersTree->SetBranchAddress("start_ch", &startChPtr);
        clustersTree->SetBranchAddress("end_ch", &endChPtr);
        clustersTree->SetBranchAddress("size", &sizePtr);
        clustersTree->SetBranchAddress("amplitude", &amplitudePtr);

        // Set up detector data branches
        std::vector<std::vector<float>*> detDataPtrs(nDetectors), rawDetDataPtrs(nDetectors);
        std::vector<std::vector<float>*> pedDataPtrs(nDetectors), sigDataPtrs(nDetectors);
        
        for (int d = 0; d < nDetectors; ++d) {
            detDataPtrs[d] = new std::vector<float>();
            rawDetDataPtrs[d] = new std::vector<float>();
            pedDataPtrs[d] = new std::vector<float>();
            sigDataPtrs[d] = new std::vector<float>();
            
            if (detectorTrees[d]) detectorTrees[d]->SetBranchAddress("data", &detDataPtrs[d]);
            if (rawDetectorTrees[d]) rawDetectorTrees[d]->SetBranchAddress("raw_data", &rawDetDataPtrs[d]);
            if (pedestalTrees[d]) pedestalTrees[d]->SetBranchAddress("pedestal", &pedDataPtrs[d]);
            if (sigmaTrees[d]) sigmaTrees[d]->SetBranchAddress("sigma", &sigDataPtrs[d]);
        }

            // Parse base time from filename
            std::tm baseTime = parseFilenameTime(inputFile);
            
            // Process events - nEntries already validated above
            for (Long64_t entry = 0; entry < nEntries; ++entry) {
            if (verbose && entry < 5) LogInfo << "Reading entry " << entry << " from file " << inputFile << std::endl;
            eventInfoTree->GetEntry(entry);
            clustersTree->GetEntry(entry);
            
            // Copy cluster data with bounds checks (ensure consistent sizes)
            const size_t maxClusterEntries = 10000;
            size_t clusterCopyCount = maxClusterEntries;
            clusterCopyCount = detectorPtr   ? std::min(clusterCopyCount, detectorPtr->size())   : 0;
            clusterCopyCount = startChPtr    ? std::min(clusterCopyCount, startChPtr->size())    : 0;
            clusterCopyCount = endChPtr      ? std::min(clusterCopyCount, endChPtr->size())      : 0;
            clusterCopyCount = sizePtr       ? std::min(clusterCopyCount, sizePtr->size())       : 0;
            clusterCopyCount = amplitudePtr  ? std::min(clusterCopyCount, amplitudePtr->size())  : clusterCopyCount;

            eventData.cluster_detector.assign(clusterCopyCount, 0);
            eventData.cluster_start_ch.assign(clusterCopyCount, 0);
            eventData.cluster_end_ch.assign(clusterCopyCount, 0);
            eventData.cluster_size.assign(clusterCopyCount, 0);
            eventData.cluster_amplitude.assign(clusterCopyCount, 0.0f);

            auto copyClusterIntVector = [&](const std::vector<int>* srcPtr, std::vector<int>& dst, const char* label) {
                if (srcPtr && clusterCopyCount > 0) {
                    if (srcPtr->size() > clusterCopyCount) {
                        LogWarning << label << " vector size (" << srcPtr->size() << ") exceeds " << clusterCopyCount
                                   << ", truncating." << std::endl;
                    }
                    for (size_t idx = 0; idx < clusterCopyCount; ++idx) {
                        dst[idx] = (*srcPtr)[idx];
                    }
                }
            };

            copyClusterIntVector(detectorPtr, eventData.cluster_detector, "Cluster detector index");
            copyClusterIntVector(startChPtr, eventData.cluster_start_ch, "Cluster start channel");
            copyClusterIntVector(endChPtr, eventData.cluster_end_ch, "Cluster end channel");
            copyClusterIntVector(sizePtr, eventData.cluster_size, "Cluster size");

            if (amplitudePtr && clusterCopyCount > 0) {
                if (amplitudePtr->size() > clusterCopyCount) {
                    LogWarning << "Cluster amplitude vector size (" << amplitudePtr->size() << ") exceeds "
                               << clusterCopyCount << ", truncating." << std::endl;
                }
                for (size_t idx = 0; idx < clusterCopyCount; ++idx) {
                    const float val = (*amplitudePtr)[idx];
                    eventData.cluster_amplitude[idx] = std::isfinite(val) ? val : 0.0f;
                }
            }
            
            // Read detector data for this event and clamp to expected channel count
            for (int d = 0; d < nDetectors; ++d) {
                eventData.detectorData[d].assign(expectedChannels, 0.0f);
                eventData.rawDetectorData[d].assign(expectedChannels, 0.0f);

                if (detectorTrees[d]) {
                    detectorTrees[d]->GetEntry(entry);
                    if (detDataPtrs[d] != nullptr && !detDataPtrs[d]->empty()) {
                        const auto& src = *detDataPtrs[d];
                        const size_t copyCount = std::min(src.size(), expectedChannels);
                        for (size_t idx = 0; idx < copyCount; ++idx) {
                            const float val = src[idx];
                            eventData.detectorData[d][idx] = std::isfinite(val) ? val : 0.0f;
                        }
                        if (src.size() != expectedChannels && !detSizeWarned[d]) {
                            LogWarning << "Detector " << d << " data size " << src.size()
                                       << " (expected " << expectedChannels << ") in file " << inputFile
                                       << ". Clamping to " << expectedChannels << " elements." << std::endl;
                            detSizeWarned[d] = true;
                        }
                    }
                } else if (entry == 0) {
                    LogWarning << "Missing detector tree for detector " << d << " in file " << inputFile << std::endl;
                }

                if (rawDetectorTrees[d]) {
                    rawDetectorTrees[d]->GetEntry(entry);
                    if (rawDetDataPtrs[d] != nullptr && !rawDetDataPtrs[d]->empty()) {
                        const auto& src = *rawDetDataPtrs[d];
                        const size_t copyCount = std::min(src.size(), expectedChannels);
                        for (size_t idx = 0; idx < copyCount; ++idx) {
                            const float val = src[idx];
                            eventData.rawDetectorData[d][idx] = std::isfinite(val) ? val : 0.0f;
                        }
                        if (src.size() != expectedChannels && !rawSizeWarned[d]) {
                            LogWarning << "Raw detector " << d << " data size " << src.size()
                                       << " (expected " << expectedChannels << ") in file " << inputFile
                                       << ". Clamping to " << expectedChannels << " elements." << std::endl;
                            rawSizeWarned[d] = true;
                        }
                    }
                } else if (entry == 0) {
                    LogWarning << "Missing raw detector tree for detector " << d << " in file " << inputFile << std::endl;
                }
            }

            // Calculate actual event time (base time + internal timestamp)
            std::tm eventTime = eventToTime(baseTime, eventData.timestamp);
            
            // Check if timestamp is reasonable (allow up to ~1 hour of ticks: 1e12)
            if (eventData.timestamp < 0 || eventData.timestamp > 1e12) {
                LogWarning << "Invalid timestamp: " << eventData.timestamp << ", skipping entry " << entry << std::endl;
                continue;
            }
            
            // Determine beam configuration and output file
            BeamConfig config = findBeamConfig(eventTime, beamConfigs);
            std::string dateStr = getDateFromFilename(inputFile);  // Use date from filename
            std::string energyStr = config.getEnergyString();
            std::string outputKey = dateStr + "_" + energyStr;
            
            // Skip processing if this output key was already skipped due to existing file
            if (skippedOutputKeys.find(outputKey) != skippedOutputKeys.end()) {
                continue;  // Skip this entry silently
            }

            if (verbose && entry % 1000 == 0) {
                LogInfo << "Event " << entry << " -> " << outputKey << " | detSizes=[";
                for (int dd = 0; dd < nDetectors; ++dd) {
                    LogInfo << eventData.detectorData[dd].size();
                    if (dd < nDetectors-1) LogInfo << ",";
                }
                LogInfo << "] rawSizes=[";
                for (int dd = 0; dd < nDetectors; ++dd) {
                    LogInfo << eventData.rawDetectorData[dd].size();
                    if (dd < nDetectors-1) LogInfo << ",";
                }
                LogInfo << "] clusters=" << eventData.cluster_amplitude.size() << std::endl;
            }

            // Create output file if needed
            if (outputFiles.find(outputKey) == outputFiles.end()) {
                std::string outputFileName = outputDir + "/" + outputKey + ".root";
                
                // Check if file already exists and skip if not forcing overwrite
                if (!forceOverwrite && std::filesystem::exists(outputFileName)) {
                    LogInfo << "Output file already exists, skipping: " << outputFileName << " (use -f to overwrite)" << std::endl;
                    // Mark this key as skipped so we don't try to process it again
                    skippedOutputKeys.insert(outputKey);
                    continue;
                }
                
                LogInfo << "Creating output file: " << outputFileName << std::endl;
                outputFiles[outputKey] = new TFile(outputFileName.c_str(), "RECREATE");
                
                // Create event_info tree
                outputTrees_eventInfo[outputKey] = new TTree("event_info", "event_info");
                TTree* outEventTree = outputTrees_eventInfo[outputKey];
                outEventTree->Branch("event_index", &eventData.event_index, "event_index/I");
                outEventTree->Branch("evt_size", &eventData.evt_size, "evt_size/L");
                outEventTree->Branch("fw_version", &eventData.fw_version, "fw_version/L");
                outEventTree->Branch("trigger_number", &eventData.trigger_number, "trigger_number/L");
                outEventTree->Branch("board_id", &eventData.board_id, "board_id/L");
                outEventTree->Branch("timestamp", &eventData.timestamp, "timestamp/L");
                outEventTree->Branch("ext_timestamp", &eventData.ext_timestamp, "ext_timestamp/L");
                outEventTree->Branch("trigger_id", &eventData.trigger_id, "trigger_id/L");
                outEventTree->Branch("file_offset", &eventData.file_offset, "file_offset/L");

                // Create clusters tree
                outputTrees_clusters[outputKey] = new TTree("clusters", "Clusters per event");
                TTree* outClustersTree = outputTrees_clusters[outputKey];
                outClustersTree->Branch("detector", &eventData.cluster_detector);
                outClustersTree->Branch("start_ch", &eventData.cluster_start_ch);
                outClustersTree->Branch("end_ch", &eventData.cluster_end_ch);
                outClustersTree->Branch("size", &eventData.cluster_size);
                outClustersTree->Branch("amplitude", &eventData.cluster_amplitude);

                // Create detector trees
                outputTrees_detectors[outputKey].resize(nDetectors);
                outputTrees_rawDetectors[outputKey].resize(nDetectors);
                outputTrees_pedestals[outputKey].resize(nDetectors);
                outputTrees_sigmas[outputKey].resize(nDetectors);

                // Initialize static data storage for this output file and read pedestals/sigmas once
                auto& staticData = staticDataMap[outputKey];
                for (int d = 0; d < nDetectors; ++d) {
                    auto& pedVecOut = staticData.pedestals[d];
                    auto& sigmaVecOut = staticData.sigmas[d];

                    pedVecOut.assign(expectedChannels, 0.0f);
                    sigmaVecOut.assign(expectedChannels, 0.0f);

                    if (pedestalTrees[d] && pedestalTrees[d]->GetEntries() > 0) {
                        pedestalTrees[d]->GetEntry(0);
                        if (pedDataPtrs[d] != nullptr) {
                            const auto& pedVecIn = *pedDataPtrs[d];
                            const size_t copyCount = std::min(pedVecIn.size(), expectedChannels);
                            for (size_t idx = 0; idx < copyCount; ++idx) {
                                const float val = pedVecIn[idx];
                                pedVecOut[idx] = std::isfinite(val) ? val : 0.0f;
                            }
                            if (pedVecIn.size() != expectedChannels) {
                                LogWarning << "Pedestal vector size mismatch for detector " << d
                                           << " (" << pedVecIn.size() << " elements, expected " << expectedChannels
                                           << "). Clamping data to " << expectedChannels << "." << std::endl;
                            }
                        }
                    }

                    if (sigmaTrees[d] && sigmaTrees[d]->GetEntries() > 0) {
                        sigmaTrees[d]->GetEntry(0);
                        if (sigDataPtrs[d] != nullptr) {
                            const auto& sigmaVecIn = *sigDataPtrs[d];
                            const size_t copyCount = std::min(sigmaVecIn.size(), expectedChannels);
                            for (size_t idx = 0; idx < copyCount; ++idx) {
                                const float val = sigmaVecIn[idx];
                                sigmaVecOut[idx] = std::isfinite(val) ? val : 0.0f;
                            }
                            if (sigmaVecIn.size() != expectedChannels) {
                                LogWarning << "Sigma vector size mismatch for detector " << d
                                           << " (" << sigmaVecIn.size() << " elements, expected " << expectedChannels
                                           << "). Clamping data to " << expectedChannels << "." << std::endl;
                            }
                        }
                    }
                }

                for (int d = 0; d < nDetectors; ++d) {
                    // Detector data trees
                    outputTrees_detectors[outputKey][d] = new TTree(Form("detector%d", d), Form("Baseline-subtracted data detector %d", d));
                    outputTrees_detectors[outputKey][d]->Branch("data", &eventData.detectorData[d]);

                    outputTrees_rawDetectors[outputKey][d] = new TTree(Form("raw_detector%d", d), Form("Raw data detector %d", d));
                    outputTrees_rawDetectors[outputKey][d]->Branch("raw_data", &eventData.rawDetectorData[d]);

                    outputTrees_pedestals[outputKey][d] = new TTree(Form("pedestal%d", d), Form("Pedestals detector %d", d));
                    outputTrees_pedestals[outputKey][d]->Branch("pedestal", &staticData.pedestals[d]);
                    LogInfo << "Static pedestal vector size for " << outputKey << " detector " << d << ": " << staticData.pedestals[d].size() << std::endl;
                    if (staticData.pedestals[d].size() <= 200000) {
                        outputTrees_pedestals[outputKey][d]->Fill();
                        outputTrees_pedestals[outputKey][d]->Write("", TObject::kOverwrite);
                    } else {
                        LogError << "Refusing to write oversized pedestal vector for " << outputKey << " detector " << d << ": size=" << staticData.pedestals[d].size() << std::endl;
                    }

                    outputTrees_sigmas[outputKey][d] = new TTree(Form("sigma%d", d), Form("Sigma detector %d", d));
                    outputTrees_sigmas[outputKey][d]->Branch("sigma", &staticData.sigmas[d]);
                    LogInfo << "Static sigma vector size for " << outputKey << " detector " << d << ": " << staticData.sigmas[d].size() << std::endl;
                    if (staticData.sigmas[d].size() <= 200000) {
                        outputTrees_sigmas[outputKey][d]->Fill();
                        outputTrees_sigmas[outputKey][d]->Write("", TObject::kOverwrite);
                    } else {
                        LogError << "Refusing to write oversized sigma vector for " << outputKey << " detector " << d << ": size=" << staticData.sigmas[d].size() << std::endl;
                    }
                }
            }

            // Sanity-check sizes before filling to avoid ROOT integer overflows
            bool skipEntry = false;
            // Check event_info/cluster vectors (cluster sizes are already bounded earlier)
            for (int d = 0; d < nDetectors; ++d) {
                const auto& detv = eventData.detectorData[d];
                const auto& rawv = eventData.rawDetectorData[d];
                const auto& pedv = staticDataMap[outputKey].pedestals[d];
                const auto& sigv = staticDataMap[outputKey].sigmas[d];

                if (detv.size() > 200000 || rawv.size() > 200000) {
                    LogError << "Suspiciously large detector vector size for detector " << d << ": det=" << detv.size() << ", raw=" << rawv.size() << ", skipping entry " << entry << std::endl;
                    skipEntry = true; break;
                }
                if (pedv.size() > 200000 || sigv.size() > 200000) {
                    LogError << "Suspiciously large static pedestal/sigma for detector " << d << ": ped=" << pedv.size() << ", sig=" << sigv.size() << ", skipping entry " << entry << std::endl;
                    skipEntry = true; break;
                }
            }

            if (skipEntry) {
                // Skip writing this entry to avoid corrupting output and to help debugging
                continue;
            }

            // Fill trees
            outputTrees_eventInfo[outputKey]->Fill();
            outputTrees_clusters[outputKey]->Fill();
            for (int d = 0; d < nDetectors; ++d) {
                if (verbose) LogInfo << "Filling detector tree for " << outputKey << " det " << d << " entry " << entry << std::endl;
                outputTrees_detectors[outputKey][d]->Fill();
                if (verbose) LogInfo << "Filled detector tree for " << outputKey << " det " << d << " entry " << entry << std::endl;

                if (verbose) LogInfo << "Filling raw detector tree for " << outputKey << " det " << d << " entry " << entry << std::endl;
                outputTrees_rawDetectors[outputKey][d]->Fill();
                if (verbose) LogInfo << "Filled raw detector tree for " << outputKey << " det " << d << " entry " << entry << std::endl;
            }
        }

            // Clean up
            for (int d = 0; d < nDetectors; ++d) {
                delete detDataPtrs[d];
                delete rawDetDataPtrs[d];
                delete pedDataPtrs[d];
                delete sigDataPtrs[d];
            }
            
            inFile->Close();
            delete inFile;
            
        } catch (const std::exception& e) {
            LogError << "Exception while processing file " << inputFile << ": " << e.what() << std::endl;
            LogWarning << "Skipping this file and continuing..." << std::endl;
        } catch (...) {
            LogError << "Unknown exception while processing file: " << inputFile << std::endl;
            LogWarning << "Skipping this file and continuing..." << std::endl;
        }
    }

    // Close all output files
    for (auto& [key, file] : outputFiles) {
        LogInfo << "Finalizing file: " << key << ".root" << std::endl;
        file->Write();
        file->Close();
        delete file;
    }

    LogInfo << "Grouping complete! Created " << outputFiles.size() << " output files." << std::endl;
    return 0;
}
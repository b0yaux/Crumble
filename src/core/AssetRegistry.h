#pragma once
#include "ofMain.h"

/**
 * A Logical Asset represents a group of related media streams (Video, Audio, etc.)
 * that share a common name or identity.
 */
struct LogicalAsset {
    std::string name;
    std::string videoPath;
    std::string audioPath;
    
    bool hasVideo() const { return !videoPath.empty(); }
    bool hasAudio() const { return !audioPath.empty(); }
    bool isComplete() const { return hasVideo() && hasAudio(); }
};

/**
 * AssetRegistry manages the mapping between logical names (e.g. "birds", "drums:5")
 * and physical file paths.
 */
class AssetRegistry {
public:
    static AssetRegistry& get() {
        static AssetRegistry instance;
        return instance;
    }

    /**
     * Scans a list of directories and builds the logical asset map.
     * Groups files with the same name (e.g. "clip1.mov" + "clip1.wav" -> "clip1")
     */
    void scan(const std::vector<std::string>& searchPaths);

    /**
     * Resolves a logical path to a specific stream path based on the requested extension/type.
     * 
     * Supported syntaxes:
     * - "birds"        (Logical Asset): Resolves to the MOV/WAV pair named 'birds'.
     * - "drums:5"      (Bank Index): Resolves to the 6th asset pair in the 'drums' folder.
     * - "drums:snare"  (Bank Named): Resolves to the 'snare' asset within the 'drums' bank.
     * 
     * @param logicalPath The user-provided identifier.
     * @param typeHint The component to retrieve ("audio" or "video").
     */
    std::string resolve(const std::string& logicalPath, const std::string& typeHint = "") const;

    const std::map<std::string, LogicalAsset>& getAssets() const { return assets; }
    const std::map<std::string, std::vector<LogicalAsset>>& getBanks() const { return banks; }

private:
    AssetRegistry() = default;
    
    std::map<std::string, LogicalAsset> assets;
    std::map<std::string, std::vector<LogicalAsset>> banks;
    
    void processDirectory(const std::string& path, const std::string& bankName = "");
    void registerFile(const std::string& filePath, const std::string& bankName = "");
};

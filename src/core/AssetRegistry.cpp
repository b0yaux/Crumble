#include "AssetRegistry.h"

void AssetRegistry::scan(const std::vector<std::string>& searchPaths) {
    assets.clear();
    banks.clear();

    for (auto& path : searchPaths) {
        // Expand tilde
        std::string expanded = path;
        if (!expanded.empty() && expanded[0] == '~') {
            const char* home = getenv("HOME");
            if (home) expanded = std::string(home) + expanded.substr(1);
        }
        
        processDirectory(expanded);
    }
    
    ofLogNotice("AssetRegistry") << "Scan complete: " << assets.size() << " logical assets, " << banks.size() << " banks discovered.";
}

void AssetRegistry::processDirectory(const std::string& path, const std::string& bankName) {
    ofDirectory dir(path);
    if (!dir.exists()) return;

    dir.listDir();
    
    // Determine the name of this bank if not provided
    std::string currentBank = bankName;
    if (currentBank.empty()) {
        currentBank = ofFilePath::getFileName(path);
    }

    for (int i = 0; i < dir.size(); i++) {
        std::string filePath = dir.getPath(i);
        ofFile file(filePath);
        
        if (file.isDirectory()) {
            // Recurse into subdirectories as new banks
            processDirectory(filePath, ofFilePath::getFileName(filePath));
        } else {
            registerFile(filePath, currentBank);
        }
    }
}

void AssetRegistry::registerFile(const std::string& filePath, const std::string& bankName) {
    std::string ext = ofToLower(ofFilePath::getFileExt(filePath));
    if (ext != "mov" && ext != "hap" && ext != "mp4" && ext != "wav" && ext != "mp3" && ext != "aif") {
        return;
    }

    std::string baseName = ofFilePath::removeExt(ofFilePath::getFileName(filePath));
    
    // 1. Register in global assets map (by base name)
    LogicalAsset& asset = assets[baseName];
    asset.name = baseName;
    
    if (ext == "mov" || ext == "hap" || ext == "mp4") {
        asset.videoPath = filePath;
    } else {
        asset.audioPath = filePath;
    }

    // 2. Register in the bank
    if (!bankName.empty()) {
        auto& bank = banks[bankName];
        
        // Find if this asset already exists in this bank
        bool found = false;
        for (auto& bAsset : bank) {
            if (bAsset.name == baseName) {
                if (ext == "mov" || ext == "hap" || ext == "mp4") bAsset.videoPath = filePath;
                else bAsset.audioPath = filePath;
                found = true;
                break;
            }
        }
        
        if (!found) {
            LogicalAsset newBAsset;
            newBAsset.name = baseName;
            if (ext == "mov" || ext == "hap" || ext == "mp4") newBAsset.videoPath = filePath;
            else newBAsset.audioPath = filePath;
            bank.push_back(newBAsset);
        }
    }
}

std::string AssetRegistry::resolve(const std::string& logicalPath, const std::string& typeHint) const {
    if (logicalPath.empty()) return "";
    
    // 0. Absolute Path Check (Bypass registry if it's already a direct file)
    if (ofFilePath::isAbsolute(logicalPath)) {
        return logicalPath;
    }

    // 1. Parse bank:index or bank:name
    size_t colonPos = logicalPath.find(':');
    if (colonPos != std::string::npos) {
        std::string bankName = logicalPath.substr(0, colonPos);
        std::string selector = logicalPath.substr(colonPos + 1);

        auto it = banks.find(bankName);
        if (it != banks.end()) {
            const auto& bank = it->second;
            
            // Try as index first
            try {
                size_t idx = std::stoi(selector);
                if (idx < bank.size()) {
                    const LogicalAsset& a = bank[idx];
                    std::string result = (typeHint == "audio") ? a.audioPath : a.videoPath;
                    ofLogNotice("AssetRegistry") << "Resolved " << logicalPath << " (" << typeHint << ") -> " << result;
                    if (!result.empty()) return result;
                }
            } catch (...) {
                // Not a number, try as name within bank
                for (const auto& a : bank) {
                    if (a.name == selector) {
                        std::string result = (typeHint == "audio") ? a.audioPath : a.videoPath;
                        ofLogNotice("AssetRegistry") << "Resolved " << logicalPath << " (" << typeHint << ") -> " << result;
                        if (!result.empty()) return result;
                    }
                }
            }
        }
    }

    // 2. Try as global asset name
    auto it = assets.find(logicalPath);
    if (it != assets.end()) {
        std::string result = (typeHint == "audio") ? it->second.audioPath : it->second.videoPath;
        if (!result.empty()) return result;
    }

    // 3. Fallback to raw path (honestly failed to resolve)
    return "";
}

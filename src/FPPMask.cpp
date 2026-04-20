#include <drogon/HttpAppFramework.h>
#undef LOG_WARN
#undef LOG_INFO
#undef LOG_DEBUG

#include <fpp-pch.h>

#include <atomic>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Plugin.h"
#include "Plugins.h"
#include "settings.h"
#include "common.h"
#include "log.h"
#include "MultiSync.h"
#include "Events.h"
#include "channeloutput/ChannelOutputSetup.h"
#include "fseq/FSEQFile.h"

struct MaskData {
    std::string path;
    std::vector<uint8_t> frames;
    uint32_t numFrames = 0;
    uint32_t channelCount = 0;
    uint32_t stepMs = 50;
};

class FPPMaskPlugin : public FPPPlugins::Plugin,
                      public FPPPlugins::ChannelDataPlugin,
                      public FPPPlugins::APIProviderPlugin {
public:
    FPPMaskPlugin()
      : FPPPlugins::Plugin("fpp-mask", true),
        FPPPlugins::ChannelDataPlugin(),
        FPPPlugins::APIProviderPlugin() {
        configPath = FPP_DIR_CONFIG("/plugin.fpp-mask.json");
        loadConfig();
        startMs = GetTimeMS();
        if (!currentMaskFile.empty()) {
            loadMask(currentMaskFile);
        }
    }

    void modifyChannelData(int /*ms*/, uint8_t* seqData) override {
        if (!enabled.load(std::memory_order_relaxed)) return;
        std::shared_ptr<MaskData> mask = currentMask.load();
        if (!mask || mask->numFrames == 0 || mask->channelCount == 0) return;

        uint64_t now = GetTimeMS();
        uint32_t frameIdx = static_cast<uint32_t>(((now - startMs) / mask->stepMs) % mask->numFrames);
        const uint8_t* maskFrame = mask->frames.data() + static_cast<size_t>(frameIdx) * mask->channelCount;

        ensureRanges();
        for (auto& [start, len] : ranges) {
            if (start >= mask->channelCount) continue;
            uint32_t end = std::min<uint32_t>(start + len, mask->channelCount);
            for (uint32_t ch = start; ch < end; ++ch) {
                seqData[ch] = static_cast<uint8_t>(
                    (static_cast<uint16_t>(seqData[ch]) * static_cast<uint16_t>(maskFrame[ch])) / 255);
            }
        }
    }

    void registerApis() override {
        auto handler = [this](const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            handleRequest(req, std::move(cb));
        };
        drogon::app().registerHandler("/Mask", handler, {drogon::Get, drogon::Post});
        drogon::app().registerHandlerViaRegex("/Mask/.*", handler, {drogon::Get, drogon::Post});

        Events::AddCallback("/Mask/Set", [this](const std::string&, const std::string& payload) {
            setEnabled(parseBool(payload));
        });
        Events::AddCallback("/Mask/Toggle", [this](const std::string&, const std::string&) {
            setEnabled(!enabled.load());
        });
        Events::AddCallback("/Mask/Load", [this](const std::string&, const std::string& payload) {
            if (loadMask(payload)) saveConfig();
        });
    }

    void unregisterApis() override {}

    void multiSyncData(const uint8_t* data, int len) override {
        if (len <= 0) return;
        std::string s(reinterpret_cast<const char*>(data), len);
        if (!s.empty() && s.back() == '\0') s.pop_back();
        if (s == "on") setEnabled(true, false);
        else if (s == "off") setEnabled(false, false);
        else if (s.rfind("load:", 0) == 0) {
            if (loadMask(s.substr(5))) saveConfig();
        }
    }

    void settingChanged(const std::string& key, const std::string& /*value*/) override {
        if (key == "MaskFile") {
            auto it = settings.find("MaskFile");
            if (it != settings.end() && !it->second.empty() && it->second != currentMaskFile) {
                if (loadMask(it->second)) saveConfig();
            }
        }
        std::lock_guard<std::mutex> g(rangesMutex);
        ranges.clear();
    }

private:
    static bool parseBool(const std::string& s) {
        return s == "1" || s == "on" || s == "true" || s == "ON" || s == "True";
    }

    void ensureRanges() {
        std::lock_guard<std::mutex> g(rangesMutex);
        if (!ranges.empty()) return;
        for (auto& r : GetOutputRanges()) {
            ranges.emplace_back(r.first, r.second);
        }
    }

    void setEnabled(bool v, bool propagate = true) {
        bool was = enabled.exchange(v);
        if (was == v) return;
        if (propagate && multiSync) {
            std::string msg = v ? "on" : "off";
            multiSync->SendPluginData(name, reinterpret_cast<uint8_t*>(msg.data()), msg.size());
        }
        saveConfig();
        LogInfo(VB_PLUGIN, "fpp-mask: %s\n", v ? "enabled" : "disabled");
    }

    bool loadMask(const std::string& filename) {
        std::string fullPath = filename;
        if (!filename.empty() && filename.front() != '/') {
            fullPath = FPP_DIR_SEQUENCE("/" + filename);
        }
        std::unique_ptr<FSEQFile> f{FSEQFile::openFSEQFile(fullPath)};
        if (!f) {
            LogErr(VB_PLUGIN, "fpp-mask: failed to open %s\n", fullPath.c_str());
            return false;
        }

        auto md = std::make_shared<MaskData>();
        md->path = fullPath;
        md->numFrames = f->getNumFrames();
        md->channelCount = f->getChannelCount();
        md->stepMs = f->getStepTime() ? f->getStepTime() : 50;

        std::vector<std::pair<uint32_t, uint32_t>> r{{0, md->channelCount}};
        f->prepareRead(r, 0);

        md->frames.assign(static_cast<size_t>(md->numFrames) * md->channelCount, 255);
        for (uint32_t i = 0; i < md->numFrames; ++i) {
            std::unique_ptr<FSEQFile::FrameData> fd{f->getFrame(i)};
            if (!fd) continue;
            fd->readFrame(md->frames.data() + static_cast<size_t>(i) * md->channelCount,
                          md->channelCount);
        }

        currentMask.store(md);
        currentMaskFile = filename;
        startMs = GetTimeMS();
        LogInfo(VB_PLUGIN, "fpp-mask: loaded %s (%u frames, %u channels, %ums step)\n",
                fullPath.c_str(), md->numFrames, md->channelCount, md->stepMs);
        return true;
    }

    void handleRequest(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        std::string path = req->getPath();
        std::string action;
        if (path.size() > 5) action = path.substr(6);

        Json::Value resp;
        if (action == "on" || action == "enable") {
            setEnabled(true);
        } else if (action == "off" || action == "disable") {
            setEnabled(false);
        } else if (action == "toggle") {
            setEnabled(!enabled.load());
        } else if (action.rfind("load/", 0) == 0) {
            std::string fn = action.substr(5);
            resp["loaded"] = loadMask(fn);
            if (resp["loaded"].asBool()) saveConfig();
        }

        auto m = currentMask.load();
        resp["enabled"] = enabled.load();
        resp["maskFile"] = currentMaskFile;
        if (m) {
            resp["frames"] = m->numFrames;
            resp["channels"] = m->channelCount;
            resp["stepMs"] = m->stepMs;
        }

        Json::StreamWriterBuilder w;
        auto r = drogon::HttpResponse::newHttpResponse();
        r->setBody(Json::writeString(w, resp));
        r->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        cb(r);
    }

    void loadConfig() {
        if (!FileExists(configPath)) return;
        Json::Value root;
        if (!LoadJsonFromFile(configPath, root)) return;
        if (root.isMember("enabled")) enabled.store(root["enabled"].asBool());
        if (root.isMember("maskFile")) currentMaskFile = root["maskFile"].asString();
    }

    void saveConfig() {
        Json::Value root;
        root["enabled"] = enabled.load();
        root["maskFile"] = currentMaskFile;
        Json::StreamWriterBuilder w;
        std::ofstream f(configPath);
        if (f) f << Json::writeString(w, root);
    }

    std::string configPath;
    std::string currentMaskFile;
    std::atomic<bool> enabled{false};
    std::atomic<std::shared_ptr<MaskData>> currentMask;
    uint64_t startMs = 0;
    std::vector<std::pair<uint32_t, uint32_t>> ranges;
    std::mutex rangesMutex;
};

extern "C" {
    FPPPlugins::Plugin* createPlugin() { return new FPPMaskPlugin(); }
}

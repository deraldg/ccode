#pragma once

#include <atomic>
#include <memory>
#include <streambuf>
#include <string>
#include <thread>

namespace foxtalk {

enum class OutputMode {
    ToWindow,
    ToConsole,
    ToBoth
};

class IOutputSink {
public:
    virtual ~IOutputSink() = default;
    virtual void enqueueChunk(const std::string& s) = 0;
};

class FoxtalkCoutRedirectBuf : public std::streambuf {
public:
    FoxtalkCoutRedirectBuf(IOutputSink* sink, std::streambuf* original, OutputMode mode);

    void setMode(OutputMode m);

protected:
    int sync() override;
    int overflow(int ch) override;

private:
    void flushBuffer();

private:
    IOutputSink* sink_{nullptr};
    std::streambuf* original_{nullptr};
    OutputMode mode_{OutputMode::ToWindow};
    char buf_[64 * 1024]{};
};

class RedirectController {
public:
    explicit RedirectController(IOutputSink* sink);
    ~RedirectController();

    void install(OutputMode mode);
    void restore();
    void setMode(OutputMode mode);

private:
    void startCStdCapture();
    void stopCStdCapture();

#ifdef _WIN32
    static void readerThreadLoop(RedirectController* self, int fd);
#endif

private:
    IOutputSink* sink_{nullptr};
    OutputMode mode_{OutputMode::ToWindow};

    std::streambuf* oldCout_{nullptr};
    std::streambuf* oldCerr_{nullptr};

    std::unique_ptr<FoxtalkCoutRedirectBuf> coutBuf_;
    std::unique_ptr<FoxtalkCoutRedirectBuf> cerrBuf_;

#ifdef _WIN32
    int oldStdout_{-1};
    int oldStderr_{-1};
    int pipeOut_[2]{-1, -1};
    int pipeErr_[2]{-1, -1};
    std::thread tOut_;
    std::thread tErr_;
    std::atomic<bool> running_{false};
#endif
};

} // namespace foxtalk
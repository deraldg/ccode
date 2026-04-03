#include "tv/foxtalk_redirect.hpp"

#include <chrono>
#include <iostream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace foxtalk {

FoxtalkCoutRedirectBuf::FoxtalkCoutRedirectBuf(IOutputSink* sink,
                                               std::streambuf* original,
                                               OutputMode mode)
    : sink_(sink), original_(original), mode_(mode)
{
    setp(buf_, buf_ + sizeof(buf_) - 1);
}

void FoxtalkCoutRedirectBuf::setMode(OutputMode m)
{
    mode_ = m;
}

int FoxtalkCoutRedirectBuf::sync()
{
    flushBuffer();
    return 0;
}

int FoxtalkCoutRedirectBuf::overflow(int ch)
{
    if (ch != EOF) {
        *pptr() = static_cast<char>(ch);
        pbump(1);
    }

    if (ch == '\n') {
        flushBuffer();
        return ch;
    }

    if (pptr() >= epptr())
        flushBuffer();

    return ch;
}

void FoxtalkCoutRedirectBuf::flushBuffer()
{
    if (pptr() == pbase())
        return;

    const std::string s(pbase(), pptr());
    pbump(-static_cast<int>(pptr() - pbase()));

    if ((mode_ == OutputMode::ToWindow || mode_ == OutputMode::ToBoth) && sink_)
        sink_->enqueueChunk(s);

    if ((mode_ == OutputMode::ToConsole || mode_ == OutputMode::ToBoth) && original_) {
        original_->sputn(s.data(), static_cast<std::streamsize>(s.size()));
        original_->pubsync();
    }
}

RedirectController::RedirectController(IOutputSink* sink)
    : sink_(sink)
{
}

RedirectController::~RedirectController()
{
    restore();
}

void RedirectController::install(OutputMode mode)
{
    mode_ = mode;

    oldCout_ = std::cout.rdbuf();
    oldCerr_ = std::cerr.rdbuf();

    coutBuf_ = std::make_unique<FoxtalkCoutRedirectBuf>(sink_, oldCout_, mode_);
    cerrBuf_ = std::make_unique<FoxtalkCoutRedirectBuf>(sink_, oldCerr_, mode_);

    std::cout.rdbuf(static_cast<std::streambuf*>(coutBuf_.get()));
    std::cerr.rdbuf(static_cast<std::streambuf*>(cerrBuf_.get()));

#ifdef _WIN32
    startCStdCapture();
#endif
}

void RedirectController::restore()
{
#ifdef _WIN32
    stopCStdCapture();
#endif

    if (oldCout_) {
        std::cout.rdbuf(oldCout_);
        oldCout_ = nullptr;
    }

    if (oldCerr_) {
        std::cerr.rdbuf(oldCerr_);
        oldCerr_ = nullptr;
    }

    coutBuf_.reset();
    cerrBuf_.reset();
}

void RedirectController::setMode(OutputMode mode)
{
    mode_ = mode;

    if (coutBuf_)
        coutBuf_->setMode(mode_);

    if (cerrBuf_)
        cerrBuf_->setMode(mode_);
}

#ifdef _WIN32

void RedirectController::readerThreadLoop(RedirectController* self, int fd)
{
    char buf[64 * 1024];

    while (self->running_.load(std::memory_order_relaxed)) {
        const int n = _read(fd, buf, static_cast<unsigned>(sizeof(buf)));
        if (n <= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
            continue;
        }

        if (self->sink_)
            self->sink_->enqueueChunk(std::string(buf, buf + n));
    }
}

void RedirectController::startCStdCapture()
{
    running_.store(true, std::memory_order_relaxed);

    oldStdout_ = _dup(1);
    oldStderr_ = _dup(2);

    if (_pipe(pipeOut_, 64 * 1024, _O_TEXT) == 0) {
        _dup2(pipeOut_[1], 1);
        tOut_ = std::thread(&RedirectController::readerThreadLoop, this, pipeOut_[0]);
    }

    if (_pipe(pipeErr_, 64 * 1024, _O_TEXT) == 0) {
        _dup2(pipeErr_[1], 2);
        tErr_ = std::thread(&RedirectController::readerThreadLoop, this, pipeErr_[0]);
    }
}

void RedirectController::stopCStdCapture()
{
    running_.store(false, std::memory_order_relaxed);

    if (oldStdout_ != -1) {
        _dup2(oldStdout_, 1);
        _close(oldStdout_);
        oldStdout_ = -1;
    }

    if (oldStderr_ != -1) {
        _dup2(oldStderr_, 2);
        _close(oldStderr_);
        oldStderr_ = -1;
    }

    if (pipeOut_[1] != -1) {
        _close(pipeOut_[1]);
        pipeOut_[1] = -1;
    }

    if (pipeErr_[1] != -1) {
        _close(pipeErr_[1]);
        pipeErr_[1] = -1;
    }

    if (tOut_.joinable())
        tOut_.join();

    if (tErr_.joinable())
        tErr_.join();

    if (pipeOut_[0] != -1) {
        _close(pipeOut_[0]);
        pipeOut_[0] = -1;
    }

    if (pipeErr_[0] != -1) {
        _close(pipeErr_[0]);
        pipeErr_[0] = -1;
    }
}

#else

void RedirectController::startCStdCapture()
{
}

void RedirectController::stopCStdCapture()
{
}

#endif

} // namespace foxtalk
#pragma once
#include <cstdint>

namespace Magpie::FrameTrace {

// CPU wall times only. Nested events overlap; none is a GPU execution timer.
enum class Event : uint16_t {
    BackendWait, BackendMessages, CaptureUpdate, CaptureResult, WgcAcquire,
    WgcFrame, WgcRejected, WgcClose, WgcStart, DuplicateCheck, DuplicateReadback,
    BackendRender, Guidance, NvofSubmit, NativeEffect, ReferencePrepare,
    ReferencePublish, ReferenceConsume, ReferencePresented, Publication,
    PublicationTransaction, PublicationFence, FrontendTick, FrontendPrepare,
    CursorUpdate, FocusProbe, HitTestRequest, HitTestComplete, FrontendAcquire,
    FrontendAcquireBusy, FrontendBase, FrontendDraw, BeginFrame, CapacityBusy,
    Present, PresentGap, ContentSubmit, OverlaySubmit, DcompCommit, PresentGpuWait,
    RenderDecision, CaptureAccepted, FrontendMessage, WgcNotificationWait,
    WgcDequeue, CaptureWake, OverlayDeferred, Count
};

#ifdef MP_ENABLE_FRAME_TRACE
bool Start() noexcept; // Frontend, before starting the backend thread.
void BindBackend() noexcept;
void Stop() noexcept;  // Frontend, AFTER joining the backend thread.
bool Enabled() noexcept;
int64_t Tick() noexcept;
void SetFrame(uint64_t frameId) noexcept;
uint64_t Frame() noexcept;
void Record(Event event, int64_t start, int64_t end, uint64_t frameId,
    int64_t a = 0, int64_t b = 0) noexcept;
void Mark(Event event, int64_t a = 0, int64_t b = 0) noexcept;
void Presentation(int64_t start, int64_t end, int64_t result, uintptr_t swapchain) noexcept;
#else
inline bool Start() noexcept { return false; }
inline void BindBackend() noexcept {}
inline void Stop() noexcept {}
inline bool Enabled() noexcept { return false; }
inline int64_t Tick() noexcept { return 0; }
inline void SetFrame(uint64_t) noexcept {}
inline uint64_t Frame() noexcept { return 0; }
inline void Record(Event, int64_t, int64_t, uint64_t, int64_t = 0, int64_t = 0) noexcept {}
inline void Mark(Event, int64_t = 0, int64_t = 0) noexcept {}
inline void Presentation(int64_t, int64_t, int64_t, uintptr_t) noexcept {}
#endif

class Scope {
public:
    explicit Scope(Event event, int64_t a = 0, int64_t b = 0) noexcept
        : _event(event), _start(Enabled() ? Tick() : 0), _frame(Frame()), _a(a), _b(b) {}
    ~Scope() noexcept { End(); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    void Data(int64_t a, int64_t b = 0) noexcept { _a = a; _b = b; }
    void FrameId(uint64_t frame) noexcept { _frame = frame; }
    void End() noexcept {
        if (!_start) return;
        Record(_event, _start, Tick(), _frame, _a, _b);
        _start = 0;
    }
private:
    Event _event;
    int64_t _start;
    uint64_t _frame;
    int64_t _a, _b;
};

}

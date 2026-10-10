#pragma once

#include <chrono>
#include <cstdint>

#include <jni.h>

#include <Carbon/Carbon.h>

#include "AndroidDevice.h"
#include "AndroidText.h"
#include "GalleryState.h"

struct android_app;
struct GameActivityMotionEvent;
struct GameActivityKeyEvent;

namespace AndroidGallery
{
    /// The host of the Android Gallery: runs on GameActivity's native thread, owns the Carbon context and the
    /// device, and forwards everything Android reports to Carbon each frame: touches, pens and mice, keys, the
    /// safe area (system bars and the display cutout), the text size, the soft keyboard's area and text, and the
    /// system's dark appearance. It renders while the app is visible and has a surface, and waits otherwise.
    class AndroidHost
    {
    public:
        explicit AndroidHost(android_app* app);
        ~AndroidHost();

        AndroidHost(const AndroidHost&) = delete;
        AndroidHost& operator=(const AndroidHost&) = delete;

        /// Runs until Android destroys the activity.
        void Run();
        /// Handles one of the glue's APP_CMD_* commands.
        void HandleCommand(int32_t command);
        /// Called on the UI thread when Back was pressed while the host handles it (MainActivity).
        static void RequestBack();

    private:
        void StartSurface();
        void CreateGraphics();
        void DestroyGraphics();
        void RunFrame();
        void UpdateMetrics();
        void ForwardInput();
        void ForwardMotion(const GameActivityMotionEvent& event);
        void ForwardKey(const GameActivityKeyEvent& event);
        void FollowSystemAppearance();
        void CallActivity(jmethodID method, bool value);

    private:
        android_app* m_App = nullptr;
        AndroidDevice m_Device;
        AndroidText m_Text;
        Carbon::Context* m_Context = nullptr;
        Gallery::GalleryState m_State;
        bool m_IsBackendReady = false;
        bool m_IsVisible = false;
        float m_ContentScale = 1.0f;
        std::chrono::steady_clock::time_point m_LastFrame;
        bool m_HasLastFrame = false;
        int32_t m_MouseButtons = 0;
        bool m_HasEditorAction = false;
        bool m_IsSystemDark = false;
        bool m_AreSystemBarsLight = false;
        bool m_HasSystemBarsAppearance = false;
        bool m_IsBackHandled = false;
        /// A frame is due now: something happened, or the last frame was still animating.
        bool m_NeedsFrame = true;

        JNIEnv* m_Env = nullptr;
        jmethodID m_SetLightSystemBars = nullptr;
        jmethodID m_SetBackHandled = nullptr;
        jmethodID m_GetClipboardText = nullptr;
        jmethodID m_SetClipboardText = nullptr;
    };
} // namespace AndroidGallery

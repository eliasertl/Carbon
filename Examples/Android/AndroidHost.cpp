#include "AndroidHost.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <string>

#include <android/keycodes.h>
#include <android/log.h>
#include <android/looper.h>
#include <game-activity/GameActivity.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include "Gallery.h"
#include "SystemFonts.h"

namespace AndroidGallery
{
    namespace
    {
        constexpr const char* LogTag = "CarbonGallery";
        // Android's baseline density: one point is one pixel at 160 dots per inch.
        constexpr float BaselineDensity = 160.0f;
        // The longest step animations take after the app was paused, so that nothing jumps.
        constexpr float MaxDeltaTime = 0.1f;
        // How long the host sleeps at most while the soft keyboard is shown: its text arrives without a command
        // that would wake the loop.
        constexpr float KeyboardPollInterval = 0.05f;

        std::atomic<bool> s_IsBackRequested = false;
        // The native thread's looper, which Back wakes from the UI thread.
        std::atomic<ALooper*> s_Looper = nullptr;

        int ToAndroidPriority(Carbon::LogLevel level)
        {
            switch (level)
            {
                case Carbon::LogLevel::Trace:
                    return ANDROID_LOG_VERBOSE;
                case Carbon::LogLevel::Debug:
                    return ANDROID_LOG_DEBUG;
                case Carbon::LogLevel::Info:
                    return ANDROID_LOG_INFO;
                case Carbon::LogLevel::Warning:
                    return ANDROID_LOG_WARN;
                case Carbon::LogLevel::Error:
                    return ANDROID_LOG_ERROR;
                case Carbon::LogLevel::Fatal:
                    return ANDROID_LOG_FATAL;
            }
            return ANDROID_LOG_INFO;
        }

        Carbon::Key ToCarbonKey(int32_t keyCode)
        {
            if (keyCode >= AKEYCODE_A && keyCode <= AKEYCODE_Z)
                return static_cast<Carbon::Key>(static_cast<int>(Carbon::Key::A) + (keyCode - AKEYCODE_A));
            if (keyCode >= AKEYCODE_0 && keyCode <= AKEYCODE_9)
                return static_cast<Carbon::Key>(static_cast<int>(Carbon::Key::D0) + (keyCode - AKEYCODE_0));
            switch (keyCode)
            {
                case AKEYCODE_TAB:
                    return Carbon::Key::Tab;
                case AKEYCODE_DPAD_LEFT:
                    return Carbon::Key::LeftArrow;
                case AKEYCODE_DPAD_RIGHT:
                    return Carbon::Key::RightArrow;
                case AKEYCODE_DPAD_UP:
                    return Carbon::Key::UpArrow;
                case AKEYCODE_DPAD_DOWN:
                    return Carbon::Key::DownArrow;
                case AKEYCODE_PAGE_UP:
                    return Carbon::Key::PageUp;
                case AKEYCODE_PAGE_DOWN:
                    return Carbon::Key::PageDown;
                case AKEYCODE_MOVE_HOME:
                    return Carbon::Key::Home;
                case AKEYCODE_MOVE_END:
                    return Carbon::Key::End;
                case AKEYCODE_INSERT:
                    return Carbon::Key::Insert;
                case AKEYCODE_FORWARD_DEL:
                    return Carbon::Key::Delete;
                case AKEYCODE_DEL:
                    return Carbon::Key::Backspace;
                case AKEYCODE_SPACE:
                    return Carbon::Key::Space;
                case AKEYCODE_ENTER:
                case AKEYCODE_NUMPAD_ENTER:
                    return Carbon::Key::Enter;
                case AKEYCODE_ESCAPE:
                    return Carbon::Key::Escape;
                case AKEYCODE_MENU:
                    return Carbon::Key::Menu;
                case AKEYCODE_CTRL_LEFT:
                    return Carbon::Key::LeftCtrl;
                case AKEYCODE_SHIFT_LEFT:
                    return Carbon::Key::LeftShift;
                case AKEYCODE_ALT_LEFT:
                    return Carbon::Key::LeftAlt;
                case AKEYCODE_META_LEFT:
                    return Carbon::Key::LeftSuper;
                case AKEYCODE_CTRL_RIGHT:
                    return Carbon::Key::RightCtrl;
                case AKEYCODE_SHIFT_RIGHT:
                    return Carbon::Key::RightShift;
                case AKEYCODE_ALT_RIGHT:
                    return Carbon::Key::RightAlt;
                case AKEYCODE_META_RIGHT:
                    return Carbon::Key::RightSuper;
                default:
                    return Carbon::Key::None;
            }
        }

        // Keys the system keeps: the volume, the camera, and Back, which goes through the activity's back
        // dispatcher (predictive back) rather than to the native code.
        bool AcceptKeyEvent(const GameActivityKeyEvent* event)
        {
            switch (event->keyCode)
            {
                case AKEYCODE_VOLUME_UP:
                case AKEYCODE_VOLUME_DOWN:
                case AKEYCODE_VOLUME_MUTE:
                case AKEYCODE_CAMERA:
                case AKEYCODE_FOCUS:
                case AKEYCODE_BACK:
                    return false;
                default:
                    return true;
            }
        }

        // Fingers, pens and mice; controllers and other sources are not used.
        bool AcceptMotionEvent(const GameActivityMotionEvent* event)
        {
            return (event->source & AINPUT_SOURCE_CLASS_POINTER) != 0;
        }

        Carbon::PointerType ToPointerType(int32_t toolType)
        {
            return toolType == AMOTION_EVENT_TOOL_TYPE_STYLUS || toolType == AMOTION_EVENT_TOOL_TYPE_ERASER
                       ? Carbon::PointerType::Pen
                       : Carbon::PointerType::Touch;
        }

        // Configuration.uiMode as GameActivity keeps it up to date (the glue's AConfiguration is not refreshed
        // when the configuration changes).
        bool IsSystemDark(const android_app* app)
        {
            constexpr int UiModeNightMask = 0x30;
            constexpr int UiModeNightYes = 0x20;
            return (GameActivity_getUIMode(app->activity) & UiModeNightMask) == UiModeNightYes;
        }

        Carbon::EdgeInsets GetInsets(GameActivity* activity, GameCommonInsetsType type, float scale)
        {
            ARect rect = {};
            GameActivity_getWindowInsets(activity, type, &rect);
            return Carbon::EdgeInsets(static_cast<float>(rect.top) / scale, static_cast<float>(rect.right) / scale,
                                      static_cast<float>(rect.bottom) / scale, static_cast<float>(rect.left) / scale);
        }
    } // namespace

    AndroidHost::AndroidHost(android_app* app) : m_App(app)
    {
        m_App->userData = this;
        s_Looper = m_App->looper;
        m_App->onAppCmd = [](android_app* source, int32_t command)
        { static_cast<AndroidHost*>(source->userData)->HandleCommand(command); };
        android_app_set_key_event_filter(m_App, AcceptKeyEvent);
        android_app_set_motion_event_filter(m_App, AcceptMotionEvent);
        // Mouse wheels and touchpads scroll through these axes, which GameActivity reports only when asked.
        GameActivityPointerAxes_enableAxis(AMOTION_EVENT_AXIS_VSCROLL);
        GameActivityPointerAxes_enableAxis(AMOTION_EVENT_AXIS_HSCROLL);

        // The activity's Java methods, called from this thread.
        m_App->activity->vm->AttachCurrentThread(&m_Env, nullptr);
        jclass activityClass = m_Env->GetObjectClass(m_App->activity->javaGameActivity);
        m_SetLightSystemBars = m_Env->GetMethodID(activityClass, "setLightSystemBars", "(Z)V");
        m_SetBackHandled = m_Env->GetMethodID(activityClass, "setBackHandled", "(Z)V");
        m_GetClipboardText = m_Env->GetMethodID(activityClass, "getClipboardText", "()Ljava/lang/String;");
        m_SetClipboardText = m_Env->GetMethodID(activityClass, "setClipboardText", "(Ljava/lang/String;)V");
        m_Env->DeleteLocalRef(activityClass);

        Carbon::ContextDescription description;
        description.Callbacks.Log = [](Carbon::LogLevel level, std::string_view source, std::string_view message)
        {
            __android_log_print(ToAndroidPriority(level), LogTag, "%.*s: %.*s", static_cast<int>(source.size()),
                                source.data(), static_cast<int>(message.size()), message.data());
        };
        description.Callbacks.AssertFailed = [](const Carbon::AssertInfo&) {};
        description.Callbacks.SetKeyboardVisible = [this](bool visible) { m_Text.RequestKeyboard(visible); };
        description.Callbacks.GetClipboardText = [this]() -> std::string
        {
            std::string text;
            auto* value =
                static_cast<jstring>(m_Env->CallObjectMethod(m_App->activity->javaGameActivity, m_GetClipboardText));
            if (value == nullptr)
                return text;
            const char* modified = m_Env->GetStringUTFChars(value, nullptr);
            ModifiedUTF8ToUTF8(modified != nullptr ? modified : "", text);
            m_Env->ReleaseStringUTFChars(value, modified);
            m_Env->DeleteLocalRef(value);
            return text;
        };
        description.Callbacks.SetClipboardText = [this](std::string_view text)
        {
            std::string modified;
            UTF8ToModifiedUTF8(text, modified);
            jstring value = m_Env->NewStringUTF(modified.c_str());
            m_Env->CallVoidMethod(m_App->activity->javaGameActivity, m_SetClipboardText, value);
            m_Env->DeleteLocalRef(value);
        };
        m_Context = Carbon::CreateContext(description);

        Carbon::IO& io = Carbon::GetIO();
        // A phone or a tablet: touch mode from the first frame, not from the first tap.
        io.SetDefaultPointerType(Carbon::PointerType::Touch);
        // The system's fonts for what Carbon's embedded fonts lack: emoji, Japanese, Chinese and Korean.
        m_State.HasEmojiFont = Example::AddSystemFallbackFonts().HasEmoji;

        // The Gallery starts in the system's appearance and follows it when it changes.
        m_IsSystemDark = IsSystemDark(m_App);
        m_State.IsDark = m_IsSystemDark;
        Carbon::SetTheme(m_State.IsDark ? Carbon::Theme::Dark() : Carbon::Theme::Light());
    }

    AndroidHost::~AndroidHost()
    {
        DestroyGraphics();
        if (m_Context != nullptr)
            Carbon::DestroyContext(m_Context);
        m_App->activity->vm->DetachCurrentThread();
        s_Looper = nullptr;
        m_App->userData = nullptr;
        m_App->onAppCmd = nullptr;
    }

    void AndroidHost::RequestBack()
    {
        s_IsBackRequested = true;
        if (ALooper* looper = s_Looper.load())
            ALooper_wake(looper);
    }

    void AndroidHost::Run()
    {
        while (m_App->destroyRequested == 0)
        {
            // Frames are rendered on demand: when something happened (a touch, a key, a command), while Carbon
            // animates, and when Carbon asks for one later (a caret that blinks). In between the thread sleeps.
            // Without a surface or while invisible there is nothing to draw: it waits for the next command.
            const bool canRender = m_IsVisible && m_Device.HasSurface();
            int timeout = -1;
            if (canRender && m_NeedsFrame)
                timeout = 0;
            else if (canRender)
            {
                float delay = m_IsBackendReady ? Carbon::GetNextFrameDelay() : 0.0f;
                if (m_Text.IsKeyboardShown())
                    delay = std::min(delay, KeyboardPollInterval);
                if (delay < 3600.0f)
                    timeout = static_cast<int>(std::ceil(delay * 1000.0f));
            }
            android_poll_source* source = nullptr;
            const int result = ALooper_pollOnce(timeout, nullptr, nullptr, reinterpret_cast<void**>(&source));
            if (result == ALOOPER_POLL_ERROR)
                break;
            if (source != nullptr || result == ALOOPER_POLL_WAKE)
                m_NeedsFrame = true;
            if (source != nullptr)
                source->process(m_App, source);
            // Handle everything that is pending before the next frame.
            if (result != ALOOPER_POLL_TIMEOUT)
                continue;
            if (canRender)
                RunFrame();
        }
    }

    void AndroidHost::HandleCommand(int32_t command)
    {
        switch (command)
        {
            case APP_CMD_INIT_WINDOW:
                StartSurface();
                break;
            case APP_CMD_TERM_WINDOW:
                // The surface goes; the context and with it Carbon's backend and the textures stay.
                m_Device.DestroySurface();
                break;
            case APP_CMD_START:
                m_IsVisible = true;
                m_HasLastFrame = false;
                break;
            case APP_CMD_STOP:
                m_IsVisible = false;
                break;
            case APP_CMD_GAINED_FOCUS:
            case APP_CMD_LOST_FOCUS:
                if (m_Context != nullptr)
                    Carbon::GetIO().AddFocusEvent(command == APP_CMD_GAINED_FOCUS);
                break;
            case APP_CMD_CONFIG_CHANGED:
                FollowSystemAppearance();
                break;
            case APP_CMD_EDITOR_ACTION:
                // The keyboard's action key (Done, Search, Go) acts like Return.
                m_HasEditorAction = true;
                break;
            case APP_CMD_DESTROY:
                DestroyGraphics();
                break;
            default:
                break;
        }
    }

    void AndroidHost::StartSurface()
    {
        if (m_App->window == nullptr)
            return;
        bool isNewContext = false;
        if (!m_Device.CreateSurface(m_App->window, isNewContext))
            return;
        if (isNewContext)
            CreateGraphics();
    }

    void AndroidHost::CreateGraphics()
    {
        m_IsBackendReady = m_Device.InitCarbon();
        if (!m_IsBackendReady)
        {
            __android_log_print(ANDROID_LOG_ERROR, LogTag, "Carbon's OpenGL ES backend could not be installed");
            return;
        }
        // The Images page's texture belongs to the context, so it is made again with every new one.
        m_State.Artwork = Gallery::CreateArtwork(m_Device);
    }

    void AndroidHost::DestroyGraphics()
    {
        if (m_IsBackendReady)
            m_Device.ShutdownCarbon();
        m_IsBackendReady = false;
        m_State.Artwork = {};
        m_Device.DestroyContext();
    }

    void AndroidHost::FollowSystemAppearance()
    {
        const bool isDark = IsSystemDark(m_App);
        if (isDark == m_IsSystemDark)
            return;
        m_IsSystemDark = isDark;
        m_State.IsDark = isDark;
        Carbon::SetTheme(isDark ? Carbon::Theme::Dark() : Carbon::Theme::Light());
    }

    void AndroidHost::CallActivity(jmethodID method, bool value)
    {
        if (method != nullptr)
            m_Env->CallVoidMethod(m_App->activity->javaGameActivity, method, static_cast<jboolean>(value));
    }

    void AndroidHost::UpdateMetrics()
    {
        Carbon::IO& io = Carbon::GetIO();
        uint32_t width = 0;
        uint32_t height = 0;
        m_Device.GetSurfaceSize(width, height);
        // The density follows the system's display size setting.
        const int density = GameActivity_getDensityDpi(m_App->activity);
        m_ContentScale = density > 0 ? static_cast<float>(density) / BaselineDensity : 1.0f;
        const Carbon::Vec2 display(static_cast<float>(width) / m_ContentScale,
                                   static_cast<float>(height) / m_ContentScale);
        io.SetDisplaySize(display.X, display.Y);
        io.SetContentScale(m_ContentScale);

        const auto now = std::chrono::steady_clock::now();
        const float deltaTime = m_HasLastFrame ? std::chrono::duration<float>(now - m_LastFrame).count() : 1.0f / 60.0f;
        m_LastFrame = now;
        m_HasLastFrame = true;
        io.SetDeltaTime(std::clamp(deltaTime, 0.0001f, MaxDeltaTime));

        // The safe area: the system bars and the display cutout, whichever reaches further in from each edge.
        GameActivity* activity = m_App->activity;
        const Carbon::EdgeInsets bars = GetInsets(activity, GAMECOMMON_INSETS_TYPE_SYSTEM_BARS, m_ContentScale);
        const Carbon::EdgeInsets cutout = GetInsets(activity, GAMECOMMON_INSETS_TYPE_DISPLAY_CUTOUT, m_ContentScale);
        io.SetSafeAreaInsets(Carbon::EdgeInsets(std::max(bars.Top, cutout.Top), std::max(bars.Right, cutout.Right),
                                                std::max(bars.Bottom, cutout.Bottom),
                                                std::max(bars.Left, cutout.Left)));
        // The text size of the system's settings ("Font size").
        io.SetTextScale(GameActivity_getFontScale(activity));
        // The soft keyboard covers the bottom of the display, up to its inset.
        const float keyboard =
            std::min(GetInsets(activity, GAMECOMMON_INSETS_TYPE_IME, m_ContentScale).Bottom, display.Y);
        io.SetKeyboardRect(keyboard > 0.0f ? Carbon::Rect(0.0f, display.Y - keyboard, display.X, keyboard)
                                           : Carbon::Rect());
    }

    void AndroidHost::ForwardMotion(const GameActivityMotionEvent& event)
    {
        Carbon::IO& io = Carbon::GetIO();
        const int32_t action = event.action & AMOTION_EVENT_ACTION_MASK;
        const auto index = static_cast<uint32_t>((event.action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                                                 AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
        if (event.pointerCount == 0)
            return;
        const auto pointX = [this](const GameActivityPointerAxes& pointer)
        { return GameActivityPointerAxes_getX(&pointer) / m_ContentScale; };
        const auto pointY = [this](const GameActivityPointerAxes& pointer)
        { return GameActivityPointerAxes_getY(&pointer) / m_ContentScale; };

        // A mouse (or a touchpad's cursor): positions, buttons and the wheel, as on a desktop.
        const GameActivityPointerAxes& first = event.pointers[0];
        if (GameActivityPointerAxes_getToolType(&first) == AMOTION_EVENT_TOOL_TYPE_MOUSE)
        {
            io.AddMousePosEvent(pointX(first), pointY(first));
            if (action == AMOTION_EVENT_ACTION_SCROLL)
            {
                io.AddMouseWheelEvent(-GameActivityPointerAxes_getAxisValue(&first, AMOTION_EVENT_AXIS_HSCROLL),
                                      -GameActivityPointerAxes_getAxisValue(&first, AMOTION_EVENT_AXIS_VSCROLL));
                return;
            }
            const int32_t buttons = action == AMOTION_EVENT_ACTION_HOVER_EXIT ? m_MouseButtons : event.buttonState;
            constexpr std::pair<int32_t, Carbon::MouseButton> Buttons[] = {
                {AMOTION_EVENT_BUTTON_PRIMARY, Carbon::MouseButton::Left},
                {AMOTION_EVENT_BUTTON_SECONDARY, Carbon::MouseButton::Right},
                {AMOTION_EVENT_BUTTON_TERTIARY, Carbon::MouseButton::Middle},
            };
            for (const auto& [mask, button] : Buttons)
                if ((buttons & mask) != (m_MouseButtons & mask))
                    io.AddMouseButtonEvent(button, (buttons & mask) != 0);
            m_MouseButtons = buttons;
            if (action == AMOTION_EVENT_ACTION_HOVER_EXIT)
                io.AddMouseLeaveEvent();
            return;
        }

        // Fingers and pens. A finger lifted inside a text control that is being edited brings the keyboard back
        // after the user dismissed it.
        const auto send = [&](Carbon::TouchPhase phase, const GameActivityPointerAxes& pointer)
        {
            io.AddTouchEvent(phase, static_cast<uint64_t>(pointer.id), pointX(pointer), pointY(pointer),
                             ToPointerType(GameActivityPointerAxes_getToolType(&pointer)));
            if (phase == Carbon::TouchPhase::Ended && io.WantsTextInput() &&
                !GameActivity_isSoftwareKeyboardVisible(m_App->activity))
            {
                const Carbon::Vec2 point(pointX(pointer), pointY(pointer));
                for (const Carbon::Rect& area : io.GetTextInputAreas())
                    if (area.Contains(point))
                        m_Text.RequestKeyboard(true);
            }
        };
        switch (action)
        {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                if (index < event.pointerCount)
                    send(Carbon::TouchPhase::Began, event.pointers[index]);
                break;
            case AMOTION_EVENT_ACTION_MOVE:
                for (uint32_t i = 0; i < event.pointerCount; i++)
                    send(Carbon::TouchPhase::Moved, event.pointers[i]);
                break;
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                if (index < event.pointerCount)
                    send(Carbon::TouchPhase::Ended, event.pointers[index]);
                break;
            case AMOTION_EVENT_ACTION_CANCEL:
                for (uint32_t i = 0; i < event.pointerCount; i++)
                    send(Carbon::TouchPhase::Cancelled, event.pointers[i]);
                break;
            default:
                break;
        }
    }

    void AndroidHost::ForwardKey(const GameActivityKeyEvent& event)
    {
        if (event.action != AKEY_EVENT_ACTION_DOWN && event.action != AKEY_EVENT_ACTION_UP)
            return;
        Carbon::IO& io = Carbon::GetIO();
        const bool isDown = event.action == AKEY_EVENT_ACTION_DOWN;
        const Carbon::Key key = ToCarbonKey(event.keyCode);
        if (key != Carbon::Key::None)
            io.AddKeyEvent(key, isDown);
        // Typed characters of a hardware keyboard (and of adb's "input text"). Shortcuts type nothing.
        const bool isShortcut = (event.metaState & (AMETA_CTRL_ON | AMETA_META_ON)) != 0;
        if (isDown && !isShortcut && event.unicodeChar >= 0x20 && event.unicodeChar != 0x7F)
            io.AddInputCharacter(static_cast<char32_t>(event.unicodeChar));
    }

    void AndroidHost::ForwardInput()
    {
        if (android_input_buffer* input = android_app_swap_input_buffers(m_App))
        {
            for (uint64_t i = 0; i < input->motionEventsCount; i++)
                ForwardMotion(input->motionEvents[i]);
            android_app_clear_motion_events(input);
            for (uint64_t i = 0; i < input->keyEventsCount; i++)
                ForwardKey(input->keyEvents[i]);
            android_app_clear_key_events(input);
        }
        if (m_HasEditorAction)
        {
            Carbon::IO& io = Carbon::GetIO();
            io.AddKeyEvent(Carbon::Key::Enter, true);
            io.AddKeyEvent(Carbon::Key::Enter, false);
            m_HasEditorAction = false;
        }
    }

    void AndroidHost::RunFrame()
    {
        m_NeedsFrame = false;
        // A lost context takes Carbon's backend and the textures with it: start over with a new one.
        if (m_Device.IsContextLost())
        {
            __android_log_print(ANDROID_LOG_WARN, LogTag, "The OpenGL ES context was lost; creating a new one");
            DestroyGraphics();
            StartSurface();
            m_NeedsFrame = true;
            return;
        }
        uint32_t width = 0;
        uint32_t height = 0;
        m_Device.GetSurfaceSize(width, height);
        if (!m_IsBackendReady || !m_Device.BeginFrame(width, height))
            return;

        UpdateMetrics();
        ForwardInput();
        m_Text.ApplyKeyboardChanges(m_App->activity, m_App->textInputState != 0);
        m_App->textInputState = 0;
        // Back on a phone: from a page to the list of pages.
        if (s_IsBackRequested.exchange(false))
            Carbon::ShowNavigationDetail(Gallery::NavigationID, false);

        Carbon::NewFrame();
        Gallery::BuildGallery(m_State);
        Carbon::EndFrame();

        m_Text.Update(m_App->activity);
        m_NeedsFrame = Carbon::GetNextFrameDelay() <= 0.0f;
        m_Device.Render(Carbon::GetStyleColor(Carbon::StyleColor::Background));
        m_Device.EndFrame();

        // The system bars' icons contrast with the interface, and Back belongs to the app while a page is shown
        // over the list.
        if (!m_HasSystemBarsAppearance || m_AreSystemBarsLight != !m_State.IsDark)
        {
            m_AreSystemBarsLight = !m_State.IsDark;
            m_HasSystemBarsAppearance = true;
            CallActivity(m_SetLightSystemBars, m_AreSystemBarsLight);
        }
        const bool isBackHandled = Carbon::IsCompactWidth() && Carbon::IsNavigationDetailShown(Gallery::NavigationID);
        if (isBackHandled != m_IsBackHandled)
        {
            m_IsBackHandled = isBackHandled;
            CallActivity(m_SetBackHandled, isBackHandled);
        }
    }
} // namespace AndroidGallery

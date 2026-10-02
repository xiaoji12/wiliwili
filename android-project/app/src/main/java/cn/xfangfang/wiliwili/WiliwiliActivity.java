package cn.xfangfang.wiliwili;

import android.os.Bundle;
import android.view.KeyEvent;

import org.libsdl.app.BorealisHandler;
import org.libsdl.app.PlatformUtils;
import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLControllerManager;

/**
 * wiliwili Android entry point.
 *
 * <h2>Gamepad</h2>
 * Fully wired in borealis'
 * {@code library/borealis/library/lib/platforms/sdl/sdl_input.cpp}:
 * SDL_BUTTONS_MAPPING covers DPAD / A-B-X-Y / LB-RB / start-select, the analog
 * sticks are read with a 16383.5f deadzone, LT/RT triggers with 3276.7f, and
 * rumble goes through SDL_GameControllerRumble. No Java code needed.
 *
 * <h2>Android TV remote</h2>
 * An Android TV remote advertises SOURCE_DPAD, so SDLControllerManager treats
 * it as a joystick and translates its buttons through getButtonMask():
 * DPAD_CENTER -&gt; A, BACK -&gt; B, MENU -&gt; START, DPAD_* -&gt; DPAD_*.
 * borealis then maps those onto BUTTON_A / BUTTON_B / BUTTON_X and
 * BUTTON_NAV_*. Remotes that only advertise SOURCE_KEYBOARD take the keyboard
 * path instead, where borealis has an explicit
 * {@code // Android tv remote control} block (SDL_SCANCODE_UP/DOWN/LEFT/RIGHT
 * -&gt; BUTTON_NAV_*, MENU -&gt; BUTTON_X, AC_BACK -&gt; BUTTON_B).
 *
 * <h2>The one gap this class closes</h2>
 * Media keys are in neither the joystick button mask nor borealis' keyboard
 * mapping: SDL turns them into SDL_SCANCODE_AUDIOPLAY / AUDIOSTOP / AUDIONEXT /
 * AUDIOPREV / AUDIOREWIND / AUDIOFASTFORWARD, and sdl_input.cpp never consumes
 * those scancodes, so the remote's transport buttons are dead. We rewrite them
 * to the printable keys wiliwili binds by default (config_helper.cpp):
 * space = shortcut_video_pause, ] = shortcut_forward, [ = shortcut_rewind.
 */
public class WiliwiliActivity extends SDLActivity
{
    /**
     * Rewrites the Android key codes SDL/borealis cannot use into ones they can.
     *
     * @param keyCode  the original Android key code
     * @param deviceId the originating input device id
     * @return the key code SDL should see
     */
    private static int remapKeyCode(int keyCode, int deviceId) {
        switch (keyCode) {
            // Media keys -> default wiliwili shortcuts.
            // These key codes are absent from SDLControllerManager.getButtonMask(),
            // so they always travel the keyboard path and the rewrite is safe for
            // gamepads as well.
            case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
            case KeyEvent.KEYCODE_MEDIA_PLAY:
            case KeyEvent.KEYCODE_MEDIA_PAUSE:
                // -> SDL_SCANCODE_SPACE -> BRLS_KBD_KEY_SPACE -> shortcut_video_pause
                return KeyEvent.KEYCODE_SPACE;

            case KeyEvent.KEYCODE_MEDIA_FAST_FORWARD:
                // -> SDL_SCANCODE_RIGHTBRACKET -> shortcut_forward
                return KeyEvent.KEYCODE_RIGHT_BRACKET;

            case KeyEvent.KEYCODE_MEDIA_REWIND:
                // -> SDL_SCANCODE_LEFTBRACKET -> shortcut_rewind
                return KeyEvent.KEYCODE_LEFT_BRACKET;

            // OK button of a remote that is NOT recognised as a joystick.
            // Such a device skips getButtonMask(), so SDL maps DPAD_CENTER to
            // SDL_SCANCODE_SELECT, which borealis never consumes. Rewriting it to
            // ENTER makes it reach BUTTON_A through SDL_SCANCODE_RETURN.
            // Devices that do advertise SOURCE_DPAD are left untouched so the
            // normal joystick path (DPAD_CENTER -> A) keeps working.
            case KeyEvent.KEYCODE_DPAD_CENTER:
                if (!SDLControllerManager.isDeviceSDLJoystick(deviceId)) {
                    return KeyEvent.KEYCODE_ENTER;
                }
                return keyCode;

            default:
                return keyCode;
        }
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        final int keyCode = remapKeyCode(event.getKeyCode(), event.getDeviceId());
        if (keyCode == event.getKeyCode()) {
            return super.dispatchKeyEvent(event);
        }

        final KeyEvent remapped = new KeyEvent(
                event.getDownTime(), event.getEventTime(), event.getAction(),
                keyCode, event.getRepeatCount(), event.getMetaState(),
                event.getDeviceId(), event.getScanCode(), event.getFlags());
        return super.dispatchKeyEvent(remapped);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Receive brightness changes from borealis
        PlatformUtils.borealisHandler = new BorealisHandler();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        // borealis heavily uses static variables, the activity cannot be
        // recreated in place under SDL2, force exit instead (same as the
        // borealis demo project)
        System.exit(0);
    }

    @Override
    protected String[] getLibraries() {
        // Load SDL2, prebuilt libmpv and the wiliwili app (SDL_main)
        return new String[] {
                "SDL2",
                "mpv",
                "wiliwili"
        };
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from bbhost db5457cbd447f4ef129b58df0a78e0a8819c7033.
#define BBHOST_HAVE_SDL3 1
#define SDL_SCANCODE_ESCAPE 41
#define SDL_SCANCODE_DELETE 76
#include "compat.h"
#include "bindings.h"
#include "native_api.h"
#include "chords.h"


#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <string>

#if defined(BBHOST_HAVE_SDL3)

#endif

namespace {

// Bloodborne's pad, for the record, since every row below is one of these:
// R1 attacks, R2 is the strong attack, L1 transforms the weapon, L2 is the
// firearm (and the parry), Circle dodges and sprints, Triangle drinks a Blood
// Vial ("KG_R_U: Restore HP (Uses Blood Vial)" in the game's own help text),
// Square uses the quick item ("KG_R_L: Use item"), Cross interacts, the d-pad
// switches weapons and items, R3 locks on, the touchpad's two sides are
// Gestures and Personal Effects. That is the US disc's layout; the Japanese
// region swaps Circle and Cross, in play and in the menus alike, and the
// region comes from param.sfo (hle/system.cpp).
constexpr std::uint32_t kL3 = 0x2, kR3 = 0x4, kOptions = 0x8, kUp = 0x10, kRight = 0x20, kDown = 0x40, kLeft = 0x80,
                        kL2 = 0x100, kR2 = 0x200, kL1 = 0x400, kR1 = 0x800, kTriangle = 0x1000, kCircle = 0x2000,
                        kCross = 0x4000, kSquare = 0x8000, kTouchPad = 0x100000;

// The defaults are DS3's shape - WASD, Space to roll, E to interact, R for the
// healing item, F for the quick item, Q to lock on, Shift as the strong
// modifier, the mouse's buttons for the hands - put on Bloodborne's buttons.
const BindingInfo kInfo[kBindCount] = {
    {"move_forward", "Move Forward", "Move the character forward.", 0, "W", 0},
    {"move_back", "Move Back", "Move the character backward.", 0, "S", 0},
    {"move_left", "Move Left", "Move the character left.", 0, "A", 0},
    {"move_right", "Move Right", "Move the character right.", 0, "D", 0},
    // Held with the movement keys, as Dark Souls III's Left Alt: a walk.
    {"walk", "Walk", "Hold with movement to walk slowly.", 0, "Left Alt", 0},
    {"roll", "Dodge / Sprint", "Tap to dodge; hold while moving to sprint.", kCircle, "Space", 0},
    {"lock_on", "Lock On", "Lock on to a target or center the camera.", kR3, "Q", 0},

    {"attack", "Attack", "Perform a light attack with the right-hand weapon.", kR1, nullptr, 1},
    {"strong_attack", "Strong Attack", "Perform a strong attack; hold to charge.", kR2, nullptr, 0},
    {"strong_modifier", "Strong Attack Modifier", "Hold with light attack to perform a strong attack.", 0, "Left Shift", 0},
    {"transform", "Transform", "Transform the trick weapon or perform a transformation attack.", kL1, "F", 0},
    {"firearm", "Firearm / Parry", "Fire the left-hand weapon to attack or interrupt an enemy.", kL2, nullptr, 2},
    {"blood_vial", "Blood Vial", "Restore health using a Blood Vial.", kTriangle, "C", 0},
    {"use_item", "Use Item", "Use the selected quick item.", kSquare, "R", 0},

    {"interact", "Interact", "Talk, examine, pick up items, open doors, and interact.", kCross, "E", 0},
    {"switch_item", "Next Item", "Select the next equipped quick item.", kDown, "Down", 7},
    {"switch_right", "Switch Right Weapon", "Switch between equipped right-hand weapons.", kRight, "Right", 0},
    {"switch_left", "Switch Left Weapon", "Switch between equipped left-hand weapons.", kLeft, "Left", 0},
    {"dpad_up", "Blood Bullets", "Create Blood Bullets at the cost of health.", kUp, "Up", 0},
    // The touchpad is a button with a side: the game's key guide reads
    // "<KG_TP_L> : Gestures" and "<KG_TP_R> : Personal Effects". A press alone
    // cannot say which, so these place a finger as well (apply below).
    {"gestures", "Gestures", "Open the gestures menu.", kTouchPad, "G", 0},
    {"personal_effects", "Personal Effects", "Open the personal effects menu.", kTouchPad, "Tab", 0},

    {"look_up", "Look Up", "Rotate the camera upward.", 0, "I", 0},
    {"look_down", "Look Down", "Rotate the camera downward.", 0, "K", 0},
    {"look_left", "Look Left", "Rotate the camera left.", 0, "J", 0},
    {"look_right", "Look Right", "Rotate the camera right.", 0, "L", 0},
    {"menu", "Menu", "Open or close the game menu.", kOptions, "Return", 0},
    // Menus need an OK and a Back that are not the gameplay keys sharing their
    // pad bits: nobody reaches for Space to accept or E to back out.
    // Their buttons are the region's (engine/menu_pointer.h), set in apply.
    {"confirm", "Menu Confirm", "Confirm the selected menu entry.", 0, "Return", 0},
    {"back", "Menu Back", "Return to the previous menu.", 0, "Backspace", 0},

    {"l3", "Left Stick Press", "Activate the left stick press action.", kL3, "Left Ctrl", 0},
    // The developers' debug menu: not a pad press at all. The patch opens it
    // on the touchpad's left side, which is also Gestures; this key toggles
    // the menu itself (engine/debug_menu.h), when the Debug Menu plugin is
    // on. The Key Bindings screen lists it only then.
    {"previous_item", "Previous Item", "Select the previous equipped quick item.", kDown, nullptr, 6},
    {"debug_menu", "Debug Menu", "Open or close the developer debug menu when enabled.", 0, "`", 0},
};

// Earlier names a bbhost.toml may still use.
const struct {
    const char* old_name;
    int action;
} kAliases[] = {
    {"dpad_down", kBindSwitchItem}, {"dpad_left", kBindSwitchLeft}, {"dpad_right", kBindSwitchRight},
};

std::atomic<int> g_key[kBindCount];
std::atomic<int> g_mouse[kBindCount];

std::atomic<int> g_capture{-1};
// The Debug Menu key: held per device (keyboard, mouse), since each device's
// actions are read on their own, and a press queued for the pad read.
std::atomic<bool> g_debug_held[2], g_debug_toggle{false};

void note_debug_key(int device, bool down) {
    if (!down) {
        g_debug_held[device].store(false, std::memory_order_relaxed);
    } else if (!g_debug_held[device].exchange(true) && debug_menu_active()) {
        g_debug_toggle.store(true);
    }
}
std::atomic<int> g_capture_done{-1};
std::atomic<bool> g_hold{false};  // after a capture: blocked until nothing is held
std::atomic<std::int64_t> g_hold_ms{0};  // when that hold began
std::atomic<std::int64_t> g_capture_ms{0};  // when the capture began

std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

int scancode_from_name(const std::string& name) {
#if defined(BBHOST_HAVE_SDL3)
    if(name=="Keypad Plus")return native_scancode_from_name("Keypad +");
    if(name=="Comma")return native_scancode_from_name(",");
    if(name=="Keypad Comma")return native_scancode_from_name("Keypad ,");
    return static_cast<int>(native_scancode_from_name(name.c_str()));
#else
    (void)name;
    return 0;
#endif
}

const char* scancode_name(int sc) {
#if defined(BBHOST_HAVE_SDL3)
    const char* n = native_scancode_name(sc);
    return n && *n ? n : nullptr;
#else
    (void)sc;
    return nullptr;
#endif
}

// Mouse 1..5 are the buttons; 6 and 7 the wheel turned up and down, which the
// pad read makes a short press of each notch (hle/system.cpp).
constexpr int kMouseInputs = 7;
const char* const kMouseNames[kMouseInputs + 1] = {nullptr,  "Left Button", "Right Button", "Middle Button",
                                                   "Button 4", "Button 5", "Wheel Up", "Wheel Down"};
// The same where the name has to fit in a sentence.
const char* const kMouseShort[kMouseInputs + 1] = {nullptr, "L Mouse", "R Mouse", "M Mouse", "Mouse 4", "Mouse 5", "Wheel Up", "Wheel Down"};
// Their names in [keys]: Mouse1..Mouse5, WheelUp, WheelDown.
std::string mouse_token(int m) { return m == 6 ? "WheelUp" : m == 7 ? "WheelDown" : "Mouse" + std::to_string(m); }

std::string trim(std::string v) {
    while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '"')) v.pop_back();
    std::size_t i = 0;
    while (i < v.size() && (v[i] == ' ' || v[i] == '\t' || v[i] == '"')) ++i;
    return v.substr(i);
}

int action_named(const char* name) {
    for (int a = 0; a < kBindCount; ++a) {
        if (std::strcmp(name, kInfo[a].key) == 0) return a;
    }
    for (const auto& al : kAliases) {
        if (std::strcmp(name, al.old_name) == 0) return al.action;
    }
    return -1;
}

// Bumped by every change, for consumers that cache what they built from a
// binding (host_bindings_serial).
std::atomic<std::uint64_t> g_serial{1};

// "E", "Mouse2", "X, Mouse2", "none": the whole binding.
void set_from_text(int a, const std::string& text, const char* where) {
    Chords::Binding chord;
    if(text.find(" + ")!=std::string::npos){
      std::size_t pos=0;
      while(pos<text.size()){
       auto end=text.find(" + ",pos);auto tok=trim(text.substr(pos,end==std::string::npos?end:end-pos));
       if(tok.size()==6&&tok.rfind("Mouse",0)==0&&tok[5]>='1'&&tok[5]<='5')chord.mouse|=1u<<(tok[5]-'1');
       else if(tok=="WheelUp"||tok=="WheelDown")chord.mouse|=1u<<(tok=="WheelUp"?5:6);
       else{int k=scancode_from_name(tok);if(!k){host_log("keys: invalid chord token %s",tok.c_str());return;}if(std::find(chord.keys.begin(),chord.keys.end(),k)==chord.keys.end())chord.keys.push_back(k);}
       if(end==std::string::npos)break;pos=end+3;
      }
      std::sort(chord.keys.begin(),chord.keys.end());chord.text=text;Chords::Set(a,chord);g_key[a]=0;g_mouse[a]=0;g_serial.fetch_add(1);return;
    }
    Chords::Clear(a);
    int key = 0, mouse = 0;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        std::size_t end = text.find(',', pos);
        if (end == std::string::npos) end = text.size();
        const std::string tok = trim(text.substr(pos, end - pos));
        pos = end + 1;
        if (tok.empty() || tok == "none") continue;
        if (tok.size() == 6 && tok.rfind("Mouse", 0) == 0 && tok[5] >= '1' && tok[5] <= '5') {
            mouse = tok[5] - '0';
            continue;
        }
        if (tok == "WheelUp" || tok == "WheelDown") {
            mouse = tok == "WheelUp" ? 6 : 7;
            continue;
        }
        const int sc = scancode_from_name(tok);
        if (!sc) {
            host_log("keys: %s: %s = \"%s\" - \"%s\" is not an SDL key name", where, kInfo[a].key, text.c_str(),
                     tok.c_str());
            continue;
        }
        key = sc;
    }
    g_key[a].store(key, std::memory_order_relaxed);
    g_mouse[a].store(mouse, std::memory_order_relaxed);
    g_serial.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace

std::uint64_t host_bindings_serial() { return g_serial.load(std::memory_order_relaxed); }

const BindingInfo& host_binding_info(int action) { return kInfo[action < 0 || action >= kBindCount ? 0 : action]; }

void host_bindings_load_defaults() {
    for (int a = 0; a < kBindCount; ++a) {
        Chords::Clear(a);
        g_key[a].store(kInfo[a].def_key ? scancode_from_name(kInfo[a].def_key) : 0, std::memory_order_relaxed);
        g_mouse[a].store(kInfo[a].def_mouse, std::memory_order_relaxed);
    }
    g_serial.fetch_add(1,std::memory_order_relaxed);
}

bool host_bindings_parse(const char* name, const char* value) {
    const int a = action_named(name);
    if (a < 0) return false;
    set_from_text(a, value, "options");
    return true;
}

void host_bindings_save(std::FILE* f) {
    for(int a=0;a<kBindCount;++a)std::fprintf(f,"bind.%s=%s\n",kInfo[a].key,host_binding_ini_value(a).c_str());
}

std::string host_binding_ini_value(int a) {
    auto chord=Chords::Get(a);if(chord.active())return chord.text;
    const char* key=scancode_name(g_key[a].load());
    std::string value=key?key:"";if(value==",")value="Comma";if(value=="Keypad ,")value="Keypad Comma";if(value=="Keypad +")value="Keypad Plus";
    const int m=g_mouse[a].load();
    if(m) {if(!value.empty())value+=", ";value+=m==6?"WheelUp":m==7?"WheelDown":"Mouse"+std::to_string(m);}
    return value.empty()?"none":value;
}
int host_binding_key(int action) { return action >= 0 && action < kBindCount ? g_key[action].load() : 0; }
int host_binding_mouse(int action) { return action >= 0 && action < kBindCount ? g_mouse[action].load() : 0; }

void host_binding_describe(int action, char* out, std::size_t n) {
    if (!n) return;
    auto chord=Chords::Get(action);if(chord.active()){std::snprintf(out,n,"%s",chord.text.c_str());return;}
    const char* k = scancode_name(host_binding_key(action));
    // A lone ` is a speck in the menu's font.
    if (k && std::strcmp(k, "`") == 0) k = "` (backtick)";
    const int m = host_binding_mouse(action);
    const char* ms = m >= 1 && m <= kMouseInputs ? kMouseNames[m] : nullptr;
    if (k && ms) {
        std::snprintf(out, n, "%s  /  %s", k, ms);
    } else {
        std::snprintf(out, n, "%s", k ? k : ms ? ms : "Unbound");
    }
}

void host_binding_prompt(int action, char* out, std::size_t n) {
    if (!n) return;
    auto chord=Chords::Get(action);if(chord.active()){std::snprintf(out,n,"%s",chord.text.c_str());return;}
    out[0] = '\0';
    const char* k = scancode_name(host_binding_key(action));
    const int m = host_binding_mouse(action);
    // The key first: a player who bound both reads the key in a sentence more
    // easily than "Left Button", and the prompt has room for one.
    const char* what = k ? k : m >= 1 && m <= kMouseInputs ? kMouseShort[m] : nullptr;
    if (what) std::snprintf(out, n, "%s", what);
}

namespace {

// Two opposite directions held at once cancel, as Dark Souls III's keyboard
// does (its digital-to-analog converter sums each axis's keys). Pressing A
// while sprinting on W+D so runs straight on until D is let go - two turns of
// 45 degrees. Letting the newer key win instead (#51's first cut) turned the
// stick 90 degrees in one frame, past the character script's 90-degree
// sprint-turn threshold, and the character spun as if told to turn round.
constexpr int kOpposites[4][2] = {
    {kBindMoveF, kBindMoveB}, {kBindMoveL, kBindMoveR}, {kBindLookU, kBindLookD}, {kBindLookL, kBindLookR}};

// Walk held on a mouse button, for the keyboard's movement keys (the mouse is
// read on the pad read, the keyboard on the window's update).
std::atomic<bool> g_walk_on_mouse{false};

// How far the movement keys push the stick while Walk is held, of a full push.
// Measured in the Hunter's Dream: under 35% the character stands, from 40% to
// 80% it walks (1.65 m/s), at a full push it runs (3.7 m/s) - so the middle of
// the walk, with room either side.
constexpr float kWalkPush = 0.6f;

}  // namespace

void host_bindings_keys_held(const bool* keys, int nkeys, bool held[kBindCount]) {
    for (int a = 0; a < kBindCount; ++a) {
        const int sc = g_key[a].load(std::memory_order_relaxed);
        held[a] = sc > 0 && sc < nkeys && keys[sc];
    }
    for (const auto& pair : kOpposites) {
        if (held[pair[0]] && held[pair[1]]) held[pair[0]] = held[pair[1]] = false;
    }
    held[kBindWalk] = held[kBindWalk] || g_walk_on_mouse.load(std::memory_order_relaxed);
    note_debug_key(0, held[kBindDebugMenu]);
}

void host_bindings_mouse_held(std::uint32_t buttons, bool held[kBindCount]) {
    for (int a = 0; a < kBindCount; ++a) {
        const int m = g_mouse[a].load(std::memory_order_relaxed);
        held[a] = m >= 1 && m <= kMouseInputs && (buttons & (1u << (m - 1)));
    }
    g_walk_on_mouse.store(held[kBindWalk], std::memory_order_relaxed);
    note_debug_key(1, held[kBindDebugMenu]);
}

void host_bindings_apply(const bool held[kBindCount], bool strong, PadState& p) {
    bool any = false;
    for (int a = 0; a < kBindCount; ++a) {
        // The Debug Menu key sets no pad bit: its press was queued when it was
        // read, and the pad read toggles the menu on the game's thread
        // (host_bind_take_debug_toggle).
        if (!held[a] || a == kBindDebugMenu) continue;
        any = true;
        std::uint32_t bit = kInfo[a].button;
        // Shift is the strong-attack modifier, as in DS3: one key rather than
        // a second binding for every attack.
        if (a == kBindAttack && strong) bit = kR2;
        if (a == kBindConfirm) bit = menu_confirm_button();
        if (a == kBindBack) bit = menu_back_button();
        p.buttons |= bit;
        if (bit == kR2) p.r2 = 255;  // the triggers are analogue as well as buttons
        if (bit == kL2) p.l2 = 255;
        switch (a) {
            case kBindLookU: p.ry = kStickLow; break;
            case kBindLookD: p.ry = kStickHigh; break;
            case kBindLookL: p.rx = kStickLow; break;
            case kBindLookR: p.rx = kStickHigh; break;
            case kBindGestures:
            case kBindEffects:
                // One quarter in from the edge, half way down, and a stable id
                // for as long as it is held: the game tracks fingers by id, so a
                // new one every frame would read as tapping.
                if (p.touch_count < 2) {
                    PadState::Touch& t = p.touch[p.touch_count++];
                    t.x = static_cast<std::uint16_t>(a == kBindEffects ? kPadTouchW * 3 / 4 : kPadTouchW / 4);
                    t.y = static_cast<std::uint16_t>(kPadTouchH / 2);
                    t.id = 200;
                    t.down = true;
                }
                break;
            default: break;
        }
    }
    // The movement keys: a full push each way, a diagonal the square's corner
    // (as Dark Souls III's keys give) - or, with Walk held, a gentle push the
    // game reads as a walk, a diagonal's two axes shortened so that its reach,
    // and so the walk, is the same. Opposite keys cancel (keys_held above).
    const int x = (held[kBindMoveR] ? 1 : 0) - (held[kBindMoveL] ? 1 : 0);
    const int y = (held[kBindMoveB] ? 1 : 0) - (held[kBindMoveF] ? 1 : 0);
    if (x != 0 || y != 0) {
        int reach = kStickHigh - kStickRest;
        if (held[kBindWalk]) {
            const float r = static_cast<float>(reach) * kWalkPush / (x != 0 && y != 0 ? std::sqrt(2.0f) : 1.0f);
            reach = static_cast<int>(std::lround(r));
        }
        if (x != 0) p.lx = static_cast<std::uint8_t>(kStickRest + x * reach);
        if (y != 0) p.ly = static_cast<std::uint8_t>(kStickRest + y * reach);
    }
    if (any) p.connected = true;
}

namespace {
std::mutex capture_lock;
std::vector<std::string> capture_tokens;
std::atomic<bool> capture_wait_neutral{true};
void capture_add(std::string token){if(token=="Keypad +")token="Keypad Plus";if(token==",")token="Comma";if(token=="Keypad ,")token="Keypad Comma";std::lock_guard guard(capture_lock);if(std::find(capture_tokens.begin(),capture_tokens.end(),token)==capture_tokens.end())capture_tokens.push_back(std::move(token));}
}

void host_bind_capture_begin(int action) {
    if (action < 0 || action >= kBindCount) return;
    {std::lock_guard guard(capture_lock);capture_tokens.clear();}
    capture_wait_neutral=true;
    g_capture_ms.store(now_ms(), std::memory_order_relaxed);
    g_capture.store(action, std::memory_order_relaxed);
    host_log("keys: rebinding %s - accumulate keys/clicks, release all to finish (Escape cancels, Delete unbinds)",
             kInfo[action].key);
}

int host_bind_capturing() { return g_capture.load(std::memory_order_relaxed); }

namespace {
void finish(int a, const char* what) {
    g_serial.fetch_add(1, std::memory_order_relaxed);
    g_capture.store(-1, std::memory_order_relaxed);
    g_hold_ms.store(now_ms(), std::memory_order_relaxed);
    g_hold.store(true, std::memory_order_relaxed);
    g_capture_done.store(a, std::memory_order_relaxed);
    char d[96];
    host_binding_describe(a, d, sizeof(d));
    host_log("keys: %s %s, now %s", kInfo[a].key, what, d);
}
}  // namespace

bool host_bind_capture_key(int scancode) {
    const int a=g_capture.load();if(a<0)return false;
    if(scancode==SDL_SCANCODE_ESCAPE){finish(a,"unchanged");return true;}
    if(capture_wait_neutral)return true;
    if(scancode==SDL_SCANCODE_DELETE){host_binding_clear(a);finish(a,"unbound");return true;}
    if(native_reserved_key(scancode)){host_log("keys: reserved port hotkey; choose another key");return true;}
    const char* name=scancode_name(scancode);if(name)capture_add(name);
    return true;
}
bool host_bind_capture_mouse(int button) {
    if(g_capture.load()<0||button<1||button>kMouseInputs)return false;
    if(capture_wait_neutral)return true;
    capture_add(mouse_token(button));return true;
}

int host_bind_capture_take_done() { return g_capture_done.exchange(-1); }
std::string host_bind_capture_preview(){std::lock_guard guard(capture_lock);std::string text;for(auto& token:capture_tokens){if(!text.empty())text+=" + ";text+=token;}return text.empty()?"Press combination...":text+" ...";}

bool host_bind_take_debug_toggle() { return g_debug_toggle.exchange(false); }

bool host_bind_capture_blocking() {
    return g_capture.load(std::memory_order_relaxed) >= 0 || g_hold.load(std::memory_order_relaxed);
}

namespace {
std::atomic<std::int64_t> g_clear_ms{0};
}  // namespace

void host_bind_note_clear_key() { g_clear_ms.store(now_ms(), std::memory_order_relaxed); }

bool host_bind_take_clear_key() {
    const std::int64_t at = g_clear_ms.exchange(0);
    return at && now_ms() - at < 250;
}

void host_binding_clear(int action) {
    if (action < 0 || action >= kBindCount) return;
    Chords::Clear(action);
    g_key[action].store(0, std::memory_order_relaxed);
    g_mouse[action].store(0, std::memory_order_relaxed);
    g_serial.fetch_add(1, std::memory_order_relaxed);
    host_log("keys: %s cleared", kInfo[action].key);
}

void host_bind_capture_note_held(bool any_down) {
    if(g_capture.load()>=0 && !any_down){
      if(capture_wait_neutral.exchange(false))return;
      std::string value;
      {std::lock_guard guard(capture_lock);for(const auto& token:capture_tokens){if(!value.empty())value+=" + ";value+=token;}}
      if(!value.empty()){
       const int a=g_capture.load();set_from_text(a,value,"capture");
       const auto chord=Chords::Get(a);
       for(int b=0;b<kBindCount;++b)if(b!=a){
        auto other=Chords::Get(b);
        if(chord.active()&&other.active()&&chord.keys==other.keys&&chord.mouse==other.mouse)host_binding_clear(b);
        else if(!chord.active()&&!other.active()){
         if(g_key[a]&&g_key[a]==g_key[b])g_key[b]=0;
         if(g_mouse[a]&&g_mouse[a]==g_mouse[b])g_mouse[b]=0;
        }
       }
       finish(a,"rebound");
      }
    }
    if(g_hold.load()&&(!any_down||now_ms()-g_hold_ms.load()>1500))g_hold=false;
}

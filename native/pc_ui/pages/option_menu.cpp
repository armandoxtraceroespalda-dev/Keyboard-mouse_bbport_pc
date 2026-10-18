// SPDX-License-Identifier: GPL-3.0-or-later
// Native System page and guest row builders adapted from bbhost option_menu.cpp.
// The existing renderer and bbport.ini remain authoritative.
#include "option_menu.h"
#include "../engine_hooks.h"
#include "../menu_memory.h"
#include "../menu_asset_mounts.h"
namespace BbEngine::Graphics { inline bool Enabled(){ return false; } }
#include "bindings.h"
#include "binding_pages.h"
#include "shared_camera_settings.h"
#include "menu_navigation.h"
#include "language_text.h"
#include "prompt_text.h"
extern bool debug_menu_active();
extern std::uint32_t menu_confirm_button();
extern std::uint32_t menu_back_button();
extern std::uint32_t input_delivered_buttons();
#include <chrono>
#include "bbport_platform.h"
#include "bbport_settings.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>
extern "C" int runtime_file_translate(const char*,char*,std::size_t);
extern "C" int runtime_file_mount(const char*,const char*);
extern "C" void runtime_file_unmount(const char*);
namespace BbEngine::Options {
namespace {
#define GUEST_ABI __attribute__((sysv_abi))
std::uint64_t g_slide=0;
bool g_installed=false;
constexpr std::uint64_t kPreferredGuestSlide=0x400000;
constexpr std::uint64_t kOpenSection=0x1f20900,kScratchInit=0x208af20,kAddSliderRow=0x1f2ac00;
constexpr std::uint64_t kAddChoiceRow=0x1f2a100,kBuildOnOff=0x1f2b3b0,kWstrCpy=0x2d67040;
constexpr std::uint64_t kListAppend=0x1f1c0c0,kAddListRow=0x1f29370;
constexpr std::uint64_t kListRedraw=0x1ed06e0,kLayoutRows=0x1f28de0,kMsgRepository=0x1ee8cc0;
constexpr std::uint32_t kDefaultsRowId=0x9d8a,kMsgMenuText=200,kMsgLineHelp=201;
constexpr std::uint64_t kAddCommandRow=0x1f4e2a0,kSystemFinalize=0x1fea630;
constexpr std::uint64_t kOpenSectionFlow=0x1fb4c70,kRowFunctorVTable=0x573d120,kOpenerFunctorVTable=0x5743880;
constexpr int kSectionSlots=6;
char g_open_name[32]="PCCamera";
std::uint64_t guest(std::uint64_t bn) { return g_slide+(bn-kPreferredGuestSlide); }
void* guest_fn(std::uint64_t bn) { return reinterpret_cast<void*>(guest(bn)); }
template<typename T,typename... A> T hle_call_guest(void* fn,A... a) { return static_cast<T>(Call(reinterpret_cast<std::uint64_t>(fn),a...)); }
void host_log(const char* format,...) {
    std::fputs("Engine: ",stdout); va_list args; va_start(args,format); std::vprintf(format,args); va_end(args); std::putchar('\n');
}
struct GuestFunction {
    alignas(16) std::uint8_t buf[0x28]{};

    GuestFunction(std::uint64_t vtable_bn, void* fn) {
        const std::uint64_t vt = guest(vtable_bn);
        void* self = &buf[0];
        std::memcpy(&buf[0x00], &vt, sizeof(vt));
        std::memcpy(&buf[0x08], &fn, sizeof(fn));
        std::memcpy(&buf[0x20], &self, sizeof(self));
    }
};

// A caption pair: two objects 0x40 apart, the label in the first and the line
// help in the second, which is the shape every row builder reads. Within each,
// +0x00 is the raw UTF-16 pointer the row draws, +0x08 a wstring the command
// row copies, +0x38 a flag.
//
// The port used to build one of these from an arbitrary message id and then
// store a pointer to its own string literal over the text. It drew, but the
// flags beside it were the placeholder's, and a section title - which the
// movie carries as a `StaticText_<id>` placement, the instance name being the
// id - could not be done that way at all. So the port has message ids of its
// own now (tools/pc_option_messages.tsv, written into the overlay's
// menu.msgbnd.dcx by engine/menu_assets.cpp) and asks for them exactly as the game
// does.
struct Captions {
    alignas(16) std::uint8_t buf[0x80]{};

    explicit Captions(std::uint32_t id) {
        hle_call_guest<std::int64_t>(guest_fn(kMsgRepository), &buf[0], kMsgMenuText, id);
        hle_call_guest<std::int64_t>(guest_fn(kMsgRepository), &buf[0x40], kMsgLineHelp, id);
    }

    // The repository's text is copied into a wstring beside the pointer, and a
    // caption long enough to leave the SSO buffer put it on the heap. Every
    // builder that makes one of these frees it the same way, so this does too:
    // one leak per menu open is still a leak.
    ~Captions() {
        free_wstring(&buf[0x08]);
        free_wstring(&buf[0x48]);
    }

    Captions(const Captions&) = delete;
    Captions& operator=(const Captions&) = delete;

    // MSVC's _String_val: the union at +0x00 is either eight wchars or the
    // heap pointer, the length is at +0x10, the capacity at +0x18 and the DL
    // allocator at +0x20. Capacity below 8 is the small-string buffer.
    static void free_wstring(void* ws) {
        auto* p = static_cast<std::uint8_t*>(ws);
        std::uint64_t cap = 0, ptr = 0, alloc = 0;
        std::memcpy(&cap, p + 0x18, sizeof(cap));
        if (cap < 8) {
            return;
        }
        std::memcpy(&ptr, p, sizeof(ptr));
        std::memcpy(&alloc, p + 0x20, sizeof(alloc));
        if (!ptr || !alloc) {
            return;
        }
        std::uint64_t vtable = 0;
        std::memcpy(&vtable, reinterpret_cast<void*>(static_cast<std::uintptr_t>(alloc)),
                    sizeof(vtable));
        std::uint64_t fn = 0;
        std::memcpy(&fn, reinterpret_cast<void*>(static_cast<std::uintptr_t>(vtable + 0x70)),
                    sizeof(fn));
        hle_call_guest<std::int64_t>(reinterpret_cast<void*>(static_cast<std::uintptr_t>(fn)),
                                     alloc, ptr);
    }
};

// The repository's own text for a message id: the caption object's +0x00,
// which the repository owns and which outlives anything the port holds.
const char16_t* message_text(std::uint32_t id) {
    Captions c(id);
    const char16_t* t = nullptr;
    std::memcpy(&t, &c.buf[0], sizeof(t));
    return t;
}

// A list entry, the 0x48 bytes every list the rows take is made of: the
// value at +0x00 (a byte in a choice row's list, an int32 in a pick list's),
// the raw UTF-16 pointer that is drawn at +0x08, a wstring copy of it at
// +0x10 and a flag at +0x40. The containers deep-copy an entry, so the
// caller's copy, and later the container's, own a heap buffer for any text
// of eight characters or more, and free it the way the game's own handlers
// do: the buffer pointer at +0x18, its capacity at +0x30, the allocator at
// +0x38 - which is Captions::free_wstring's shape from +0x18.
void make_entry(std::uint8_t (&entry)[0x48], std::int32_t value, const char16_t* text) {
    // A message the bundle does not have (a player's own older menu.msgbnd in
    // paths.mods wins over the one made at start) gives no text, and the
    // game's wstring copy would read through null: an empty entry instead.
    if (!text) text = u"";
    std::memset(entry, 0, sizeof(entry));
    std::memcpy(&entry[0], &value, sizeof(value));
    std::memcpy(&entry[8], &text, sizeof(text));
    hle_call_guest<std::int64_t>(guest_fn(kWstrCpy), &entry[0x10], text);
    entry[0x40] = 1;
}

void free_entries(std::uint8_t* first, std::uint64_t count) {
    for (std::uint64_t k = 0; k < count; ++k) {
        Captions::free_wstring(first + k * 0x48 + 0x18);
    }
}

// Every builder takes the value its row's **Defaults** restores. The widgets
// copy it when they are built - a choice row keeps the byte at +0x368 beside
// its value pointer, a pick list the int32 at +0x3d4 - and the game's own
// Defaults row (sub_1f24850: "restore defaults?", then sub_1f1a2d0 calls every
// widget's slot +0x20) writes it back through the value pointer. So each row
// is built with what its setting ships with (host_opt_default_*), and the poll
// sees the change like any other.

// The game's slider: a byte from 0 to 10 in steps of 1 (sub_1f2ac00 builds it
// with those bounds), as Controls' Camera Sensitivity uses it.
void add_slider_row(void* dialog, std::uint8_t* value, std::uint32_t id, std::uint8_t def) {
    Captions c(id);
    hle_call_guest<std::int64_t>(guest_fn(kAddSliderRow), dialog, &c.buf[0], value, &def);
}

// An On/Off row. sub_1f2b3b0 builds the two-entry list from the game's own
// "on" and "off" messages, so the port does not have to construct a choice
// entry - the one structure here it would otherwise have to guess.
void add_toggle_row(void* dialog, void* value, std::uint32_t id, std::uint8_t def) {
    Captions c(id);
    alignas(16) std::uint8_t list[0x100]{};
    hle_call_guest<std::int64_t>(guest_fn(kBuildOnOff), &list);
    hle_call_guest<std::int64_t>(guest_fn(kAddChoiceRow), dialog, &c.buf[0], value, &list, &def);
    std::uint64_t n = 0;
    std::memcpy(&n, &list[0x98], sizeof(n));
    free_entries(list, n);
}

struct Choice {
    std::int32_t value;
    std::uint32_t id;
};

// A pick-list row. Its entries are the 0x48-byte shape above with an int32
// value, which is the same shape the game builds for Language.
void add_list_row(void* dialog, void* value, std::uint32_t id, const Choice* choices, int count, std::int32_t def) {
    Captions c(id);
    // The container is an inline array plus a count at +0x908; zeroing it is
    // the whole of its initialisation.
    alignas(16) std::uint8_t list[0x1000]{};
    for (int i = 0; i < count; ++i) {
        alignas(16) std::uint8_t entry[0x48];
        make_entry(entry, choices[i].value, message_text(choices[i].id));
        hle_call_guest<std::int64_t>(guest_fn(kListAppend), &list, &entry);
        free_entries(entry, 1);
    }
    hle_call_guest<std::int64_t>(guest_fn(kAddListRow), dialog, &c.buf[0], value, &list, &def);
    free_entries(list, static_cast<std::uint64_t>(count));
}


// The dialog's row vector, found by reading the row builder: sub_1f2a100 ends
// with sub_1f2bf60(dialog + 0xe50, &row), and that is a push_back of 0x90-byte
// records - begin at +0xe58, end at +0xe60, each holding the row's caption
// pointer at +0x00, its line help's at +0x40 and its widget at +0x80. Reading
// it back is how the port knows what the engine actually took.
constexpr std::size_t kItemsBegin = 0x0e58, kItemsEnd = 0x0e60, kItemStride = 0x90, kItemWidget = 0x80;
// The dialog's own list component, whose items are the rows (their captions
// and cursor), and within a choice widget (sub_1f2a100, 0x410 bytes) the list
// that draws its value and the pointer the value is written through.
constexpr std::size_t kDialogRows = 0x0a70, kChoiceList = 0xa0, kChoiceValue = 0x360;

int dialog_row_count(void* dialog) {
    std::uint64_t begin = 0, end = 0;
    std::memcpy(&begin, static_cast<std::uint8_t*>(dialog) + kItemsBegin, sizeof(begin));
    std::memcpy(&end, static_cast<std::uint8_t*>(dialog) + kItemsEnd, sizeof(end));
    if (!begin || end < begin || (end - begin) % kItemStride != 0) {
        return -1;
    }
    return static_cast<int>((end - begin) / kItemStride);
}

std::uint8_t* dialog_widget(void* dialog, int row) {
    std::uint64_t begin = 0, w = 0;
    std::memcpy(&begin, static_cast<std::uint8_t*>(dialog) + kItemsBegin, sizeof(begin));
    std::memcpy(&w, reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(begin)) + row * kItemStride + kItemWidget,
                sizeof(w));
    return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(w));
}

void log_rows(const char* name, void* dialog, int built, int slots) {
    // Read back from the engine's own vector, so a row built and then lost is
    // loud rather than silent. sub_1f20900 appends its "Defaults" row after
    // the handler returns, so it is not counted here.
    const int took = dialog_row_count(dialog);
    host_log("pc-options: %s: %d rows built, %d in the dialog, %d slots%s", name, built, took, slots,
             took != built || took > slots ? "  <- not every row will draw" : "");
}

constexpr std::uint64_t kChoiceAppend=0x1f2c200;
void redraw(std::uint8_t* list) { hle_call_guest<std::int64_t>(guest_fn(kListRedraw),list); }
void add_text_row(void* dialog, std::uint8_t* value, std::uint32_t id, const char16_t* label, const char16_t* help,
                  const char16_t* text) {
    Captions c(id);
    std::memcpy(&c.buf[0x00], &label, sizeof(label));
    std::memcpy(&c.buf[0x40], &help, sizeof(help));
    alignas(16) std::uint8_t list[0x100]{};
    alignas(16) std::uint8_t entry[0x48];
    make_entry(entry, 0, text);
    hle_call_guest<std::int64_t>(guest_fn(kChoiceAppend), &list, &entry);
    free_entries(entry, 1);
    *value = 0;
    std::uint8_t def = 0;  // its only entry; the Key Bindings screen has no Defaults row
    hle_call_guest<std::int64_t>(guest_fn(kAddChoiceRow), dialog, &c.buf[0], value, &list, &def);
    free_entries(list, 1);
}


constexpr std::uint64_t kListVTable=0x5799fb0;

constexpr std::uint32_t kKeysPageRow=119000,kKeysFirstRow=119001;
int g_keys_page = 0;  // kept across opens, as DS3 keeps its tab
// Seven choice rows: category and six actions; native Defaults follows; display mode lives in vanilla Controls.
constexpr int kKeysRows = 1 + BindingPages::Slots;
std::uint8_t g_keys_value[kKeysRows];
char16_t g_keys_label[kKeysRows][48];
char16_t g_keys_help[kKeysRows][160];
char16_t g_keys_text[kKeysRows][64];
struct KeysScreen {
    std::uint8_t* dialog = nullptr;
    std::uint8_t* rows = nullptr;          // the dialog's own list
    std::uint8_t* widget[kKeysRows] = {};  // each row's choice widget
    int page = -1;                         // the page the rows show
    std::uint32_t pad_was = 0;             // for Circle's and the arrows' edges
};
KeysScreen g_keys;
// A clear made on the menu thread, for the poll to save on the window thread
// with every other settings write.
std::atomic<bool> g_keys_save{false};

// Detect the game-selected message bundle, not the launcher's language.
PcLanguage::Language g_keys_language=PcLanguage::Language::English;
const char* localized(const char* source){return PcLanguage::Translate(source,g_keys_language);}
template <std::size_t N>
void to_utf16(char16_t (&out)[N],const char* s) {
 auto wide=PcPrompts::wide(localized(s?s:""));
 const auto n=std::min(wide.size(),N-1);std::copy_n(wide.data(),n,out);out[n]=0;
}

// The page names, as the tabs read. Plain ASCII: the line help and values
// are drawn as HTML text, where an ampersand is not a character.
const auto& kKeyPageNames=BindingPages::Names;
constexpr int CategoryCount=BindingPages::Count;

// The actions listed: the Debug Menu key only when the menu is there (the
// Debug Menu plugin, engine/debug_menu.h); it is the last.
static_assert(kBindDebugMenu == kBindCount - 1, "the Debug Menu key is the last action");
int keys_listed() { return debug_menu_active() ? kBindCount : kBindDebugMenu; }

// The row's action; keys_listed() or more for a row the last, short page
// leaves blank, which shows nothing and takes no capture.
int keys_action(int row) { return BindingPages::Action(g_keys.page,row-1); }
bool keys_row_bound(int row) { return row >= 1 && row < kKeysRows && keys_action(row)>=0 && keys_action(row) < keys_listed(); }

void keys_fill_value(int row) {
    if(row==0){to_utf16(g_keys_text[row],kKeyPageNames[g_keys.page]);return;}
    if (!keys_row_bound(row)) {
        g_keys_text[row][0] = 0;
        return;
    }
    char d[64];
    if(BbSettings::Get().keyboard_mouse)host_binding_describe(keys_action(row), d, sizeof(d));
    else {
      std::snprintf(d,sizeof d,"%s",NativePages::ControllerBindingLabel(keys_action(row)).c_str());
    }
    to_utf16(g_keys_text[row], d);
}

void keys_fill(int page) {
    const auto* title=message_text(119020);
    g_keys_language=PcLanguage::Detect(title);
    g_keys.page = (page % CategoryCount + CategoryCount) % CategoryCount;
    g_keys_page = g_keys.page;
    to_utf16(g_keys_label[0], "Category");
    to_utf16(g_keys_help[0], "Left and right change the group of actions shown.");
    keys_fill_value(0);
    for (int r = 1; r < kKeysRows; ++r) {
        if (!keys_row_bound(r)) {
            g_keys_label[r][0] = g_keys_help[r][0] = g_keys_text[r][0] = 0;
            continue;
        }
        const BindingInfo& b = host_binding_info(keys_action(r));
        to_utf16(g_keys_label[r], b.label);
        to_utf16(g_keys_help[r], b.help);
        keys_fill_value(r);
    }
}



// Whether the remembered objects are still this screen's. The dialog is freed
// when it closes and its memory reused, so identity is checked where only the
// port could have put it: every action row writes through one of its bytes.
bool keys_alive() {
    if (!g_keys.dialog || !g_keys.rows) return false;
    std::uint64_t vt = 0;
    std::memcpy(&vt, g_keys.rows, sizeof(vt));
    if (vt != guest(kListVTable) + 0x10 && vt != guest(kListVTable)) return false;
    for (int r = 0; r < kKeysRows; ++r) {
        if (!g_keys.widget[r]) return false;
        std::uint64_t v = 0;
        std::memcpy(&v, g_keys.widget[r] + kChoiceValue, sizeof(v));
        if (v != reinterpret_cast<std::uint64_t>(&g_keys_value[r])) return false;
    }
    return true;
}

GUEST_ABI void pc_keys_handler(void* dialog, void* params) {
    host_log("pc-options: PCKeys opened, dialog=%p params=%p", dialog, params);
    alignas(16) std::uint8_t scratch[0x100]{};
    hle_call_guest<std::int64_t>(guest_fn(kScratchInit), &scratch);
    g_keys = KeysScreen{};
    // The Circle that opened the screen may still be down when the page row
    // first has focus; it is not a press on that row.
    g_keys.pad_was = input_delivered_buttons();
    keys_fill(g_keys_page);
    add_text_row(dialog, &g_keys_value[0], kKeysPageRow, g_keys_label[0], g_keys_help[0], g_keys_text[0]);
    for (int r = 1; r < kKeysRows; ++r) {
        add_text_row(dialog, &g_keys_value[r], kKeysFirstRow + r - 1, g_keys_label[r], g_keys_help[r], g_keys_text[r]);
    }
    log_rows("PCKeys", dialog, kKeysRows, kKeysRows);
    if (dialog_row_count(dialog) < kKeysRows) return;
    auto* d = static_cast<std::uint8_t*>(dialog);
    g_keys.rows = d + kDialogRows;
    for (int r = 0; r < kKeysRows; ++r) g_keys.widget[r] = dialog_widget(dialog, r);
    g_keys.dialog = d;
    if (!keys_alive()) {
        host_log("pc-options: PCKeys: the dialog is not laid out as expected; bindings show but cannot change here");
        g_keys = KeysScreen{};
    }
}

// The screen's per-frame work, on the menu thread. `row` is which of the
// screen's lists is updating: -2 the dialog's own, 0 the page row's value,
// 1..7 an action row's.
void keys_update(int row, bool focused) {
    bool rows_changed = false, values_changed = false;
    if(host_bind_capturing()>=0)for(int r=1;r<kKeysRows;++r)if(keys_row_bound(r)&&keys_action(r)==host_bind_capturing()){
      to_utf16(g_keys_text[r],host_bind_capture_preview().c_str());values_changed=true;
    }
    if(!BbSettings::Get().keyboard_mouse)for(int r=1;r<kKeysRows;++r){
      char16_t before[64];std::memcpy(before,g_keys_text[r],sizeof before);keys_fill_value(r);
      if(std::memcmp(before,g_keys_text[r],sizeof before))values_changed=true;
    }
    if (const int done = host_bind_capture_take_done(); done >= 0) {
        // Every row, not just the one captured: a key taken from another
        // action on this page changes that row too.
        for (int r = 1; r < kKeysRows; ++r) keys_fill_value(r);
        values_changed = true;
    }
    if (focused && row >= 0 && host_bind_capturing() < 0) {
        const std::uint32_t pad = input_delivered_buttons();
        const std::uint32_t edge = pad & ~g_keys.pad_was;
        g_keys.pad_was = pad;
        const std::uint32_t decide = menu_confirm_button();
        if (row == 0 && (edge & (decide | 0x20u | 0x80u))) {
            // Right, or Circle / a click, is the next page; Left the previous.
            keys_fill(g_keys.page + ((edge & 0x80u) ? -1 : 1));
            rows_changed = values_changed = true;
        } else if (keys_row_bound(row) && BbSettings::Get().keyboard_mouse && host_bind_take_clear_key()) {
            host_binding_clear(keys_action(row));
            keys_fill_value(row);
            redraw(g_keys.widget[row] + kChoiceList);
            g_keys_save.store(true, std::memory_order_relaxed);
        } else if (keys_row_bound(row) && BbSettings::Get().keyboard_mouse && (edge & decide)) {
            to_utf16(g_keys_text[row], "Hold keys/clicks...");
            redraw(g_keys.widget[row] + kChoiceList);
            host_bind_capture_begin(keys_action(row));

        }
    }
    if (rows_changed) redraw(g_keys.rows);
    if (values_changed) {
        for (int r = 0; r < kKeysRows; ++r) redraw(g_keys.widget[r] + kChoiceList);
    }
}

struct DefaultsView {
    std::uint8_t* dialog=nullptr;
    std::uint8_t* pick=nullptr;
    bool hidden=false;
    char16_t first=0;
};
DefaultsView g_defaults_view;
char16_t g_defaults_caption[32];
constexpr std::size_t kPickItems=0x100;
void keep_defaults_row(std::uint8_t* dialog, int pick_row) {
    g_defaults_view = DefaultsView{};
    const int rows = dialog_row_count(dialog);
    if (rows < 1) return;
    std::uint64_t begin = 0;
    std::memcpy(&begin, dialog + kItemsBegin, sizeof(begin));
    auto* rec = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(begin)) + (rows - 1) * kItemStride;
    const char16_t* label = nullptr;
    std::memcpy(&label, rec, sizeof(label));
    if (!label || label != message_text(kDefaultsRowId)) {
        host_log("pc-options: the last row is not Defaults; left alone");
        return;
    }
    std::size_t n = 0;
    while (label[n] && n + 1 < sizeof(g_defaults_caption) / sizeof(g_defaults_caption[0])) {
        g_defaults_caption[n] = label[n];
        ++n;
    }
    g_defaults_caption[n] = 0;
    const char16_t* ours = g_defaults_caption;
    std::memcpy(rec, &ours, sizeof(ours));
    hle_call_guest<std::int64_t>(guest_fn(kLayoutRows), dialog, 0);
    g_defaults_view.dialog = dialog;
    g_defaults_view.pick = pick_row >= 0 && pick_row < rows - 1 ? dialog_widget(dialog, pick_row) : nullptr;
}

// From the list update: the pick list took focus, or the rows got it back.
void defaults_view_update(std::uint8_t* comp, bool focused) {
    DefaultsView& v = g_defaults_view;
    if (!v.dialog || !v.pick || !focused) return;
    if (comp == v.pick + kPickItems && !v.hidden) {
        v.hidden = true;
        v.first = g_defaults_caption[0];
        g_defaults_caption[0] = 0;
        redraw(v.dialog + kDialogRows);
    } else if (comp == v.dialog + kDialogRows && v.hidden) {
        v.hidden = false;
        g_defaults_caption[0] = v.first;
        redraw(v.dialog + kDialogRows);
    }
}

// Native Defaults is retained, including its standard confirmation dialog.
std::int64_t open_named(void* root, void* params, const char* name, void* handler, int pick_row) {
    std::snprintf(g_open_name, sizeof(g_open_name), "%s", name);
    g_defaults_view = DefaultsView{};
    const std::int64_t dialog =
        hle_call_guest<std::int64_t>(guest_fn(kOpenSection), root, params, g_open_name, handler, 0, 0);
    auto* d = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(dialog));
    if (d) {
        keep_defaults_row(d, pick_row);
    }
    return dialog;
}


// Values are owned by this page, and written to the existing atomics only after
// the engine's slider changes. Opening the page never overwrites another frontend.
struct Slider {
    std::atomic<float>* setting;
    std::uint32_t id;
    float base,step;
    std::uint8_t def,value=0,last=0;
};
Slider camera[] = {
    {nullptr,118004,1.0f,0.05f,0},
    {nullptr,121000,0.5f,0.10f,5},
    {nullptr,121001,0.5f,0.10f,5}
};
bool seeded=false;
bool camera_page=false,graphics_page=false,input_page=false;
Slider graphics_sliders[]={{nullptr,117007,0,0.2f,5},{nullptr,117006,1,0.2f,0}};
Slider effect_sliders[]={{nullptr,120000,0,0.1f,10},{nullptr,120002,0,0.2f,5}};
struct StartupToggle { std::atomic<bool>* setting=nullptr; std::uint32_t id; std::uint8_t value=0,last=0; };
StartupToggle graphics_toggles[]={{nullptr,117001},{nullptr,117000}};
StartupToggle effect_toggles[]={{nullptr,117005},{nullptr,117002},{nullptr,117003},{nullptr,120001}};
bool graphics_seeded=false,effects_seeded=false;
std::int32_t output_resolution=1,last_resolution=1;
const Choice outputs[]={{0,117010},{1,117012},{2,117013},{3,117015},{4,117040},{5,117041},{6,117042},{7,117043},{8,117044},{9,117045}};
template<typename Rows> void SeedSliders(Rows& rows) {
    for (auto& s:rows) s.value=s.last=static_cast<std::uint8_t>(std::clamp(std::lround((s.setting->load()-s.base)/s.step),0l,10l));
}
void SeedCamera() {
    for (auto& s:camera) s.value=s.last=static_cast<std::uint8_t>(std::clamp(std::lround((s.setting->load()-s.base)/s.step),0l,10l));
    seeded=true;
}
GUEST_ABI void CameraHandler(void* dialog,void* params) {
    host_log("pc-options: PCCamera opened, dialog=%p params=%p",dialog,params);
    alignas(16) std::uint8_t scratch[0x100]{};
    hle_call_guest<std::int64_t>(guest_fn(kScratchInit),&scratch);
    SeedCamera();
    for (auto& s:camera) add_slider_row(dialog,&s.value,s.id,s.def);
    log_rows("PCCamera",dialog,3,3);
}
GUEST_ABI std::int64_t OpenCamera(void* root,void* params) {
    return open_named(root,params,"PCCamera",reinterpret_cast<void*>(&CameraHandler),-1);
}
GUEST_ABI void* OpenCameraRow(void* out,void* ctx) {
    GuestFunction f(kOpenerFunctorVTable,reinterpret_cast<void*>(&OpenCamera));
    hle_call_guest<std::int64_t>(guest_fn(kOpenSectionFlow),out,ctx,f.buf);
    return out;
}
GUEST_ABI void GraphicsHandler(void* dialog,void* params) {
    host_log("pc-options: PCGraphics opened, dialog=%p params=%p",dialog,params);
    alignas(16) std::uint8_t scratch[0x100]{};
    hle_call_guest<std::int64_t>(guest_fn(kScratchInit),&scratch);
    for (auto& t:graphics_toggles) { t.value=t.last=t.setting->load()?1:0;add_toggle_row(dialog,&t.value,t.id,1); }
    SeedSliders(graphics_sliders);
    for (auto& s:graphics_sliders) add_slider_row(dialog,&s.value,s.id,s.def);
    output_resolution=last_resolution=BbSettings::Get().output_res;
    add_list_row(dialog,&output_resolution,117004,outputs,BbSettings::OutputCount,BbSettings::OutputDefault);
    graphics_seeded=true;log_rows("PCGraphics",dialog,5,5);
}
GUEST_ABI void EffectsHandler(void* dialog,void* params) {
    host_log("pc-options: PCEffects opened, dialog=%p params=%p",dialog,params);
    alignas(16) std::uint8_t scratch[0x100]{};
    hle_call_guest<std::int64_t>(guest_fn(kScratchInit),&scratch);
    for (auto& t:effect_toggles) { t.value=t.last=t.setting->load()?1:0;add_toggle_row(dialog,&t.value,t.id,1); }
    SeedSliders(effect_sliders);
    for (auto& s:effect_sliders) add_slider_row(dialog,&s.value,s.id,s.def);
    effects_seeded=true;log_rows("PCEffects",dialog,6,6);
}
GUEST_ABI std::int64_t OpenGraphics(void* root,void* params) {
    return open_named(root,params,"PCGraphics",reinterpret_cast<void*>(&GraphicsHandler),4);
}
GUEST_ABI std::int64_t OpenEffects(void* root,void* params) {
    return open_named(root,params,"PCEffects",reinterpret_cast<void*>(&EffectsHandler),-1);
}
GUEST_ABI void* OpenGraphicsRow(void* out,void* ctx) {
    GuestFunction f(kOpenerFunctorVTable,reinterpret_cast<void*>(&OpenGraphics));
    hle_call_guest<std::int64_t>(guest_fn(kOpenSectionFlow),out,ctx,f.buf);return out;
}
GUEST_ABI void* OpenEffectsRow(void* out,void* ctx) {
    GuestFunction f(kOpenerFunctorVTable,reinterpret_cast<void*>(&OpenEffects));
    hle_call_guest<std::int64_t>(guest_fn(kOpenSectionFlow),out,ctx,f.buf);return out;
}


struct VanillaScreen {
 std::uint8_t* dialog=nullptr;std::uint8_t* widget=nullptr;std::uint8_t* link=nullptr;
 std::uint8_t value=0;std::uint32_t pad_was=0;int anchor_row=0;
 std::uint64_t child=0,root_data=0,hidden_rows[16]{};int hidden_count=0;bool visibility_logged=false;
};
VanillaScreen vanilla_controls,vanilla_environment;
char16_t device_label[48],device_help[160],device_text[64];

void detect_language(){const auto* title=message_text(119020);g_keys_language=PcLanguage::Detect(title);}
void device_caption(){to_utf16(device_label,"Play with");to_utf16(device_help,"Use Controller or Keyboard and mouse");to_utf16(device_text,BbSettings::Get().keyboard_mouse?"Keyboard and mouse":"Controller");}
bool screen_alive(const VanillaScreen& v){
 if(!v.dialog||!v.widget)return false;
 std::uint64_t vt=0,ptr=0;
 if(!BbPlatform::ReadProcessMemory(v.dialog,&vt,8)||vt!=g_slide+0x539abe0)return false;
 std::uint64_t begin=0,end=0,found=0;
 if(!BbPlatform::ReadProcessMemory(v.dialog+kItemsBegin,&begin,8)||!BbPlatform::ReadProcessMemory(v.dialog+kItemsEnd,&end,8))return false;
 if(!begin||v.anchor_row<0||end<begin+std::uint64_t(v.anchor_row+1)*kItemStride)return false;
 return BbPlatform::ReadProcessMemory(reinterpret_cast<void*>(begin+v.anchor_row*kItemStride+kItemWidget),&found,8)&&found==reinterpret_cast<std::uint64_t>(v.widget);
}
bool sync_camera(const SharedCamera::Values& v){
 auto& cfg=BbSettings::Get();bool changed=cfg.mouse_sensitivity!=v.sensitivity||cfg.mouse_invert_x!=v.invert_x||cfg.mouse_invert_y!=v.invert_y||cfg.mouse_auto_rotation!=v.auto_adjust;
 cfg.mouse_sensitivity=v.sensitivity;cfg.mouse_invert_x=v.invert_x;cfg.mouse_invert_y=v.invert_y;cfg.mouse_auto_rotation=v.auto_adjust;
 return changed;
}
void sync_controls(){
 if(!screen_alive(vanilla_controls))return;
 std::uint8_t b[16];SharedCamera::Values v;
 if(BbPlatform::ReadProcessMemory(vanilla_controls.dialog+0xeb0,b,sizeof b)&&SharedCamera::DecodeMenu(b,v)&&sync_camera(v))g_keys_save=true;
}
GUEST_ABI std::int64_t OpenKeys(void* root,void* params) {return open_named(root,params,"PCKeys",reinterpret_cast<void*>(&pc_keys_handler),-1);}
bool nested_active(const VanillaScreen& screen){
 if(!screen.child||!screen_alive(screen))return false;
 return NativeNavigation::ContainsChild(screen.dialog,screen.child,[](const void* p,void* out,std::size_t n){return BbPlatform::ReadProcessMemory(p,out,n);});
}
// Managed GFx wrappers must stay in place until their managed value is released.
struct PanelClip {
 alignas(16) std::uint8_t bytes[0x80]{};
 PanelClip()=default;PanelClip(const PanelClip&)=delete;
 ~PanelClip(){
  std::uint32_t type=0;std::memcpy(&type,bytes+0x48,4);
  if(!(type&0x40))return;
  std::uint64_t oi=0,vt=0,release=0,data=0;
  std::memcpy(&oi,bytes+0x40,8);std::memcpy(&data,bytes+0x50,8);
  if(oi&&BbPlatform::ReadProcessMemory(reinterpret_cast<void*>(oi),&vt,8)&&
     BbPlatform::ReadProcessMemory(reinterpret_cast<void*>(vt+0x18),&release,8)&&release)
   Call(release,oi,bytes+0x30,data);
 }
 std::uint64_t data()const{std::uint32_t type=0;std::uint64_t data=0;std::memcpy(&type,bytes+0x48,4);std::memcpy(&data,bytes+0x50,8);return (type&0x8f)==10?data:0;}
};
bool parent_visible(VanillaScreen& screen,bool visible){
 if(!screen_alive(screen)){if(!screen.visibility_logged)host_log("pc-options: parent hide failed: dialog identity changed");screen.visibility_logged=true;return false;}
 // Resolve the visible section from an actual row supplied by the native list.
 // The dialog's cached +0xdf0 wrapper is not used: it did not reliably target
 // the row container and produced a background flash in Windows.
 std::uint64_t provider=0,vt=0,make=0;
 if(!BbPlatform::ReadProcessMemory(screen.dialog+kDialogRows+0x258,&provider,8)||!provider||
    !BbPlatform::ReadProcessMemory(reinterpret_cast<void*>(provider),&vt,8)||
    !BbPlatform::ReadProcessMemory(reinterpret_cast<void*>(vt+0x18),&make,8)||!make){if(!screen.visibility_logged)host_log("pc-options: parent hide failed: row provider unresolved");screen.visibility_logged=true;return false;}
 PanelClip row,panel;std::uint64_t cell=0;
 Call(make,row.bytes,provider,&cell);
 if(!row.data()){if(!screen.visibility_logged)host_log("pc-options: parent hide failed: row is not a display object");screen.visibility_logged=true;return false;}
 Call(g_slide+0x1adb220,panel.bytes,row.bytes,"parent",0);
 screen.root_data=panel.data();
 if(screen.root_data)Call(g_slide+0x1acb760,panel.bytes,int(visible));
 // Also suppress the concrete rows. This remains effective if the AS3
 // parent accessor is absent, and guards rows that native animation re-shows.
 screen.hidden_count=0;int hidden=0;
 const int count=std::clamp(dialog_row_count(screen.dialog),0,16);
 for(int i=0;i<count;++i){
  PanelClip item;const std::uint64_t position=std::uint64_t(i)<<32;
  Call(make,item.bytes,provider,&position);const auto data=item.data();if(!data)continue;
  screen.hidden_rows[screen.hidden_count++]=data;
  Call(g_slide+0x1acb760,item.bytes,int(visible));
  std::uint64_t object=0;std::uint8_t flags=0;
  if(BbPlatform::ReadProcessMemory(reinterpret_cast<void*>(data+0x88),&object,8)&&object&&
     BbPlatform::ReadProcessMemory(reinterpret_cast<void*>(object+0x6b),&flags,1)&&!(flags&0x40))++hidden;
 }
 if(!screen.visibility_logged&&!visible){
  host_log("pc-options: parent hide: AS3 parent=%llx; native rows hidden=%d/%d; child active=%d",
   static_cast<unsigned long long>(screen.root_data),hidden,count,int(nested_active(screen)));
  screen.visibility_logged=true;
 }

 return true;
}
void update_parent(VanillaScreen& screen){
 if(!screen.child)return;
 if(!screen_alive(screen)){screen.child=screen.root_data=0;return;}
 const bool active=nested_active(screen);
 parent_visible(screen,!active);
 if(!active)screen.child=screen.root_data=0;
}
std::int64_t GUEST_ABI VisibilityHook(std::uint64_t,const std::uint64_t* saved) noexcept {
 if(!saved[4]||(!vanilla_controls.child&&!vanilla_environment.child))return 0;
 std::uint64_t data=0;
 if(!BbPlatform::ReadProcessMemory(reinterpret_cast<void*>(saved[5]+0x50),&data,8))return 0;
 for(const auto* screen:{&vanilla_controls,&vanilla_environment}){
  bool target=data&&data==screen->root_data;
  for(int i=0;i<screen->hidden_count&&!target;++i)target=data&&data==screen->hidden_rows[i];
  if(target&&nested_active(*screen)){const_cast<std::uint64_t*>(saved)[4]=0;break;}
 }
 return 0;
}
void open_nested(VanillaScreen& screen,void* function){
 if(!screen_alive(screen)||nested_active(screen))return;
 // Use the same original MenuStep factory/context and owning child stack as
 // vanilla Defaults. Back returns to this live parent, including its cursor.
 alignas(16) std::uint64_t result[2]{};
 GuestFunction f(kOpenerFunctorVTable,function);
 hle_call_guest<std::int64_t>(guest_fn(kOpenSectionFlow),result,screen.dialog+0x48,f.buf);
 if(!result[0])return;
 if(NativeNavigation::PushOwnedChild(screen.dialog,result[0])){
  screen.child=result[0];screen.visibility_logged=false;
  host_log("pc-options: child pushed: parent=%p child=%llx active=%d",screen.dialog,static_cast<unsigned long long>(screen.child),int(nested_active(screen)));
  parent_visible(screen,false);
 }else{
  auto* ref=reinterpret_cast<std::int32_t*>(result[0]+8);
  if(--*ref==0){std::uint64_t vt=0,dtor=0;std::memcpy(&vt,reinterpret_cast<void*>(result[0]),8);std::memcpy(&dtor,reinterpret_cast<void*>(vt),8);Call(dtor,result[0]);}
 }
}
GUEST_ABI std::int64_t KeysButton(std::uint64_t,std::uint64_t){open_nested(vanilla_controls,reinterpret_cast<void*>(&OpenKeys));return 0;}
GUEST_ABI std::int64_t CameraButton(std::uint64_t,std::uint64_t){open_nested(vanilla_environment,reinterpret_cast<void*>(&OpenCamera));return 0;}
void add_button_row(void* dialog,std::uint32_t id,void* action){
 // Exactly the vanilla Defaults action-row builder; no choice list/value.
 Captions captions(id);GuestFunction invoke(kOpenerFunctorVTable,action);alignas(16) std::uint8_t empty[0x28]{};
 hle_call_guest<std::int64_t>(reinterpret_cast<void*>(g_slide+0x1b290f0),dialog,captions.buf,invoke.buf,empty);
}
GUEST_ABI void VanillaControlsHandler(void* dialog,void* params){
 detect_language();device_caption();vanilla_controls=VanillaScreen{};vanilla_controls.dialog=static_cast<std::uint8_t*>(dialog);
 alignas(16) std::uint8_t scratch[0x100]{};hle_call_guest<std::int64_t>(guest_fn(kScratchInit),scratch);
 add_text_row(dialog,&vanilla_controls.value,125000,device_label,device_help,device_text);
 hle_call_guest<std::int64_t>(reinterpret_cast<void*>(g_slide+0x1b211a0),dialog,params);
 add_button_row(dialog,125003,reinterpret_cast<void*>(&KeysButton));
 vanilla_controls.widget=dialog_widget(dialog,0);vanilla_controls.pad_was=input_delivered_buttons();
 sync_controls();log_rows("ControllSetting",dialog,dialog_row_count(dialog),8);
}
GUEST_ABI void VanillaEnvironmentHandler(void* dialog,void* params){
 hle_call_guest<std::int64_t>(reinterpret_cast<void*>(g_slide+0x1b21840),dialog,params);
 vanilla_environment=VanillaScreen{};vanilla_environment.dialog=static_cast<std::uint8_t*>(dialog);
 add_button_row(dialog,125004,reinterpret_cast<void*>(&CameraButton));
 vanilla_environment.anchor_row=dialog_row_count(dialog)-1;vanilla_environment.widget=dialog_widget(dialog,vanilla_environment.anchor_row);vanilla_environment.pad_was=input_delivered_buttons();
 log_rows("EnvironmentSetting",dialog,dialog_row_count(dialog),6);
}
std::int64_t GUEST_ABI OpenSectionHook(std::uint64_t,const std::uint64_t* saved) noexcept {
 auto* args=const_cast<std::uint64_t*>(saved);
 if(saved[2]==g_slide+0x1b211a0)args[2]=reinterpret_cast<std::uint64_t>(&VanillaControlsHandler);
 else if(saved[2]==g_slide+0x1b21840)args[2]=reinterpret_cast<std::uint64_t>(&VanillaEnvironmentHandler);
 return 0;
}
std::int64_t GUEST_ABI ConfirmedKeysResetHook(std::uint64_t,const std::uint64_t* saved) noexcept {
 if(saved[5]!=reinterpret_cast<std::uint64_t>(g_keys.dialog)||!keys_alive())return 0;
 const bool selected=BbSettings::Get().keyboard_mouse;
 BbSettings::ApplyDs3Profile();BbSettings::RestoreControllerDefaults();BbSettings::SelectKeyboardMouse(selected);
 keys_fill(g_keys.page);
 for(int r=0;r<kKeysRows;++r)redraw(g_keys.widget[r]+kChoiceList);
 redraw(g_keys.rows);g_keys_save=true;
 host_log("pc-options: key bindings restored after native confirmation");
 return 1; // Native reset of dummy choice values is not a binding reset.
}
std::int64_t GUEST_ABI LoadCameraHook(std::uint64_t,const std::uint64_t* saved) noexcept {
 std::uint8_t b[16];SharedCamera::Values v;
 if(BbPlatform::ReadProcessMemory(reinterpret_cast<void*>(saved[5]),b,sizeof b)&&SharedCamera::DecodeGame(b,v)&&sync_camera(v))g_keys_save=true;
 return 0;
}
void vanilla_update(std::uint8_t* comp,bool focused){
 if(screen_alive(vanilla_controls)){
  sync_controls();
  const bool mode=comp==vanilla_controls.widget+kChoiceList;
  if(mode&&focused){
   const auto now=input_delivered_buttons(),edge=now&~vanilla_controls.pad_was;vanilla_controls.pad_was=now;
   if(edge&(menu_confirm_button()|0x80|0x20)){
    BbSettings::SelectKeyboardMouse(!BbSettings::Get().keyboard_mouse);device_caption();g_keys_save=true;
    redraw(vanilla_controls.widget+kChoiceList);
   }
  }
 }
}
} // namespace

bool Install(std::uint8_t* image,std::size_t size) {
    if (!BbSettings::Get().camera_controls && !BbSettings::Get().change_appearance && !BbSettings::Get().rebirth && !BbSettings::Get().graphics_controls && !BbSettings::Get().pc_controls) return false;
    const char* assets=std::getenv("BB_PC_MENU_ASSETS");
    const char* game=(std::getenv("BB_UI_GAME_VIEW")?std::getenv("BB_UI_GAME_VIEW"):std::getenv("BB_GAME_DIR"));
    if (!assets || !*assets || !game || !*game) {host_log("pc-options rejected: prepared assets or game-view environment missing");return false;}
    // Validate all four patch regions together while the loader owns the RW image.
    constexpr std::size_t table=0x53398b0+6*8,imm=0x1b20e21,call=0x1bb4978,pad=0x1bb4c60;
    constexpr std::uint8_t expected[]={0x90,0x66,0x66,0x66,0x66,0x66,0x66,0x2e,0x0f,0x1f,0x84,0,0,0,0,0};
    if (size<table+16 || size<imm+1 || size<call+5 || size<pad+16) {host_log("pc-options rejected: image too small for System layout");return false;}
    std::uint64_t zeros[2]{}; std::int32_t rel=0;
    std::memcpy(&rel,image+call+1,4);
    if (std::memcmp(image+table,zeros,16) || image[imm]!=6 || image[call]!=0xe8 ||
        static_cast<std::int64_t>(call+5)+rel!=0x1bea630 || std::memcmp(image+pad,expected,16) ||
        !MenuMemory::Compatible(image,size)) {
        std::uint64_t rows[2]{};std::memcpy(rows,image+table,16);
        host_log("pc-options rejected: System layout: extra rows=%llx/%llx (expect zero); count=%u (expect 6); call=%02x target=%llx (expect 1bea630); padding=%s",
          static_cast<unsigned long long>(rows[0]),static_cast<unsigned long long>(rows[1]),unsigned(image[imm]),unsigned(image[call]),static_cast<unsigned long long>(static_cast<std::int64_t>(call+5)+rel),std::memcmp(image+pad,expected,16)?"different":"OK"); return false;
    }
    Hook menu_hooks[4];
    constexpr std::uint8_t open_bytes[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xec,0xa8,0x02,0,0};
    constexpr std::uint8_t load_bytes[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x83,0xec,0x50};
    constexpr std::uint8_t reset_bytes[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xec,0x98,0,0,0};
    constexpr std::uint8_t visible_bytes[]={0x55,0x48,0x89,0xe5,0x41,0x56,0x53,0x48,0x81,0xec,0x30,0x01,0,0,0x89,0xf3};
    if(!Prepare(menu_hooks[0],image,size,0x1b20900,open_bytes,OpenSectionHook)||!Prepare(menu_hooks[1],image,size,0x14ef730,load_bytes,LoadCameraHook)||
       !Prepare(menu_hooks[2],image,size,0x1b1a2d0,reset_bytes,ConfirmedKeysResetHook)||!Prepare(menu_hooks[3],image,size,0x1acb760,visible_bytes,VisibilityHook)){
     for(auto& hook:menu_hooks)Discard(hook);host_log("pc-options rejected: vanilla section/camera/reset/visibility signatures differ");return false;
    }
    namespace fs=std::filesystem;
    std::vector<std::pair<std::string,std::string>> mounts;
    try {
        const fs::path root=fs::u8path(assets),original=fs::u8path(game);
        auto add=[&](const fs::path& relative) {
            const auto prepared=root/relative;
            const auto path="/app0/"+relative.generic_string();
            char resolved[512]{}; std::error_code ec;
            if (!fs::is_regular_file(prepared)) {host_log("pc-options rejected: prepared asset absent: %s",path.c_str());return false;}
            const int translated=runtime_file_translate(path.c_str(),resolved,sizeof resolved);
            if (translated || !fs::equivalent(fs::u8path(resolved),original/relative,ec) || ec) {
                host_log("pc-options rejected: asset ownership mismatch: %s; translate=%d; resolved=%s",path.c_str(),translated,resolved);
                return false; // A player's own file wins; never bind a foreign movie.
            }
            mounts.emplace_back(path,prepared.string()); return true;
        };
        if (!add("dvdroot_ps4/menu/optionsetting.gfx")) return false;
        for (const auto& item:fs::directory_iterator(original/"dvdroot_ps4/msg")) {
            if (!item.is_directory()) continue;
            const auto relative=fs::path("dvdroot_ps4/msg")/item.path().filename()/"menu.msgbnd.dcx";
            if (fs::is_regular_file(original/relative) && !add(relative)) return false;
        }
        if (mounts.size()<2 || mounts.size()>24) {host_log("pc-options rejected: local asset count=%u (expected 2..24)",unsigned(mounts.size()));return false;}
        const auto language_count=mounts.size()-1;
        if(!ConsolidateMessageMounts(mounts,root)){host_log("pc-options rejected: complete merged message view missing or inconsistent; update launcher and DLL together");return false;}
        host_log("pc-options: %u language bundles share one message-directory mount (%u total mounts)",unsigned(language_count),unsigned(mounts.size()));
    } catch (const std::exception& e) { host_log("pc-options disabled: %s",e.what()); return false; }
    unsigned installed=0;
    for (const auto& [path,file]:mounts) {
        if (runtime_file_mount(path.c_str(),file.c_str())) {
            host_log("pc-options rejected: mount failed: %s",path.c_str());
            for (unsigned i=0;i<installed;++i) runtime_file_unmount(mounts[i].first.c_str());
            return false;
        }
        ++installed;
    }
    g_slide=reinterpret_cast<std::uint64_t>(image);
    MenuMemory::Install(image,size);
    auto& settings=BbSettings::Get();
    input_page=true;
    camera_page=true;
    graphics_page=Graphics::Enabled();
    graphics_toggles[0].setting=&settings.effects[4];graphics_toggles[1].setting=&settings.effects[3];
    graphics_sliders[0].setting=&settings.graphics_ao_strength;graphics_sliders[1].setting=&settings.graphics_shadow_scale;
    effect_toggles[0].setting=&settings.effects[2];effect_toggles[1].setting=&settings.effects[1];
    effect_toggles[2].setting=&settings.effects[0];effect_toggles[3].setting=&settings.graphics_vignette;
    effect_sliders[0].setting=&settings.graphics_bloom;effect_sliders[1].setting=&settings.graphics_saturation;
    camera[0].setting=&settings.camera_fov_scale;
    camera[1].setting=&settings.camera_distance_scale;
    camera[2].setting=&settings.camera_height_scale;
    const auto name=reinterpret_cast<std::uint64_t>(g_open_name);
    std::memcpy(image+table,&name,8); image[imm]=7;
    Commit(menu_hooks);
    g_installed=true;
    host_log("pc-options: vanilla Controls and Environment extended (%u local assets); both input devices remain active",installed);
    return true;
}
bool ClickDecides(std::uint8_t* list, int index) {
    if(screen_alive(vanilla_controls)&&list==vanilla_controls.dialog+kDialogRows&&index==0)return true;
    return g_installed && g_keys.dialog && list == g_keys.rows && index >= 0 && index < kKeysRows &&
           (index==0 || keys_row_bound(index)) && keys_alive();
}
bool KeyBindingsOpen(){return g_keys.dialog && keys_alive();}

void ListUpdate(std::uint8_t* comp, bool focused) {
    if (!g_installed) {
        return;
    }
    auto* c = static_cast<std::uint8_t*>(comp);
    update_parent(vanilla_controls);update_parent(vanilla_environment);
    defaults_view_update(c, focused);
    vanilla_update(c,focused);
    if (!g_keys.dialog) {
        return;
    }
    int row = -1;
    if (c == g_keys.rows) {
        row = -2;
    } else {
        for (int r = 0; r < kKeysRows && row == -1; ++r) {
            if (c == g_keys.widget[r] + kChoiceList) row = r;
        }
    }
    if (row == -1) {
        return;
    }
    if (!keys_alive()) {
        g_keys = KeysScreen{};
        return;
    }
    keys_update(row, focused);
}


void Tick() {
    if (!g_installed) return;
    update_parent(vanilla_controls);update_parent(vanilla_environment);
    bool changed=g_keys_save.exchange(false);
    for (auto& s:camera) {
        if (!seeded) break;
        if (s.value==s.last) continue;
        s.last=std::min<std::uint8_t>(s.value,10);
        *s.setting=s.base+s.last*s.step;
        changed=true;
    }
    auto push_sliders=[&](auto& rows,bool active) {
        if (!active) return;
        for (auto& s:rows) if (s.value!=s.last) { s.last=std::min<std::uint8_t>(s.value,10);*s.setting=s.base+s.last*s.step;changed=true; }
    };
    auto push_toggles=[&](auto& rows,bool active) {
        if (!active) return;
        for (auto& t:rows) if (t.value!=t.last) { t.last=t.value;*t.setting=t.value!=0;changed=true; }
    };
    push_sliders(graphics_sliders,graphics_seeded);push_sliders(effect_sliders,effects_seeded);
    push_toggles(graphics_toggles,graphics_seeded);push_toggles(effect_toggles,effects_seeded);
    if (graphics_seeded && output_resolution!=last_resolution) {
        last_resolution=std::clamp(output_resolution,0,BbSettings::OutputCount-1);
        BbSettings::Get().output_res=last_resolution;changed=true;
    }
    if (changed) { BbSettings::Save(); host_log("pc-options: System option changed; saved to bbport.ini"); }
}
} // namespace BbEngine::Options

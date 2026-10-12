// ============================================================
// AutoLoot for The Slormancer (Aurie + YYToolkit)
//
// Automatically pulls gold, essence and quest items onto the hero.
//
// Settings (saved in AutoLoot.ini, next to the DLL):
//   - before launch: by editing AutoLoot.ini
//   - in game, keyboard: F6 autoloot on/off, F7 / F8 range -/+, F9 equipment
//   - in game, mouse: "M" button in the game's Options menu (if ModMenu.dll is installed)
// ============================================================

#include <YYToolkit/YYTK_Shared.hpp>
#include <ModMenu/ModMenuAPI.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

using namespace Aurie;
using namespace YYTK;

// ============================================================
// Constants
// ============================================================

// The hero hook runs every Step: 6 = about 20 times per second at 120 fps.
static constexpr int STEP_INTERVAL = 6;

// Debug mode: one diagnostic log every N passes.
static constexpr int LOG_INTERVAL = 20;

// Distance (in pixels) below which loot is no longer moved.
static constexpr double STOP_DISTANCE = 24.0;

// Hotkeys (Windows VK_* codes).
static constexpr int KEY_TOGGLE = VK_F6;
static constexpr int KEY_RANGE_DOWN = VK_F7;
static constexpr int KEY_RANGE_UP = VK_F8;
static constexpr int KEY_EQUIPMENT = VK_F9;
static constexpr int KEY_DUMP = VK_HOME;   // Debug: dumps the variables of the nearest obj_loot

// Range presets in pixels. Past the last one: whole room (0).
static constexpr int kRangePresets[] = { 100, 200, 300, 500, 800, 1200 };
static constexpr int kRangePresetCount = 6;

// ============================================================
// Settings (loaded from AutoLoot.ini, editable in game)
// ============================================================

static bool g_Enabled = true;    // autoloot enabled
static int  g_Range = 0;       // range in pixels, 0 = whole room
static bool g_Equipment = false;   // also pull equipment (obj_loot)
static bool g_Slormandrites = true; // pick up slormandrites (obj_loot with a spr_slormandrite_* sprite)
static bool g_ElderSlorm = true;    // pick up the elder slorm (obj_slorm_anomaly, purple orb)
static bool g_Sound = true;    // short beep when a setting is changed with the keyboard
static bool g_Debug = false;   // detailed logs + scr_pick_up_item spy

static char g_ConfigPath[MAX_PATH] = {};

// Last known hero position (set by the hero hook).
static double g_HeroX = 0.0;
static double g_HeroY = 0.0;
static bool g_HeroKnown = false;

// ============================================================
// Globals
// ============================================================

static YYTKInterface* g_Yytk = nullptr;

static PFUNC_YYGMLScript g_OriginalHeroStep = nullptr;   // scr_hero_step
static PFUNC_YYGMLScript g_OriginalPickUp = nullptr;   // scr_pick_up_item (Debug)

static const char* kLootObjects[] = {
    "obj_gold_coin",
    "obj_gold_coin_stacked",
    "obj_essence",
    "obj_quest_item",
    "obj_slorm_anomaly",  // elder slorm (purple orb, sprite spr_elder_slorm): collected on collision with the hero
    "obj_loot"            // slormites, slormandrites, slormelines (autolootable) + equipment (if g_Equipment)
};

// ============================================================
// GameMaker helpers
// ============================================================

// Converts a numeric GameMaker value to a double (through its text form).
static double ReadNumber(RValue Value)
{
    std::string s(Value.ToString());
    return std::atof(s.c_str());
}

// Reads an instance's (x, y) position through GameMaker's built-in variables.
// Avoids depending on CInstance's internal layout (which varies between headers).
static bool GetInstancePosition(CInstance* Inst, double& X, double& Y)
{
    if (!Inst || !g_Yytk)
        return false;

    RValue rx(0.0), ry(0.0);
    if (!AurieSuccess(g_Yytk->GetBuiltin("x", Inst, NULL_INDEX, rx)))
        return false;
    if (!AurieSuccess(g_Yytk->GetBuiltin("y", Inst, NULL_INDEX, ry)))
        return false;

    X = ReadNumber(rx);
    Y = ReadNumber(ry);
    return true;
}

// True if the GameMaker value is truthy (true, 1, or any non-zero number).
static bool IsTrue(RValue Value)
{
    std::string s(Value.ToString());
    if (s == "true" || s == "True") return true;
    return std::atof(s.c_str()) != 0.0;
}

// True if the instance has the variable Name and its value is truthy.
static bool HasTrueFlag(RValue Inst, const char* Name)
{
    RValue exists = g_Yytk->CallBuiltin("variable_instance_exists", { Inst, Name });
    if (!IsTrue(exists))
        return false;

    RValue value = g_Yytk->CallBuiltin("variable_instance_get", { Inst, Name });
    return IsTrue(value);
}

// True if the instance (an obj_loot) is a slormandrite: recognized by its sprite
// (spr_slormandrite_0 ... _5), which does not depend on the game's language.
static bool IsSlormandrite(RValue Inst)
{
    RValue spriteIndex = g_Yytk->CallBuiltin("variable_instance_get", { Inst, "sprite_index" });
    std::string name(g_Yytk->CallBuiltin("sprite_get_name", { spriteIndex }).ToString());
    return name.rfind("spr_slormandrite", 0) == 0;
}

// ============================================================
// Configuration (AutoLoot.ini)
// ============================================================

static void ConfigWriteInt(const char* Key, int Value)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", Value);
    WritePrivateProfileStringA("AutoLoot", Key, buf, g_ConfigPath);
}

static void ConfigCreateDefaultFile()
{
    FILE* f = nullptr;
    if (fopen_s(&f, g_ConfigPath, "w") != 0 || !f)
        return;

    std::fputs(
        "; AutoLoot configuration for The Slormancer\n"
        ";\n"
        "; Enabled   : 1 = autoloot on, 0 = off\n"
        "; Range     : range in pixels. 0 = whole room (e.g. 300 = close to the hero)\n"
        "; Equipment : 1 = also pulls equipment to the hero (you still have to press\n"
        ";             the key to pick it up), 0 = leaves it on the ground.\n"
        ";             Slormites / slormandrites / slormelines flagged autolootable\n"
        ";             by the game are always pulled, whatever this option says.\n"
        "; Slormandrites : 1 = also pulls slormandrites, 0 = leaves them on the ground\n"
        "; ElderSlorm    : 1 = also pulls the elder slorm (purple orb), 0 = leaves it on the ground\n"
        "; Sound     : 1 = short beep when a setting is changed in game\n"
        "; Debug     : 1 = detailed logs in the console\n"
        ";\n"
        "; In-game hotkeys: F6 on/off, F7 range -, F8 range +, F9 equipment\n"
        "; Settings page: M button in the game's Options menu (needs ModMenu.dll)\n"
        "\n"
        "[AutoLoot]\n"
        "Enabled=1\n"
        "Range=0\n"
        "Equipment=0\n"
        "Slormandrites=1\n"
        "ElderSlorm=1\n"
        "Sound=1\n"
        "Debug=0\n",
        f);

    std::fclose(f);
}

static void ConfigLoad()
{
    if (GetFileAttributesA(g_ConfigPath) == INVALID_FILE_ATTRIBUTES)
        ConfigCreateDefaultFile();

    g_Enabled = GetPrivateProfileIntA("AutoLoot", "Enabled", 1, g_ConfigPath) != 0;
    g_Range = static_cast<int>(GetPrivateProfileIntA("AutoLoot", "Range", 0, g_ConfigPath));
    g_Equipment = GetPrivateProfileIntA("AutoLoot", "Equipment", 0, g_ConfigPath) != 0;
    g_Slormandrites = GetPrivateProfileIntA("AutoLoot", "Slormandrites", 1, g_ConfigPath) != 0;
    g_ElderSlorm = GetPrivateProfileIntA("AutoLoot", "ElderSlorm", 1, g_ConfigPath) != 0;
    g_Sound = GetPrivateProfileIntA("AutoLoot", "Sound", 1, g_ConfigPath) != 0;
    g_Debug = GetPrivateProfileIntA("AutoLoot", "Debug", 0, g_ConfigPath) != 0;

    if (g_Range < 0) g_Range = 0;
}

// ============================================================
// In-game settings: range, toggles, hotkeys
// ============================================================

static void DescribeRange(char* Out, size_t Size)
{
    if (g_Range <= 0)
        std::snprintf(Out, Size, "whole room");
    else
        std::snprintf(Out, Size, "%d px", g_Range);
}

static void RangeIncrease()
{
    if (g_Range == 0)
        return;                         // already at the maximum

    for (int i = 0; i < kRangePresetCount; ++i)
    {
        if (kRangePresets[i] > g_Range)
        {
            g_Range = kRangePresets[i];
            return;
        }
    }

    g_Range = 0;                        // past the last preset: whole room
}

static void RangeDecrease()
{
    if (g_Range == 0)
    {
        g_Range = kRangePresets[kRangePresetCount - 1];
        return;
    }

    for (int i = kRangePresetCount - 1; i >= 0; --i)
    {
        if (kRangePresets[i] < g_Range)
        {
            g_Range = kRangePresets[i];
            return;
        }
    }

    g_Range = kRangePresets[0];
}

static void ToggleEnabled()
{
    g_Enabled = !g_Enabled;
    ConfigWriteInt("Enabled", g_Enabled ? 1 : 0);
    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Autoloot: %s", g_Enabled ? "ON" : "OFF");
}

static void ToggleEquipment()
{
    g_Equipment = !g_Equipment;
    ConfigWriteInt("Equipment", g_Equipment ? 1 : 0);
    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Pull equipment: %s", g_Equipment ? "YES" : "NO");
}

static void ToggleSlormandrites()
{
    g_Slormandrites = !g_Slormandrites;
    ConfigWriteInt("Slormandrites", g_Slormandrites ? 1 : 0);
    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Pick up slormandrites: %s", g_Slormandrites ? "YES" : "NO");
}

static void ToggleElderSlorm()
{
    g_ElderSlorm = !g_ElderSlorm;
    ConfigWriteInt("ElderSlorm", g_ElderSlorm ? 1 : 0);
    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Pick up the elder slorm: %s", g_ElderSlorm ? "YES" : "NO");
}

// True if the foreground window belongs to the game (avoids reacting to keys
// typed in another application).
static bool GameHasFocus()
{
    HWND fg = GetForegroundWindow();
    if (!fg)
        return false;

    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

// Detects a key press (rising edge).
static bool KeyPressedOnce(int Vk, bool& WasDown)
{
    const bool down = (GetAsyncKeyState(Vk) & 0x8000) != 0;
    const bool pressed = down && !WasDown;
    WasDown = down;
    return pressed;
}

static void PlayFeedbackSound(bool High)
{
    if (g_Sound)
        MessageBeep(High ? MB_OK : MB_ICONEXCLAMATION);
}

// ------------------------------------------------------------
// Diagnostics (Debug=1): variables of an instance, and of the obj_loot nearest to the hero.
// ------------------------------------------------------------

static void DumpInstance(RValue Inst, const char* Title)
{
    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] ===== DUMP: %s =====", Title);

    RValue names = g_Yytk->CallBuiltin("variable_instance_get_names", { Inst });

    size_t count = 0;
    if (!AurieSuccess(g_Yytk->GetArraySize(names, count)))
    {
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot]   variable list unreadable");
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] ===== END OF DUMP =====");
        return;
    }

    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot]   %d variables", static_cast<int>(count));

    if (count > 150) count = 150;

    for (size_t i = 0; i < count; ++i)
    {
        RValue* entry = nullptr;
        if (!AurieSuccess(g_Yytk->GetArrayEntry(names, i, entry)) || !entry)
            continue;

        std::string varName(entry->ToString());
        RValue value = g_Yytk->CallBuiltin("variable_instance_get", { Inst, varName.c_str() });

        const int kind = static_cast<int>(value.m_Kind);
        // 0 = real, 1 = string, 7 = int32, 10 = int64, 13 = bool
        if (kind == 0 || kind == 1 || kind == 7 || kind == 10 || kind == 13)
        {
            std::string s(value.ToString());
            if (s.size() > 60) s.resize(60);
            DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot]   %s = %s", varName.c_str(), s.c_str());
        }
        else
        {
            DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot]   %s = <type %d>", varName.c_str(), kind);
        }
    }

    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] ===== END OF DUMP =====");
}

// Value of an instance variable as short text ("-" if it does not exist).
static std::string ReadVarText(RValue Inst, const char* Name)
{
    RValue exists = g_Yytk->CallBuiltin("variable_instance_exists", { Inst, Name });
    if (!IsTrue(exists))
        return "-";

    RValue value = g_Yytk->CallBuiltin("variable_instance_get", { Inst, Name });
    const int kind = static_cast<int>(value.m_Kind);
    if (kind == 0 || kind == 1 || kind == 7 || kind == 10 || kind == 13)
    {
        std::string s(value.ToString());
        if (s.size() > 40) s.resize(40);
        return s;
    }
    return "<type " + std::to_string(kind) + ">";
}

// One line per obj_loot in the room: enough to compare equipment / slormandrite / fragment...
static void DumpLootList(RValue ObjAsset, int Count)
{
    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] ----- obj_loot list (distance | name | rarity | autolootable | deny_loot | sprite | variable count) -----");

    const int shown = (Count > 25) ? 25 : Count;
    for (int i = 0; i < shown; ++i)
    {
        RValue inst = g_Yytk->CallBuiltin("instance_find", { ObjAsset, RValue(static_cast<double>(i)) });

        const double lx = ReadNumber(g_Yytk->CallBuiltin("variable_instance_get", { inst, "x" }));
        const double ly = ReadNumber(g_Yytk->CallBuiltin("variable_instance_get", { inst, "y" }));
        const double dist = std::sqrt((lx - g_HeroX) * (lx - g_HeroX) + (ly - g_HeroY) * (ly - g_HeroY));

        RValue spriteIndex = g_Yytk->CallBuiltin("variable_instance_get", { inst, "sprite_index" });
        std::string spriteName(g_Yytk->CallBuiltin("sprite_get_name", { spriteIndex }).ToString());

        RValue names = g_Yytk->CallBuiltin("variable_instance_get_names", { inst });
        size_t varCount = 0;
        if (!AurieSuccess(g_Yytk->GetArraySize(names, varCount)))
            varCount = 0;

        DbgPrintEx(LOG_SEVERITY_INFO, 
            "[AutoLoot]   #%d | %.0f px | %s | %s | auto=%s | deny=%s | %s | %d vars",
            i, dist,
            ReadVarText(inst, "loot_name").c_str(),
            ReadVarText(inst, "rarity").c_str(),
            ReadVarText(inst, "autolootable").c_str(),
            ReadVarText(inst, "deny_loot").c_str(),
            spriteName.c_str(),
            static_cast<int>(varCount));
    }

    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] ----- end of list -----");
}

// Every instance (of any object) around the hero: used to identify which object
// a piece of scenery or unknown loot is made of (e.g. an orb on the ground).
static void DumpNearbyInstances(double Radius)
{
    if (!g_HeroKnown)
        return;

    RValue all(-3.0);   // GameMaker "all" constant
    const int n = static_cast<int>(ReadNumber(g_Yytk->CallBuiltin("instance_number", { all })));
    const int limit = (n > 6000) ? 6000 : n;

    struct Near { double dist; std::string object; std::string sprite; };
    std::vector<Near> found;

    for (int i = 0; i < limit; ++i)
    {
        RValue inst = g_Yytk->CallBuiltin("instance_find", { all, RValue(static_cast<double>(i)) });

        const double lx = ReadNumber(g_Yytk->CallBuiltin("variable_instance_get", { inst, "x" }));
        const double ly = ReadNumber(g_Yytk->CallBuiltin("variable_instance_get", { inst, "y" }));
        const double dist = std::sqrt((lx - g_HeroX) * (lx - g_HeroX) + (ly - g_HeroY) * (ly - g_HeroY));
        if (dist > Radius)
            continue;

        RValue objectIndex = g_Yytk->CallBuiltin("variable_instance_get", { inst, "object_index" });
        RValue spriteIndex = g_Yytk->CallBuiltin("variable_instance_get", { inst, "sprite_index" });

        Near item;
        item.dist = dist;
        item.object = std::string(g_Yytk->CallBuiltin("object_get_name", { objectIndex }).ToString());
        item.sprite = std::string(g_Yytk->CallBuiltin("sprite_get_name", { spriteIndex }).ToString());
        found.push_back(item);
    }

    std::sort(found.begin(), found.end(), [](const Near& a, const Near& b) { return a.dist < b.dist; });

    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] ----- instances within %.0f px of the hero: %d (out of %d in the room) -----",
        Radius, static_cast<int>(found.size()), n);

    const size_t shown = (found.size() > 40) ? 40 : found.size();
    for (size_t i = 0; i < shown; ++i)
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot]   %.0f px | %s | %s",
            found[i].dist, found[i].object.c_str(), found[i].sprite.c_str());

    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] ----- end of nearby instances -----");
}

// Variables of the instance nearest to the hero for a given object (if there is one).
static void DumpNearestOfObject(const char* ObjName)
{
    if (!g_HeroKnown)
        return;

    RValue obj = g_Yytk->CallBuiltin("asset_get_index", { ObjName });
    const int n = static_cast<int>(ReadNumber(g_Yytk->CallBuiltin("instance_number", { obj })));
    if (n < 1)
        return;

    double bestD2 = 1e30;
    int bestIndex = 0;

    for (int i = 0; i < n; ++i)
    {
        RValue inst = g_Yytk->CallBuiltin("instance_find", { obj, RValue(static_cast<double>(i)) });
        const double lx = ReadNumber(g_Yytk->CallBuiltin("variable_instance_get", { inst, "x" }));
        const double ly = ReadNumber(g_Yytk->CallBuiltin("variable_instance_get", { inst, "y" }));
        const double d2 = (lx - g_HeroX) * (lx - g_HeroX) + (ly - g_HeroY) * (ly - g_HeroY);
        if (d2 < bestD2)
        {
            bestD2 = d2;
            bestIndex = i;
        }
    }

    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] %s: %d in the room, the nearest is %.0f px away", ObjName, n, std::sqrt(bestD2));

    RValue best = g_Yytk->CallBuiltin("instance_find", { obj, RValue(static_cast<double>(bestIndex)) });
    DumpInstance(best, ObjName);
}

static void DumpNearestLoot()
{
    if (!g_HeroKnown)
    {
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] diagnostics: hero position unknown");
        return;
    }

    RValue obj = g_Yytk->CallBuiltin("asset_get_index", { "obj_loot" });
    RValue count = g_Yytk->CallBuiltin("instance_number", { obj });
    const int n = static_cast<int>(ReadNumber(count));

    if (n < 1)
    {
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] diagnostics: no obj_loot in the room");
        return;
    }

    double bestD2 = 1e30;
    int bestIndex = -1;

    for (int i = 0; i < n; ++i)
    {
        RValue inst = g_Yytk->CallBuiltin("instance_find", { obj, RValue(static_cast<double>(i)) });
        const double lx = ReadNumber(g_Yytk->CallBuiltin("variable_instance_get", { inst, "x" }));
        const double ly = ReadNumber(g_Yytk->CallBuiltin("variable_instance_get", { inst, "y" }));
        const double d2 = (lx - g_HeroX) * (lx - g_HeroX) + (ly - g_HeroY) * (ly - g_HeroY);

        if (d2 < bestD2)
        {
            bestD2 = d2;
            bestIndex = i;
        }
    }

    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] diagnostics: %d obj_loot in the room, the nearest is %.0f px away", n, std::sqrt(bestD2));

    DumpLootList(obj, n);

    RValue best = g_Yytk->CallBuiltin("instance_find", { obj, RValue(static_cast<double>(bestIndex)) });
    DumpInstance(best, "obj_loot nearest to the hero");
}

static void HandleHotkeys()
{
    static bool wasToggle = false, wasDown = false, wasUp = false, wasEquip = false, wasDump = false;

    const bool pToggle = KeyPressedOnce(KEY_TOGGLE, wasToggle);
    const bool pDown = KeyPressedOnce(KEY_RANGE_DOWN, wasDown);
    const bool pUp = KeyPressedOnce(KEY_RANGE_UP, wasUp);
    const bool pEquip = KeyPressedOnce(KEY_EQUIPMENT, wasEquip);
    const bool pDump = KeyPressedOnce(KEY_DUMP, wasDump);

    if (!GameHasFocus())
        return;

    if (pDump && g_Debug)
    {
        DumpNearbyInstances(300.0);
        DumpNearestOfObject("obj_slorm_anomaly");
        DumpNearestLoot();
    }

    char rangeText[32];

    if (pToggle)
    {
        g_Enabled = !g_Enabled;
        ConfigWriteInt("Enabled", g_Enabled ? 1 : 0);
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Autoloot: %s", g_Enabled ? "ON" : "OFF");
        PlayFeedbackSound(g_Enabled);
    }

    if (pDown)
    {
        RangeDecrease();
        ConfigWriteInt("Range", g_Range);
        DescribeRange(rangeText, sizeof(rangeText));
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Range: %s", rangeText);
        PlayFeedbackSound(true);
    }

    if (pUp)
    {
        RangeIncrease();
        ConfigWriteInt("Range", g_Range);
        DescribeRange(rangeText, sizeof(rangeText));
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Range: %s", rangeText);
        PlayFeedbackSound(true);
    }

    if (pEquip)
    {
        g_Equipment = !g_Equipment;
        ConfigWriteInt("Equipment", g_Equipment ? 1 : 0);
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Pull equipment: %s", g_Equipment ? "YES" : "NO");
        PlayFeedbackSound(g_Equipment);
    }
}

// ============================================================
// scr_hero_step hook: Self = the hero. This is where loot is pulled in.
// ============================================================

RValue& HeroStepHook(
    IN CInstance* Self,
    IN CInstance* Other,
    OUT RValue& Result,
    IN int ArgumentCount,
    IN RValue* Arguments[]
)
{
    // Let the game run its Step normally first.
    RValue& Ret = g_OriginalHeroStep(Self, Other, Result, ArgumentCount, Arguments);

    double heroX = 0.0, heroY = 0.0;
    const bool heroOk = GetInstancePosition(Self, heroX, heroY);
    if (heroOk)
    {
        g_HeroX = heroX;
        g_HeroY = heroY;
        g_HeroKnown = true;
    }

    HandleHotkeys();

    static int tick = 0;
    ++tick;

    if (g_Debug && tick == 1)
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] first scr_hero_step call");

    if (!g_Enabled || !heroOk || (tick % STEP_INTERVAL) != 0)
        return Ret;

    static int logTick = 0;
    const bool doLog = g_Debug && (++logTick % LOG_INTERVAL == 0);

    RValue hx(heroX);
    RValue hy(heroY);

    const double rangeSq = (g_Range > 0) ? static_cast<double>(g_Range) * g_Range : 0.0;

    for (const char* name : kLootObjects)
    {
        // obj_loot covers equipment AND materials (slormites, slormandrites, slormelines).
        // The game itself flags the items it picks up on its own with autolootable = 1:
        // those are always pulled. The others (equipment) only if the option is on.
        const bool isLootObject = (std::strcmp(name, "obj_loot") == 0);

        // Elder slorm disabled in the settings: leave it alone.
        if (!g_ElderSlorm && std::strcmp(name, "obj_slorm_anomaly") == 0)
            continue;

        RValue obj = g_Yytk->CallBuiltin("asset_get_index", { name });
        RValue numValue = g_Yytk->CallBuiltin("instance_number", { obj });

        std::string numStr(numValue.ToString());
        int n = std::atoi(numStr.c_str());

        int moved = 0, denied = 0, busy = 0, tooFar = 0, notAuto = 0, filtered = 0;

        // Backwards: a pickup can destroy an instance along the way.
        for (int i = n - 1; i >= 0; --i)
        {
            RValue idx(static_cast<double>(i));
            RValue inst = g_Yytk->CallBuiltin("instance_find", { obj, idx });

            // obj_loot not flagged autolootable: only pulled if the equipment option is on.
            if (isLootObject && !g_Equipment && !HasTrueFlag(inst, "autolootable"))
            {
                ++notAuto;
                continue;
            }

            // Slormandrites disabled in the settings: leave them on the ground.
            if (isLootObject && !g_Slormandrites && IsSlormandrite(inst))
            {
                ++filtered;
                continue;
            }

            // Loot position relative to the hero.
            double lx = ReadNumber(g_Yytk->CallBuiltin("variable_instance_get", { inst, "x" }));
            double ly = ReadNumber(g_Yytk->CallBuiltin("variable_instance_get", { inst, "y" }));
            double dx = lx - heroX;
            double dy = ly - heroY;
            double d2 = dx * dx + dy * dy;

            // Already close to the hero: hands off, the game finishes the pickup.
            if (d2 <= STOP_DISTANCE * STOP_DISTANCE)
                continue;

            // Outside the chosen range.
            if (rangeSq > 0.0 && d2 > rangeSq)
            {
                ++tooFar;
                continue;
            }

            // Item the game says cannot be picked up.
            if (HasTrueFlag(inst, "deny_loot"))
            {
                ++denied;
                continue;
            }

            // Item in the middle of an animation (drop or pickup): let the game handle it.
            if (HasTrueFlag(inst, "drop_animation") || HasTrueFlag(inst, "pick_up_animation"))
            {
                ++busy;
                continue;
            }

            g_Yytk->CallBuiltin("variable_instance_set", { inst, "x", hx });
            g_Yytk->CallBuiltin("variable_instance_set", { inst, "y", hy });
            ++moved;
        }

        if (doLog && n > 0)
            DbgPrintEx(LOG_SEVERITY_INFO, 
                "[AutoLoot] %s: %d instances, %d moved, %d out of range, %d animating, %d deny_loot, %d not autolootable, %d filtered",
                name, n, moved, tooFar, busy, denied, notAuto, filtered);
    }

    if (doLog)
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] hero=(%.0f, %.0f)", heroX, heroY);

    return Ret;
}

// ============================================================
// scr_pick_up_item spy (Debug mode only): prints what the game passes
// when the player picks up an item. Changes nothing. Useful for a future equipment autoloot.
// ============================================================

// Object name of an instance, read through GameMaker (object_index then object_get_name).
static std::string InstanceObjectName(CInstance* Inst)
{
    if (!Inst || !g_Yytk)
        return "<null>";

    RValue index(0.0);
    if (!AurieSuccess(g_Yytk->GetBuiltin("object_index", Inst, NULL_INDEX, index)))
        return "<?>";

    RValue name = g_Yytk->CallBuiltin("object_get_name", { index });
    return std::string(name.ToString());
}

RValue& PickUpHook(
    IN CInstance* Self,
    IN CInstance* Other,
    OUT RValue& Result,
    IN int ArgumentCount,
    IN RValue* Arguments[]
)
{
    static int printed = 0;
    static std::string last;

    if (printed < 60)
    {
        std::string line = "scr_pick_up_item: self=";
        line += InstanceObjectName(Self);
        line += " other=";
        line += InstanceObjectName(Other);
        line += " args=" + std::to_string(ArgumentCount);

        for (int i = 0; i < ArgumentCount && i < 8; ++i)
        {
            RValue* a = Arguments[i];
            if (!a)
            {
                line += " [null]";
                continue;
            }

            const int kind = static_cast<int>(a->m_Kind);
            // 0 = real, 1 = string, 7 = int32, 10 = int64, 13 = bool
            if (kind == 0 || kind == 1 || kind == 7 || kind == 10 || kind == 13)
            {
                std::string s(a->ToString());
                if (s.size() > 40) s.resize(40);
                line += " [" + std::to_string(kind) + ":" + s + "]";
            }
            else
            {
                line += " [type " + std::to_string(kind) + "]";
            }
        }

        // Never print the exact same line twice in a row.
        if (line != last)
        {
            last = line;
            ++printed;
            DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] %s", line.c_str());
        }
    }

    return g_OriginalPickUp(Self, Other, Result, ArgumentCount, Arguments);
}

// ============================================================
// Settings page in the mods menu ("M" button of the Options menu).
//
// The menu itself lives in ModMenu.dll: we register with it if it is installed. Without it,
// autoloot still works, with AutoLoot.ini and the hotkeys.
// ============================================================

static bool MenuIsActive(void*)
{
    return g_Enabled;
}

// Once per frame: hotkeys also work while the hero is not stepping (menus).
static void MenuOnFrame(void*)
{
    HandleHotkeys();
}

static void MenuDrawPage(ModMenu::IModMenu* Menu, void*)
{
    // --- Left column ---
    if (Menu->Toggle(0, 0, "Autoloot enabled", g_Enabled))
        ToggleEnabled();

    char rangeText[32];
    DescribeRange(rangeText, sizeof(rangeText));

    const int step = Menu->Selector(0, 1, "Range", rangeText);
    if (step != 0)
    {
        if (step < 0) RangeDecrease(); else RangeIncrease();
        ConfigWriteInt("Range", g_Range);
        DescribeRange(rangeText, sizeof(rangeText));
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Range: %s", rangeText);
    }

    if (Menu->Toggle(0, 2, "Pull equipment", g_Equipment))
        ToggleEquipment();

    // --- Right column ---
    if (Menu->Toggle(1, 0, "Pick up slormandrites", g_Slormandrites))
        ToggleSlormandrites();

    if (Menu->Toggle(1, 1, "Pick up the elder slorm", g_ElderSlorm))
        ToggleElderSlorm();
}

// Returns true if ModMenu.dll is installed and accepted our page.
static bool RegisterInModMenu()
{
    AurieInterfaceBase* base = nullptr;
    if (!AurieSuccess(ObGetInterface(ModMenu::kInterfaceName, base)) || !base)
        return false;

    ModMenu::ModEntry entry = {};
    entry.Name = "AUTOLOOT";
    entry.Title = "MOD  -  AUTOLOOT";
    entry.Help = "In game: F6 on/off  -  F7 / F8 range  -  F9 equipment  -  Esc: back";
    entry.IsActive = MenuIsActive;
    entry.DrawPage = MenuDrawPage;
    entry.OnFrame = MenuOnFrame;

    return static_cast<ModMenu::IModMenu*>(base)->RegisterMod(entry);
}

// ============================================================
// Module initialization
// ============================================================

EXPORTED AurieStatus ModuleInitialize(
    IN AurieModule* Module,
    IN const fs::path& ModulePath
)
{
    UNREFERENCED_PARAMETER(ModulePath);

    g_Yytk = YYTK::GetInterface();

    if (!g_Yytk)
    {
        OutputDebugStringA("[AutoLoot] ERROR: YYToolkit interface not found\n");
        return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;
    }

    // --- Configuration file: AutoLoot.ini next to the DLL ---
    HMODULE hMod = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&ModuleInitialize),
        &hMod
    );

    char dllPath[MAX_PATH] = {};
    GetModuleFileNameA(hMod, dllPath, MAX_PATH);

    std::string iniPath(dllPath);
    const size_t slash = iniPath.find_last_of("\\/");
    iniPath = (slash == std::string::npos ? std::string() : iniPath.substr(0, slash + 1)) + "AutoLoot.ini";
    strncpy_s(g_ConfigPath, iniPath.c_str(), _TRUNCATE);

    ConfigLoad();

    // --- Installs a hook on a GameMaker script ---
    auto installScriptHook = [&](const char* scriptName, const char* hookName,
        PVOID hookFunction, PFUNC_YYGMLScript& original) -> bool
        {
            CScript* script = nullptr;
            AurieStatus st = g_Yytk->GetNamedRoutinePointer(scriptName, reinterpret_cast<PVOID*>(&script));

            if (!AurieSuccess(st) || !script || !script->m_Functions || !script->m_Functions->m_ScriptFunction)
                return false;

            st = MmCreateHook(
                Module,
                hookName,
                reinterpret_cast<PVOID>(script->m_Functions->m_ScriptFunction),
                hookFunction,
                nullptr
            );

            if (!AurieSuccess(st))
                return false;

            original = reinterpret_cast<PFUNC_YYGMLScript>(MmGetHookTrampoline(Module, hookName));
            return original != nullptr;
        };

    // --- The autoloot itself: required ---
    if (!installScriptHook("gml_Script_scr_hero_step", "HeroStep",
        reinterpret_cast<PVOID>(HeroStepHook), g_OriginalHeroStep))
    {
        DbgPrintEx(LOG_SEVERITY_ERROR, "[AutoLoot] could not install the scr_hero_step hook");
        return AURIE_OBJECT_NOT_FOUND;
    }

    // --- Settings page in the mods menu: optional, autoloot works without it ---
    const bool okMenu = RegisterInModMenu();

    // --- scr_pick_up_item spy (Debug mode only) ---
    if (g_Debug)
    {
        const bool okPick = installScriptHook("gml_Script_scr_pick_up_item", "PickUp",
            reinterpret_cast<PVOID>(PickUpHook), g_OriginalPickUp);
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] scr_pick_up_item spy: %s", okPick ? "installed" : "unavailable");
    }

    char rangeText[32];
    DescribeRange(rangeText, sizeof(rangeText));

    DbgPrintEx(LOG_SEVERITY_INFO, 
        "[AutoLoot] ready: autoloot %s, range %s, equipment %s, slormandrites %s, elder slorm %s",
        g_Enabled ? "ON" : "OFF", rangeText, g_Equipment ? "YES" : "NO",
        g_Slormandrites ? "YES" : "NO", g_ElderSlorm ? "YES" : "NO");
    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] hotkeys: F6 on/off, F7/F8 range -/+, F9 equipment");
    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] mods menu (ModMenu.dll): %s",
        okMenu ? "settings page added" : "not installed (hotkeys only)");

    return AURIE_SUCCESS;
}

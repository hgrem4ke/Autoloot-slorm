// ============================================================
// AutoLoot pour The Slormancer (Aurie + YYToolkit)
//
// Ramene automatiquement l'or, l'essence et les objets de quete sur le heros.
//
// Reglages (sauvegardes dans AutoLoot.ini, a cote de la DLL) :
//   - avant le lancement : en editant AutoLoot.ini
//   - en jeu, au clavier :  F6 autoloot on/off, F7 / F8 portee -/+, F9 equipement
//   - en jeu, a la souris : bouton "M" dans le menu Options du jeu (si ModMenu.dll est installe)
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
// Constantes
// ============================================================

// Le hook du heros est appele a chaque Step : 6 = environ 20 fois par seconde a 120 fps.
static constexpr int STEP_INTERVAL = 6;

// Mode Debug : un log de diagnostic toutes les N passes.
static constexpr int LOG_INTERVAL = 20;

// Distance (en pixels) en dessous de laquelle on ne deplace plus le loot.
static constexpr double STOP_DISTANCE = 24.0;

// Raccourcis clavier (codes Windows VK_*).
static constexpr int KEY_TOGGLE = VK_F6;
static constexpr int KEY_RANGE_DOWN = VK_F7;
static constexpr int KEY_RANGE_UP = VK_F8;
static constexpr int KEY_EQUIPMENT = VK_F9;
static constexpr int KEY_DUMP = VK_HOME;   // Debug : affiche les variables du obj_loot le plus proche

// Paliers de portee en pixels. Au-dela du dernier : toute la salle (0).
static constexpr int kRangePresets[] = { 100, 200, 300, 500, 800, 1200 };
static constexpr int kRangePresetCount = 6;

// ============================================================
// Reglages (charges depuis AutoLoot.ini, modifiables en jeu)
// ============================================================

static bool g_Enabled = true;    // autoloot actif
static int  g_Range = 0;       // portee en pixels, 0 = toute la salle
static bool g_Equipment = false;   // attirer aussi l'equipement (obj_loot)
static bool g_Slormandrites = true; // ramasser les slormandrites (obj_loot au sprite spr_slormandrite_*)
static bool g_ElderSlorm = true;    // ramasser le slorm ancien (obj_slorm_anomaly, orbe violet)
static bool g_Sound = true;    // petit bip quand on change un reglage au clavier
static bool g_Debug = false;   // logs detailles + espion scr_pick_up_item

static char g_ConfigPath[MAX_PATH] = {};

// Derniere position connue du heros (renseignee par le hook du heros).
static double g_HeroX = 0.0;
static double g_HeroY = 0.0;
static bool g_HeroKnown = false;

// ============================================================
// Globales
// ============================================================

static YYTKInterface* g_Yytk = nullptr;

static PFUNC_YYGMLScript g_OriginalHeroStep = nullptr;   // scr_hero_step
static PFUNC_YYGMLScript g_OriginalPickUp = nullptr;   // scr_pick_up_item (Debug)

static const char* kLootObjects[] = {
    "obj_gold_coin",
    "obj_gold_coin_stacked",
    "obj_essence",
    "obj_quest_item",
    "obj_slorm_anomaly",  // slorm ancien (orbe violet, sprite spr_elder_slorm) : collecte par collision avec le heros
    "obj_loot"            // slormites, slormandrites, slormelines (autolootable) + equipement (si g_Equipment)
};

// ============================================================
// Utilitaires GameMaker
// ============================================================

// Convertit une valeur GameMaker numerique en double (via sa representation texte).
static double ReadNumber(RValue Value)
{
    std::string s(Value.ToString());
    return std::atof(s.c_str());
}

// Lit la position (x, y) d'une instance via les variables integrees de GameMaker.
// Evite de dependre de la structure interne de CInstance (qui varie selon les en-tetes).
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

// Vrai si la valeur GameMaker est "vraie" (true, 1, ou tout nombre non nul).
static bool IsTrue(RValue Value)
{
    std::string s(Value.ToString());
    if (s == "true" || s == "True") return true;
    return std::atof(s.c_str()) != 0.0;
}

// Vrai si l'instance possede la variable Name et que sa valeur est vraie.
static bool HasTrueFlag(RValue Inst, const char* Name)
{
    RValue exists = g_Yytk->CallBuiltin("variable_instance_exists", { Inst, Name });
    if (!IsTrue(exists))
        return false;

    RValue value = g_Yytk->CallBuiltin("variable_instance_get", { Inst, Name });
    return IsTrue(value);
}

// Vrai si l'instance (un obj_loot) est une slormandrite : reconnue a son sprite
// (spr_slormandrite_0 ... _5), ce qui ne depend pas de la langue du jeu.
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
        "; Configuration de l'AutoLoot pour The Slormancer\n"
        ";\n"
        "; Enabled   : 1 = autoloot actif, 0 = desactive\n"
        "; Range     : portee en pixels. 0 = toute la salle (ex : 300 = proche du heros)\n"
        "; Equipment : 1 = attire aussi l'equipement vers le heros (il faut toujours\n"
        ";             appuyer sur la touche pour le ramasser), 0 = laisse au sol.\n"
        ";             Les slormites / slormandrites / slormelines marques autolootable\n"
        ";             par le jeu sont toujours ramenes, quelle que soit cette option.\n"
        "; Slormandrites : 1 = ramene aussi les slormandrites, 0 = les laisse au sol\n"
        "; ElderSlorm    : 1 = ramene aussi le slorm ancien (orbe violet), 0 = le laisse au sol\n"
        "; Sound     : 1 = petit bip quand on change un reglage en jeu\n"
        "; Debug     : 1 = logs detailles dans la fenetre YYToolkit\n"
        ";\n"
        "; Raccourcis en jeu : F6 on/off, F7 portee -, F8 portee +, F9 equipement\n"
        "; Page de reglages : bouton M dans le menu Options du jeu\n"
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
// Reglages en jeu : portee, bascules, raccourcis clavier
// ============================================================

static void DescribeRange(char* Out, size_t Size)
{
    if (g_Range <= 0)
        std::snprintf(Out, Size, "toute la salle");
    else
        std::snprintf(Out, Size, "%d px", g_Range);
}

static void RangeIncrease()
{
    if (g_Range == 0)
        return;                         // deja au maximum

    for (int i = 0; i < kRangePresetCount; ++i)
    {
        if (kRangePresets[i] > g_Range)
        {
            g_Range = kRangePresets[i];
            return;
        }
    }

    g_Range = 0;                        // au-dela du dernier palier : toute la salle
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
    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Autoloot : %s", g_Enabled ? "ACTIVE" : "DESACTIVE");
}

static void ToggleEquipment()
{
    g_Equipment = !g_Equipment;
    ConfigWriteInt("Equipment", g_Equipment ? 1 : 0);
    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Attirer l'equipement : %s", g_Equipment ? "OUI" : "NON");
}

static void ToggleSlormandrites()
{
    g_Slormandrites = !g_Slormandrites;
    ConfigWriteInt("Slormandrites", g_Slormandrites ? 1 : 0);
    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Ramasser les slormandrites : %s", g_Slormandrites ? "OUI" : "NON");
}

static void ToggleElderSlorm()
{
    g_ElderSlorm = !g_ElderSlorm;
    ConfigWriteInt("ElderSlorm", g_ElderSlorm ? 1 : 0);
    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Ramasser le slorm ancien : %s", g_ElderSlorm ? "OUI" : "NON");
}

// Vrai si la fenetre au premier plan appartient au jeu (evite de reagir aux touches
// tapees dans une autre application).
static bool GameHasFocus()
{
    HWND fg = GetForegroundWindow();
    if (!fg)
        return false;

    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

// Detecte l'appui (front montant) sur une touche.
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
// Diagnostic (Debug=1) : variables d'une instance, et du obj_loot le plus proche du heros.
// ------------------------------------------------------------

static void DumpInstance(RValue Inst, const char* Title)
{
    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] ===== DUMP : %s =====", Title);

    RValue names = g_Yytk->CallBuiltin("variable_instance_get_names", { Inst });

    size_t count = 0;
    if (!AurieSuccess(g_Yytk->GetArraySize(names, count)))
    {
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot]   liste des variables illisible");
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] ===== FIN DUMP =====");
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
        // 0 = reel, 1 = texte, 7 = int32, 10 = int64, 13 = booleen
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

    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] ===== FIN DUMP =====");
}

// Valeur d'une variable d'instance sous forme de texte court ("-" si elle n'existe pas).
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

// Une ligne par obj_loot de la salle : de quoi comparer equipement / slormandrite / fragment...
static void DumpLootList(RValue ObjAsset, int Count)
{
    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] ----- liste des obj_loot (distance | nom | rarete | autolootable | deny_loot | sprite | nb variables) -----");

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

    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] ----- fin de liste -----");
}

// Toutes les instances (tous objets confondus) autour du heros : sert a identifier de quel objet
// est fait un element du decor ou du loot qu'on ne connait pas encore (ex : un orbe au sol).
static void DumpNearbyInstances(double Radius)
{
    if (!g_HeroKnown)
        return;

    RValue all(-3.0);   // constante GameMaker "all"
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

    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] ----- instances a moins de %.0f px du heros : %d (sur %d dans la salle) -----",
        Radius, static_cast<int>(found.size()), n);

    const size_t shown = (found.size() > 40) ? 40 : found.size();
    for (size_t i = 0; i < shown; ++i)
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot]   %.0f px | %s | %s",
            found[i].dist, found[i].object.c_str(), found[i].sprite.c_str());

    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] ----- fin des instances proches -----");
}

// Variables de l'instance la plus proche du heros pour un objet donne (s'il y en a une).
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

    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] %s : %d dans la salle, le plus proche est a %.0f px", ObjName, n, std::sqrt(bestD2));

    RValue best = g_Yytk->CallBuiltin("instance_find", { obj, RValue(static_cast<double>(bestIndex)) });
    DumpInstance(best, ObjName);
}

static void DumpNearestLoot()
{
    if (!g_HeroKnown)
    {
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] diagnostic : position du heros inconnue");
        return;
    }

    RValue obj = g_Yytk->CallBuiltin("asset_get_index", { "obj_loot" });
    RValue count = g_Yytk->CallBuiltin("instance_number", { obj });
    const int n = static_cast<int>(ReadNumber(count));

    if (n < 1)
    {
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] diagnostic : aucun obj_loot dans la salle");
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

    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] diagnostic : %d obj_loot dans la salle, le plus proche est a %.0f px", n, std::sqrt(bestD2));

    DumpLootList(obj, n);

    RValue best = g_Yytk->CallBuiltin("instance_find", { obj, RValue(static_cast<double>(bestIndex)) });
    DumpInstance(best, "obj_loot le plus proche du heros");
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
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Autoloot : %s", g_Enabled ? "ACTIVE" : "DESACTIVE");
        PlayFeedbackSound(g_Enabled);
    }

    if (pDown)
    {
        RangeDecrease();
        ConfigWriteInt("Range", g_Range);
        DescribeRange(rangeText, sizeof(rangeText));
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Portee : %s", rangeText);
        PlayFeedbackSound(true);
    }

    if (pUp)
    {
        RangeIncrease();
        ConfigWriteInt("Range", g_Range);
        DescribeRange(rangeText, sizeof(rangeText));
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Portee : %s", rangeText);
        PlayFeedbackSound(true);
    }

    if (pEquip)
    {
        g_Equipment = !g_Equipment;
        ConfigWriteInt("Equipment", g_Equipment ? 1 : 0);
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Attirer l'equipement : %s", g_Equipment ? "OUI" : "NON");
        PlayFeedbackSound(g_Equipment);
    }
}

// ============================================================
// Hook de scr_hero_step : Self = le heros. C'est ici que le loot est ramene.
// ============================================================

RValue& HeroStepHook(
    IN CInstance* Self,
    IN CInstance* Other,
    OUT RValue& Result,
    IN int ArgumentCount,
    IN RValue* Arguments[]
)
{
    // On laisse d'abord le jeu faire son Step normalement.
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
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] premier appel de scr_hero_step");

    if (!g_Enabled || !heroOk || (tick % STEP_INTERVAL) != 0)
        return Ret;

    static int logTick = 0;
    const bool doLog = g_Debug && (++logTick % LOG_INTERVAL == 0);

    RValue hx(heroX);
    RValue hy(heroY);

    const double rangeSq = (g_Range > 0) ? static_cast<double>(g_Range) * g_Range : 0.0;

    for (const char* name : kLootObjects)
    {
        // obj_loot regroupe l'equipement ET les materiaux (slormites, slormandrites, slormelines).
        // Le jeu marque lui-meme les objets qu'il ramasse tout seul avec autolootable = 1 :
        // ceux-la sont toujours ramenes. Les autres (equipement) seulement si l'option est activee.
        const bool isLootObject = (std::strcmp(name, "obj_loot") == 0);

        // Slorm ancien desactive dans les reglages : on le laisse tranquille.
        if (!g_ElderSlorm && std::strcmp(name, "obj_slorm_anomaly") == 0)
            continue;

        RValue obj = g_Yytk->CallBuiltin("asset_get_index", { name });
        RValue numValue = g_Yytk->CallBuiltin("instance_number", { obj });

        std::string numStr(numValue.ToString());
        int n = std::atoi(numStr.c_str());

        int moved = 0, denied = 0, busy = 0, tooFar = 0, notAuto = 0, filtered = 0;

        // A l'envers : un ramassage peut detruire une instance en cours de route.
        for (int i = n - 1; i >= 0; --i)
        {
            RValue idx(static_cast<double>(i));
            RValue inst = g_Yytk->CallBuiltin("instance_find", { obj, idx });

            // obj_loot non marque autolootable : on ne le ramene que si l'option equipement est active.
            if (isLootObject && !g_Equipment && !HasTrueFlag(inst, "autolootable"))
            {
                ++notAuto;
                continue;
            }

            // Slormandrites desactivees dans les reglages : on les laisse au sol.
            if (isLootObject && !g_Slormandrites && IsSlormandrite(inst))
            {
                ++filtered;
                continue;
            }

            // Position du loot par rapport au heros.
            double lx = ReadNumber(g_Yytk->CallBuiltin("variable_instance_get", { inst, "x" }));
            double ly = ReadNumber(g_Yytk->CallBuiltin("variable_instance_get", { inst, "y" }));
            double dx = lx - heroX;
            double dy = ly - heroY;
            double d2 = dx * dx + dy * dy;

            // Deja pres du heros : on ne touche plus, le jeu finit son ramassage.
            if (d2 <= STOP_DISTANCE * STOP_DISTANCE)
                continue;

            // Hors de la portee choisie.
            if (rangeSq > 0.0 && d2 > rangeSq)
            {
                ++tooFar;
                continue;
            }

            // Objet non ramassable selon le jeu.
            if (HasTrueFlag(inst, "deny_loot"))
            {
                ++denied;
                continue;
            }

            // Objet en pleine animation (chute ou ramassage) : on laisse le jeu faire.
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
                "[AutoLoot] %s : %d instances, %d deplacees, %d hors portee, %d en animation, %d deny_loot, %d non autolootable, %d filtrees",
                name, n, moved, tooFar, busy, denied, notAuto, filtered);
    }

    if (doLog)
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] hero=(%.0f, %.0f)", heroX, heroY);

    return Ret;
}

// ============================================================
// Espion sur scr_pick_up_item (mode Debug uniquement) : affiche ce que le jeu envoie
// quand le joueur ramasse un objet. N'altere rien. Utile pour un futur autoloot d'equipement.
// ============================================================

// Nom de l'objet d'une instance, lu via GameMaker (object_index puis object_get_name).
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
        std::string line = "scr_pick_up_item : self=";
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
            // 0 = reel, 1 = texte, 7 = int32, 10 = int64, 13 = booleen
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

        // On n'affiche pas deux fois de suite exactement la meme ligne.
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
// Page de reglages dans le menu des mods (bouton "M" du menu Options).
//
// Le menu lui-meme est dans ModMenu.dll : on s'y enregistre s'il est installe. Sans lui,
// l'autoloot fonctionne quand meme, avec AutoLoot.ini et les raccourcis clavier.
// ============================================================

// Textes avec accents (UTF-8 explicite pour ne pas dependre de l'encodage du fichier source).
#define U8_E_ACUTE "\xC3\xA9"
static const char* kTxtRange = "Port" U8_E_ACUTE "e";
static const char* kTxtEquipment = "Attirer l'" U8_E_ACUTE "quipement";

static bool MenuIsActive(void*)
{
    return g_Enabled;
}

// Une fois par image : les raccourcis clavier marchent aussi quand le heros ne bouge pas (menus).
static void MenuOnFrame(void*)
{
    HandleHotkeys();
}

static void MenuDrawPage(ModMenu::IModMenu* Menu, void*)
{
    // --- Colonne gauche ---
    if (Menu->Toggle(0, 0, "Autoloot actif", g_Enabled))
        ToggleEnabled();

    char rangeText[32];
    DescribeRange(rangeText, sizeof(rangeText));

    const int step = Menu->Selector(0, 1, kTxtRange, rangeText);
    if (step != 0)
    {
        if (step < 0) RangeDecrease(); else RangeIncrease();
        ConfigWriteInt("Range", g_Range);
        DescribeRange(rangeText, sizeof(rangeText));
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] Portee : %s", rangeText);
    }

    if (Menu->Toggle(0, 2, kTxtEquipment, g_Equipment))
        ToggleEquipment();

    // --- Colonne droite ---
    if (Menu->Toggle(1, 0, "Ramasser les slormandrites", g_Slormandrites))
        ToggleSlormandrites();

    if (Menu->Toggle(1, 1, "Ramasser le slorm ancien", g_ElderSlorm))
        ToggleElderSlorm();
}

// Renvoie vrai si ModMenu.dll est installe et a accepte notre page.
static bool RegisterInModMenu()
{
    AurieInterfaceBase* base = nullptr;
    if (!AurieSuccess(ObGetInterface(ModMenu::kInterfaceName, base)) || !base)
        return false;

    ModMenu::ModEntry entry = {};
    entry.Name = "AUTOLOOT";
    entry.Title = "MOD  -  AUTOLOOT";
    entry.Help = "En jeu : F6 on/off  -  F7 / F8 portee  -  F9 equipement  -  Echap : retour";
    entry.IsActive = MenuIsActive;
    entry.DrawPage = MenuDrawPage;
    entry.OnFrame = MenuOnFrame;

    return static_cast<ModMenu::IModMenu*>(base)->RegisterMod(entry);
}

// ============================================================
// Initialisation du module
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
        OutputDebugStringA("[AutoLoot] ERREUR: interface YYToolkit introuvable\n");
        return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;
    }

    // --- Fichier de configuration : AutoLoot.ini a cote de la DLL ---
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

    // --- Installe un hook sur un script GameMaker ---
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

    // --- L'autoloot lui-meme : indispensable ---
    if (!installScriptHook("gml_Script_scr_hero_step", "HeroStep",
        reinterpret_cast<PVOID>(HeroStepHook), g_OriginalHeroStep))
    {
        DbgPrintEx(LOG_SEVERITY_ERROR, "[AutoLoot] impossible d'installer le hook de scr_hero_step");
        return AURIE_OBJECT_NOT_FOUND;
    }

    // --- Page de reglages dans le menu des mods : facultatif, l'autoloot marche sans ---
    const bool okMenu = RegisterInModMenu();

    // --- Espion sur scr_pick_up_item (mode Debug uniquement) ---
    if (g_Debug)
    {
        const bool okPick = installScriptHook("gml_Script_scr_pick_up_item", "PickUp",
            reinterpret_cast<PVOID>(PickUpHook), g_OriginalPickUp);
        DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] espion scr_pick_up_item : %s", okPick ? "installe" : "indisponible");
    }

    char rangeText[32];
    DescribeRange(rangeText, sizeof(rangeText));

    DbgPrintEx(LOG_SEVERITY_INFO, 
        "[AutoLoot] pret : autoloot %s, portee %s, equipement %s, slormandrites %s, slorm ancien %s",
        g_Enabled ? "ACTIVE" : "DESACTIVE", rangeText, g_Equipment ? "OUI" : "NON",
        g_Slormandrites ? "OUI" : "NON", g_ElderSlorm ? "OUI" : "NON");
    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] raccourcis : F6 on/off, F7/F8 portee -/+, F9 equipement");
    DbgPrintEx(LOG_SEVERITY_INFO, "[AutoLoot] menu des mods (ModMenu.dll) : %s",
        okMenu ? "page de reglages ajoutee" : "absent (raccourcis clavier seulement)");

    return AURIE_SUCCESS;
}

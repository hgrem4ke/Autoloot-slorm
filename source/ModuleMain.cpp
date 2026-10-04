#include <YYToolkit/YYTK_Shared.hpp>

#include <cstdio>
#include <cstdint>

using namespace Aurie;
using namespace YYTK;


// ============================================================
// VARIABLES GLOBALES
// ============================================================

static YYTKInterface* g_ModuleInterface = nullptr;
static AurieModule* g_AutoLootModule = nullptr;


// ============================================================
// AFFICHAGE DES INFORMATIONS D'UNE INSTANCE
// ============================================================

static void PrintInstanceInfo(
    CInstance* Instance,
    const char* Prefix
)
{
    if (Instance == nullptr)
    {
        g_ModuleInterface->Print(
            CM_LIGHTYELLOW,
            "%s = NULL",
            Prefix
        );

        return;
    }

    RValue IdValue;
    RValue XValue;
    RValue YValue;

    bool HasId = false;
    bool HasX = false;
    bool HasY = false;


    // --------------------------------------------------------
    // ID
    // --------------------------------------------------------

    AurieStatus StatusId =
        g_ModuleInterface->GetBuiltin(
            "id",
            Instance,
            NULL_INDEX,
            IdValue
        );

    if (AurieSuccess(StatusId))
    {
        HasId = true;
    }


    // --------------------------------------------------------
    // X
    // --------------------------------------------------------

    AurieStatus StatusX =
        g_ModuleInterface->GetBuiltin(
            "x",
            Instance,
            NULL_INDEX,
            XValue
        );

    if (AurieSuccess(StatusX))
    {
        HasX = true;
    }


    // --------------------------------------------------------
    // Y
    // --------------------------------------------------------

    AurieStatus StatusY =
        g_ModuleInterface->GetBuiltin(
            "y",
            Instance,
            NULL_INDEX,
            YValue
        );

    if (AurieSuccess(StatusY))
    {
        HasY = true;
    }


    // --------------------------------------------------------
    // AFFICHAGE
    // --------------------------------------------------------

    if (HasId && HasX && HasY)
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s ID=%d X=%.2f Y=%.2f",
            Prefix,
            IdValue.ToInt32(),
            XValue.ToDouble(),
            YValue.ToDouble()
        );
    }
    else if (HasId)
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s ID=%d",
            Prefix,
            IdValue.ToInt32()
        );
    }
    else
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s Instance=%p",
            Prefix,
            static_cast<void*>(Instance)
        );
    }
}


// ============================================================
// AFFICHAGE D'UN RVALUE
// ============================================================

static void PrintRValue(
    RValue* Value,
    const char* Prefix
)
{
    if (Value == nullptr)
    {
        g_ModuleInterface->Print(
            CM_LIGHTYELLOW,
            "%s = NULL",
            Prefix
        );

        return;
    }


    switch (Value->m_Kind)
    {
        // ----------------------------------------------------
        // REAL
        // ----------------------------------------------------

    case VALUE_REAL:
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s KIND=REAL VALUE=%f",
            Prefix,
            Value->m_Real
        );

        break;
    }


    // ----------------------------------------------------
    // INT32
    // ----------------------------------------------------

    case VALUE_INT32:
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s KIND=INT32 VALUE=%d",
            Prefix,
            Value->m_i32
        );

        break;
    }


    // ----------------------------------------------------
    // INT64
    // ----------------------------------------------------

    case VALUE_INT64:
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s KIND=INT64 VALUE=%lld",
            Prefix,
            static_cast<long long>(Value->m_i64)
        );

        break;
    }


    // ----------------------------------------------------
    // BOOL
    // ----------------------------------------------------

    case VALUE_BOOL:
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s KIND=BOOL VALUE=%s",
            Prefix,
            Value->ToBoolean()
            ? "true"
            : "false"
        );

        break;
    }


    // ----------------------------------------------------
    // STRING
    // ----------------------------------------------------

    case VALUE_STRING:
    {
        const char* StringValue =
            Value->ToCString();

        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s KIND=STRING VALUE=%s",
            Prefix,
            StringValue
            ? StringValue
            : "<null>"
        );

        break;
    }


    // ----------------------------------------------------
    // OBJECT
    // ----------------------------------------------------

    case VALUE_OBJECT:
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s KIND=OBJECT",
            Prefix
        );

        break;
    }


    // ----------------------------------------------------
    // POINTER
    // ----------------------------------------------------

    case VALUE_PTR:
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s KIND=PTR VALUE=%p",
            Prefix,
            Value->m_Pointer
        );

        break;
    }


    // ----------------------------------------------------
    // ARRAY
    // ----------------------------------------------------

    case VALUE_ARRAY:
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s KIND=ARRAY",
            Prefix
        );

        break;
    }


    // ----------------------------------------------------
    // NULL
    // ----------------------------------------------------

    case VALUE_NULL:
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s KIND=NULL",
            Prefix
        );

        break;
    }


    // ----------------------------------------------------
    // UNDEFINED
    // ----------------------------------------------------

    case VALUE_UNDEFINED:
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s KIND=UNDEFINED",
            Prefix
        );

        break;
    }


    // ----------------------------------------------------
    // AUTRE
    // ----------------------------------------------------

    default:
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s KIND=%s",
            Prefix,
            Value->GetKindName().c_str()
        );

        break;
    }
    }
}


// ============================================================
// HOOK : scr_pick_up_item
// ============================================================

RValue& PickUpHook(
    IN CInstance* Self,
    IN CInstance* Other,
    OUT RValue& Result,
    IN int ArgumentCount,
    IN RValue** Arguments
)
{
    static uint32_t CallCounter = 0;

    CallCounter++;


    g_ModuleInterface->Print(
        CM_LIGHTGREEN,
        "[AutoLoot] PICKUP call=%u args=%d",
        CallCounter,
        ArgumentCount
    );


    PrintInstanceInfo(
        Self,
        "[AutoLoot] PICKUP Self"
    );


    PrintInstanceInfo(
        Other,
        "[AutoLoot] PICKUP Other"
    );


    for (int i = 0; i < ArgumentCount; i++)
    {
        char Prefix[128];

        snprintf(
            Prefix,
            sizeof(Prefix),
            "[AutoLoot] PICKUP ARG[%d]",
            i
        );

        PrintRValue(
            Arguments[i],
            Prefix
        );
    }


    const PFUNC_YYGMLScript Original =
        reinterpret_cast<PFUNC_YYGMLScript>(
            MmGetHookTrampoline(
                g_AutoLootModule,
                "PickUp"
            )
            );


    if (!Original)
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "[AutoLoot] PICKUP : trampoline introuvable"
        );

        return Result;
    }


    Original(
        Self,
        Other,
        Result,
        ArgumentCount,
        Arguments
    );


    return Result;
}


// ============================================================
// HOOK : scr_gold_loot
// ============================================================

RValue& GoldLootHook(
    IN CInstance* Self,
    IN CInstance* Other,
    OUT RValue& Result,
    IN int ArgumentCount,
    IN RValue** Arguments
)
{
    static uint32_t CallCounter = 0;

    CallCounter++;


    g_ModuleInterface->Print(
        CM_LIGHTGREEN,
        "[AutoLoot] GOLD_LOOT call=%u args=%d",
        CallCounter,
        ArgumentCount
    );


    PrintInstanceInfo(
        Self,
        "[AutoLoot] GOLD_LOOT Self"
    );


    PrintInstanceInfo(
        Other,
        "[AutoLoot] GOLD_LOOT Other"
    );


    for (int i = 0; i < ArgumentCount; i++)
    {
        char Prefix[128];

        snprintf(
            Prefix,
            sizeof(Prefix),
            "[AutoLoot] GOLD_LOOT ARG[%d]",
            i
        );

        PrintRValue(
            Arguments[i],
            Prefix
        );
    }


    const PFUNC_YYGMLScript Original =
        reinterpret_cast<PFUNC_YYGMLScript>(
            MmGetHookTrampoline(
                g_AutoLootModule,
                "GoldLoot"
            )
            );


    if (!Original)
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "[AutoLoot] GOLD_LOOT : trampoline introuvable"
        );

        return Result;
    }


    Original(
        Self,
        Other,
        Result,
        ArgumentCount,
        Arguments
    );


    return Result;
}


// ============================================================
// HOOK : scr_hero_activate
// ============================================================

RValue& HeroActivateHook(
    IN CInstance* Self,
    IN CInstance* Other,
    OUT RValue& Result,
    IN int ArgumentCount,
    IN RValue** Arguments
)
{
    static uint32_t CallCounter = 0;

    CallCounter++;


    // On limite le spam à 20 appels
    if (CallCounter <= 20)
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] HERO_ACTIVATE call=%u args=%d",
            CallCounter,
            ArgumentCount
        );


        PrintInstanceInfo(
            Self,
            "[AutoLoot] HERO_ACTIVATE Self"
        );


        PrintInstanceInfo(
            Other,
            "[AutoLoot] HERO_ACTIVATE Other"
        );


        for (int i = 0; i < ArgumentCount; i++)
        {
            char Prefix[128];

            snprintf(
                Prefix,
                sizeof(Prefix),
                "[AutoLoot] HERO_ACTIVATE ARG[%d]",
                i
            );


            PrintRValue(
                Arguments[i],
                Prefix
            );
        }
    }


    const PFUNC_YYGMLScript Original =
        reinterpret_cast<PFUNC_YYGMLScript>(
            MmGetHookTrampoline(
                g_AutoLootModule,
                "HeroActivate"
            )
            );


    if (!Original)
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "[AutoLoot] HERO_ACTIVATE : trampoline introuvable"
        );

        return Result;
    }


    Original(
        Self,
        Other,
        Result,
        ArgumentCount,
        Arguments
    );


    return Result;
}


// ============================================================
// INITIALISATION
// ============================================================

EXPORTED AurieStatus ModuleInitialize(
    IN AurieModule* Module,
    IN const fs::path& ModulePath
)
{
    UNREFERENCED_PARAMETER(ModulePath);


    // Notre propre pointeur de module
    g_AutoLootModule = Module;


    AurieStatus LastStatus =
        AURIE_SUCCESS;


    // --------------------------------------------------------
    // YYToolkit
    // --------------------------------------------------------

    g_ModuleInterface =
        YYTK::GetInterface();


    if (!g_ModuleInterface)
    {
        return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;
    }


    g_ModuleInterface->Print(
        CM_LIGHTGREEN,
        "[AutoLoot] ModuleInitialize OK"
    );


    // ========================================================
    // scr_pick_up_item
    // ========================================================

    CScript* PickUpScript = nullptr;


    LastStatus =
        g_ModuleInterface->GetNamedRoutinePointer(
            "gml_Script_scr_pick_up_item",
            reinterpret_cast<PVOID*>(&PickUpScript)
        );


    if (
        AurieSuccess(LastStatus) &&
        PickUpScript != nullptr &&
        PickUpScript->m_Functions != nullptr &&
        PickUpScript->m_Functions->m_ScriptFunction != nullptr
        )
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] scr_pick_up_item : TROUVE"
        );


        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] ScriptFunction PickUp = %p",
            reinterpret_cast<void*>(
                PickUpScript->m_Functions->m_ScriptFunction
                )
        );


        LastStatus =
            MmCreateHook(
                g_AutoLootModule,
                "PickUp",
                PickUpScript->m_Functions->m_ScriptFunction,
                PickUpHook,
                nullptr
            );


        if (AurieSuccess(LastStatus))
        {
            g_ModuleInterface->Print(
                CM_LIGHTGREEN,
                "[AutoLoot] HOOK scr_pick_up_item INSTALLE"
            );
        }
        else
        {
            g_ModuleInterface->Print(
                CM_LIGHTRED,
                "[AutoLoot] ECHEC hook PickUp : 0x%llX",
                static_cast<unsigned long long>(
                    LastStatus
                    )
            );
        }
    }
    else
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "[AutoLoot] scr_pick_up_item : INTROUVABLE"
        );
    }


    // ========================================================
    // scr_gold_loot
    // ========================================================

    CScript* GoldLootScript = nullptr;


    LastStatus =
        g_ModuleInterface->GetNamedRoutinePointer(
            "gml_Script_scr_gold_loot",
            reinterpret_cast<PVOID*>(&GoldLootScript)
        );


    if (
        AurieSuccess(LastStatus) &&
        GoldLootScript != nullptr &&
        GoldLootScript->m_Functions != nullptr &&
        GoldLootScript->m_Functions->m_ScriptFunction != nullptr
        )
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] scr_gold_loot : TROUVE"
        );


        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] ScriptFunction GoldLoot = %p",
            reinterpret_cast<void*>(
                GoldLootScript->m_Functions->m_ScriptFunction
                )
        );


        LastStatus =
            MmCreateHook(
                g_AutoLootModule,
                "GoldLoot",
                GoldLootScript->m_Functions->m_ScriptFunction,
                GoldLootHook,
                nullptr
            );


        if (AurieSuccess(LastStatus))
        {
            g_ModuleInterface->Print(
                CM_LIGHTGREEN,
                "[AutoLoot] HOOK scr_gold_loot INSTALLE"
            );
        }
        else
        {
            g_ModuleInterface->Print(
                CM_LIGHTRED,
                "[AutoLoot] ECHEC hook GoldLoot : 0x%llX",
                static_cast<unsigned long long>(
                    LastStatus
                    )
            );
        }
    }
    else
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "[AutoLoot] scr_gold_loot : INTROUVABLE"
        );
    }


    // ========================================================
    // scr_hero_activate
    // ========================================================

    CScript* HeroActivateScript = nullptr;


    LastStatus =
        g_ModuleInterface->GetNamedRoutinePointer(
            "gml_Script_scr_hero_activate",
            reinterpret_cast<PVOID*>(&HeroActivateScript)
        );


    if (
        AurieSuccess(LastStatus) &&
        HeroActivateScript != nullptr &&
        HeroActivateScript->m_Functions != nullptr &&
        HeroActivateScript->m_Functions->m_ScriptFunction != nullptr
        )
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] scr_hero_activate : TROUVE"
        );


        LastStatus =
            MmCreateHook(
                g_AutoLootModule,
                "HeroActivate",
                HeroActivateScript->m_Functions->m_ScriptFunction,
                HeroActivateHook,
                nullptr
            );


        if (AurieSuccess(LastStatus))
        {
            g_ModuleInterface->Print(
                CM_LIGHTGREEN,
                "[AutoLoot] HOOK scr_hero_activate INSTALLE"
            );
        }
        else
        {
            g_ModuleInterface->Print(
                CM_LIGHTRED,
                "[AutoLoot] ECHEC hook HeroActivate : 0x%llX",
                static_cast<unsigned long long>(
                    LastStatus
                    )
            );
        }
    }
    else
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "[AutoLoot] scr_hero_activate : INTROUVABLE"
        );
    }


    // ========================================================
    // FIN
    // ========================================================

    g_ModuleInterface->Print(
        CM_LIGHTGREEN,
        "[AutoLoot] Initialisation terminee"
    );


    return AURIE_SUCCESS;
}


// ============================================================
// DECHARGEMENT
// ============================================================

EXPORTED AurieStatus ModuleUnload(
    IN AurieModule* Module
)
{
    UNREFERENCED_PARAMETER(Module);


    if (g_ModuleInterface)
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] ModuleUnload"
        );
    }


    return AURIE_SUCCESS;
}
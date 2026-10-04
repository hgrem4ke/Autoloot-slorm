#include <YYToolkit/YYTK_Shared.hpp>

using namespace Aurie;
using namespace YYTK;

static YYTKInterface* g_ModuleInterface = nullptr;


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
            CM_LIGHTRED,
            "%s = NULL",
            Prefix
        );

        return;
    }

    std::string Kind = Value->GetKindName();


    // --------------------------------------------------------
    // ARRAY
    // --------------------------------------------------------

    if (Value->m_Kind == VALUE_ARRAY)
    {
        g_ModuleInterface->Print(
            CM_LIGHTYELLOW,
            "%s KIND=ARRAY",
            Prefix
        );

        try
        {
            std::vector<RValue> Values = Value->ToVector();

            g_ModuleInterface->Print(
                CM_LIGHTYELLOW,
                "%s ARRAY SIZE=%zu",
                Prefix,
                Values.size()
            );

            const size_t MaxValues = 50;

            size_t Count = Values.size();

            if (Count > MaxValues)
                Count = MaxValues;


            for (size_t i = 0; i < Count; i++)
            {
                char ElementPrefix[128];

                snprintf(
                    ElementPrefix,
                    sizeof(ElementPrefix),
                    "%s[%zu]",
                    Prefix,
                    i
                );

                PrintRValue(
                    &Values[i],
                    ElementPrefix
                );
            }


            if (Values.size() > MaxValues)
            {
                g_ModuleInterface->Print(
                    CM_LIGHTYELLOW,
                    "%s ... %zu elements non affiches",
                    Prefix,
                    Values.size() - MaxValues
                );
            }
        }
        catch (...)
        {
            g_ModuleInterface->Print(
                CM_LIGHTRED,
                "%s ERREUR lecture ARRAY",
                Prefix
            );
        }

        return;
    }


    // --------------------------------------------------------
    // REAL
    // --------------------------------------------------------

    if (Value->m_Kind == VALUE_REAL)
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s KIND=REAL VALUE=%f",
            Prefix,
            Value->ToDouble()
        );

        return;
    }


    // --------------------------------------------------------
    // INT32
    // --------------------------------------------------------

    if (Value->m_Kind == VALUE_INT32)
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s KIND=INT32 VALUE=%d",
            Prefix,
            Value->ToInt32()
        );

        return;
    }


    // --------------------------------------------------------
    // INT64
    // --------------------------------------------------------

    if (Value->m_Kind == VALUE_INT64)
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s KIND=INT64 VALUE=%lld",
            Prefix,
            static_cast<long long>(Value->ToInt64())
        );

        return;
    }


    // --------------------------------------------------------
    // BOOL
    // --------------------------------------------------------

    if (Value->m_Kind == VALUE_BOOL)
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s KIND=BOOL VALUE=%s",
            Prefix,
            Value->ToBoolean() ? "true" : "false"
        );

        return;
    }


    // --------------------------------------------------------
    // STRING
    // --------------------------------------------------------

    if (Value->m_Kind == VALUE_STRING)
    {
        try
        {
            std::string Text = Value->ToString();

            g_ModuleInterface->Print(
                CM_LIGHTGREEN,
                "%s KIND=STRING VALUE=%s",
                Prefix,
                Text.c_str()
            );
        }
        catch (...)
        {
            g_ModuleInterface->Print(
                CM_LIGHTRED,
                "%s KIND=STRING <ERREUR>",
                Prefix
            );
        }

        return;
    }


    // --------------------------------------------------------
    // OBJECT
    // --------------------------------------------------------

    if (Value->m_Kind == VALUE_OBJECT)
    {
        g_ModuleInterface->Print(
            CM_LIGHTYELLOW,
            "%s KIND=OBJECT PTR=%p",
            Prefix,
            Value->m_Object
        );

        return;
    }


    // --------------------------------------------------------
    // NULL
    // --------------------------------------------------------

    if (Value->m_Kind == VALUE_NULL)
    {
        g_ModuleInterface->Print(
            CM_LIGHTYELLOW,
            "%s KIND=NULL",
            Prefix
        );

        return;
    }


    // --------------------------------------------------------
    // UNDEFINED
    // --------------------------------------------------------

    if (Value->m_Kind == VALUE_UNDEFINED)
    {
        g_ModuleInterface->Print(
            CM_LIGHTYELLOW,
            "%s KIND=UNDEFINED",
            Prefix
        );

        return;
    }


    // --------------------------------------------------------
    // AUTRE
    // --------------------------------------------------------

    g_ModuleInterface->Print(
        CM_LIGHTYELLOW,
        "%s KIND=%s",
        Prefix,
        Kind.c_str()
    );
}


// ============================================================
// AFFICHAGE DES INFOS D'UNE INSTANCE
// ============================================================

static void PrintInstanceInfo(
    CInstance* Instance,
    const char* Prefix
)
{
    if (Instance == nullptr)
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "%s INSTANCE = NULL",
            Prefix
        );

        return;
    }

    CInstanceInternal& Members = Instance->GetMembers();


    g_ModuleInterface->Print(
        CM_LIGHTYELLOW,
        "%s ID=%d OBJ=%d X=%f Y=%f",
        Prefix,
        Members.m_ID,
        Members.m_ObjectIndex,
        Members.m_X,
        Members.m_Y
    );
}


// ============================================================
// HOOK : scr_pick_up_item
// ============================================================

RValue& PickUpItemHook(
    IN CInstance* Self,
    IN CInstance* Other,
    OUT RValue& Result,
    IN int ArgumentCount,
    IN RValue** Arguments
)
{
    static uint32_t call_counter = 0;

    call_counter++;


    g_ModuleInterface->Print(
        CM_LIGHTGREEN,
        "[AutoLoot] PICKUP call=%u args=%d",
        call_counter,
        ArgumentCount
    );


    // --------------------------------------------------------
    // Informations Self
    // --------------------------------------------------------

    PrintInstanceInfo(
        Self,
        "[AutoLoot] PICKUP Self"
    );


    // --------------------------------------------------------
    // Informations Other
    // --------------------------------------------------------

    PrintInstanceInfo(
        Other,
        "[AutoLoot] PICKUP Other"
    );


    // --------------------------------------------------------
    // Arguments
    // --------------------------------------------------------

    for (int i = 0; i < ArgumentCount; i++)
    {
        if (Arguments == nullptr)
        {
            g_ModuleInterface->Print(
                CM_LIGHTRED,
                "[AutoLoot] PICKUP Arguments = NULL"
            );

            break;
        }


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


    // --------------------------------------------------------
    // Trampoline original
    // --------------------------------------------------------

    const PFUNC_YYGMLScript original =
        reinterpret_cast<PFUNC_YYGMLScript>(
            MmGetHookTrampoline(
                g_ArSelfModule,
                "PickUpItem"
            )
            );


    if (!original)
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "[AutoLoot] PICKUP : trampoline introuvable"
        );

        return Result;
    }


    // --------------------------------------------------------
    // Appel original
    // --------------------------------------------------------

    original(
        Self,
        Other,
        Result,
        ArgumentCount,
        Arguments
    );


    return Result;
}


// ============================================================
// HOOK : obj_loot Other Event 25
//
// gml_Object_obj_loot_Other_25
// ============================================================

RValue& LootOther25Hook(
    IN CInstance* Self,
    IN CInstance* Other,
    OUT RValue& Result,
    IN int ArgumentCount,
    IN RValue** Arguments
)
{
    static uint32_t call_counter = 0;

    call_counter++;


    // --------------------------------------------------------
    // On limite l'affichage pour éviter de spammer le log
    // --------------------------------------------------------

    if (call_counter <= 30 ||
        (call_counter % 100) == 0)
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] LOOT_OTHER_25 call=%u args=%d",
            call_counter,
            ArgumentCount
        );


        // ----------------------------------------------------
        // Self
        // ----------------------------------------------------

        PrintInstanceInfo(
            Self,
            "[AutoLoot] LOOT_OTHER_25 Self"
        );


        // ----------------------------------------------------
        // Other
        // ----------------------------------------------------

        PrintInstanceInfo(
            Other,
            "[AutoLoot] LOOT_OTHER_25 Other"
        );


        // ----------------------------------------------------
        // Arguments
        // ----------------------------------------------------

        for (int i = 0; i < ArgumentCount; i++)
        {
            if (Arguments == nullptr)
            {
                g_ModuleInterface->Print(
                    CM_LIGHTRED,
                    "[AutoLoot] LOOT_OTHER_25 Arguments = NULL"
                );

                break;
            }


            char Prefix[128];

            snprintf(
                Prefix,
                sizeof(Prefix),
                "[AutoLoot] LOOT_OTHER_25 ARG[%d]",
                i
            );


            PrintRValue(
                Arguments[i],
                Prefix
            );
        }
    }


    // --------------------------------------------------------
    // Trampoline original
    // --------------------------------------------------------

    const PFUNC_YYGMLScript original =
        reinterpret_cast<PFUNC_YYGMLScript>(
            MmGetHookTrampoline(
                g_ArSelfModule,
                "LootOther25"
            )
            );


    if (!original)
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "[AutoLoot] LOOT_OTHER_25 : trampoline introuvable"
        );

        return Result;
    }


    // --------------------------------------------------------
    // Appel original
    // --------------------------------------------------------

    original(
        Self,
        Other,
        Result,
        ArgumentCount,
        Arguments
    );


    return Result;
}


// ============================================================
// INITIALISATION DU MODULE
// ============================================================

EXPORTED AurieStatus ModuleInitialize(
    IN AurieModule* Module,
    IN const fs::path& ModulePath
)
{
    UNREFERENCED_PARAMETER(ModulePath);
    UNREFERENCED_PARAMETER(Module);


    AurieStatus last_status = AURIE_SUCCESS;


    // ========================================================
    // INTERFACE YYTOOLKIT
    // ========================================================

    g_ModuleInterface = YYTK::GetInterface();

    if (!g_ModuleInterface)
    {
        return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;
    }


    g_ModuleInterface->Print(
        CM_LIGHTGREEN,
        "[AutoLoot] ModuleInitialize OK"
    );


    // ========================================================
    // HOOK 1 : scr_pick_up_item
    // ========================================================

    CScript* pick_up_script = nullptr;


    last_status =
        g_ModuleInterface->GetNamedRoutinePointer(
            "gml_Script_scr_pick_up_item",
            reinterpret_cast<PVOID*>(&pick_up_script)
        );


    if (!AurieSuccess(last_status) ||
        pick_up_script == nullptr)
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "[AutoLoot] scr_pick_up_item : INTROUVABLE"
        );
    }
    else
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] scr_pick_up_item : TROUVE"
        );


        if (pick_up_script->m_Functions == nullptr ||
            pick_up_script->m_Functions->m_ScriptFunction == nullptr)
        {
            g_ModuleInterface->Print(
                CM_LIGHTRED,
                "[AutoLoot] scr_pick_up_item : ScriptFunction invalide"
            );
        }
        else
        {
            g_ModuleInterface->Print(
                CM_LIGHTGREEN,
                "[AutoLoot] ScriptFunction PickUp = %p",
                pick_up_script->m_Functions->m_ScriptFunction
            );


            last_status =
                MmCreateHook(
                    g_ArSelfModule,
                    "PickUpItem",
                    pick_up_script->m_Functions->m_ScriptFunction,
                    PickUpItemHook,
                    nullptr
                );


            if (!AurieSuccess(last_status))
            {
                g_ModuleInterface->Print(
                    CM_LIGHTRED,
                    "[AutoLoot] ECHEC hook PickUpItem : 0x%llX",
                    static_cast<unsigned long long>(last_status)
                );
            }
            else
            {
                g_ModuleInterface->Print(
                    CM_LIGHTGREEN,
                    "[AutoLoot] HOOK scr_pick_up_item INSTALLE"
                );
            }
        }
    }


    // ========================================================
    // HOOK 2 : obj_loot Other Event 25
    // ========================================================

    CScript* loot_other25_script = nullptr;


    last_status =
        g_ModuleInterface->GetNamedRoutinePointer(
            "gml_Object_obj_loot_Other_25",
            reinterpret_cast<PVOID*>(&loot_other25_script)
        );


    if (!AurieSuccess(last_status) ||
        loot_other25_script == nullptr)
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "[AutoLoot] obj_loot Other_25 : INTROUVABLE"
        );
    }
    else
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] obj_loot Other_25 : TROUVE"
        );


        if (loot_other25_script->m_Functions == nullptr ||
            loot_other25_script->m_Functions->m_ScriptFunction == nullptr)
        {
            g_ModuleInterface->Print(
                CM_LIGHTRED,
                "[AutoLoot] obj_loot Other_25 : ScriptFunction invalide"
            );
        }
        else
        {
            g_ModuleInterface->Print(
                CM_LIGHTGREEN,
                "[AutoLoot] ScriptFunction LootOther25 = %p",
                loot_other25_script->m_Functions->m_ScriptFunction
            );


            last_status =
                MmCreateHook(
                    g_ArSelfModule,
                    "LootOther25",
                    loot_other25_script->m_Functions->m_ScriptFunction,
                    LootOther25Hook,
                    nullptr
                );


            if (!AurieSuccess(last_status))
            {
                g_ModuleInterface->Print(
                    CM_LIGHTRED,
                    "[AutoLoot] ECHEC hook LootOther25 : 0x%llX",
                    static_cast<unsigned long long>(last_status)
                );
            }
            else
            {
                g_ModuleInterface->Print(
                    CM_LIGHTGREEN,
                    "[AutoLoot] HOOK obj_loot_Other_25 INSTALLE"
                );
            }
        }
    }


    // ========================================================
    // FIN
    // ========================================================

    g_ModuleInterface->Print(
        CM_LIGHTGREEN,
        "[AutoLoot] INITIALISATION TERMINEE"
    );


    return AURIE_SUCCESS;
}
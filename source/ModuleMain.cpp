#include <YYToolkit/YYTK_Shared.hpp>

using namespace Aurie;
using namespace YYTK;

static YYTKInterface* g_ModuleInterface = nullptr;


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
            CM_LIGHTRED,
            "%s = NULL",
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

            const size_t MaxValues = 20;

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
        std::string Text = Value->ToString();

        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "%s KIND=STRING VALUE=%s",
            Prefix,
            Text.c_str()
        );

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
    // NULL / UNDEFINED
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
// HOOK : obj_loot Step
//
// gml_Object_obj_loot_Step_0
// ============================================================

RValue& LootStepHook(
    IN CInstance* Self,
    IN CInstance* Other,
    OUT RValue& Result,
    IN int ArgumentCount,
    IN RValue** Arguments
)
{
    UNREFERENCED_PARAMETER(Other);
    UNREFERENCED_PARAMETER(Arguments);

    static uint32_t frame_counter = 0;

    frame_counter++;


    // --------------------------------------------------------
    // On ne log qu'une fois toutes les 60 exécutions
    // pour éviter de saturer le log.
    // --------------------------------------------------------

    if ((frame_counter % 60) == 0)
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] LOOT_STEP call=%u args=%d",
            frame_counter,
            ArgumentCount
        );

        PrintInstanceInfo(
            Self,
            "[AutoLoot] LOOT_STEP Self"
        );
    }


    // --------------------------------------------------------
    // Trampoline original
    // --------------------------------------------------------

    const PFUNC_YYGMLScript original =
        reinterpret_cast<PFUNC_YYGMLScript>(
            MmGetHookTrampoline(
                g_ArSelfModule,
                "LootStep"
            )
            );


    if (!original)
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "[AutoLoot] LOOT_STEP : trampoline introuvable"
        );

        return Result;
    }


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
// INITIALISATION
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
    // YYTOOLKIT
    // ========================================================

    g_ModuleInterface = YYTK::GetInterface();

    if (!g_ModuleInterface)
        return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;


    g_ModuleInterface->Print(
        CM_LIGHTGREEN,
        "[AutoLoot] ModuleInitialize OK"
    );


    // ========================================================
    // HOOK : scr_pick_up_item
    // ========================================================

    CScript* pick_up_script = nullptr;


    last_status =
        g_ModuleInterface->GetNamedRoutinePointer(
            "gml_Script_scr_pick_up_item",
            reinterpret_cast<PVOID*>(&pick_up_script)
        );


    if (AurieSuccess(last_status) &&
        pick_up_script != nullptr &&
        pick_up_script->m_Functions != nullptr &&
        pick_up_script->m_Functions->m_ScriptFunction != nullptr)
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] scr_pick_up_item : TROUVE"
        );


        last_status =
            MmCreateHook(
                g_ArSelfModule,
                "PickUpItem",
                pick_up_script->m_Functions->m_ScriptFunction,
                PickUpItemHook,
                nullptr
            );


        if (AurieSuccess(last_status))
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
                "[AutoLoot] ECHEC hook PickUpItem : 0x%llX",
                static_cast<unsigned long long>(last_status)
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
    // HOOK : obj_loot Step
    // ========================================================

    CScript* loot_step_script = nullptr;


    last_status =
        g_ModuleInterface->GetNamedRoutinePointer(
            "gml_Object_obj_loot_Step_0",
            reinterpret_cast<PVOID*>(&loot_step_script)
        );


    if (AurieSuccess(last_status) &&
        loot_step_script != nullptr &&
        loot_step_script->m_Functions != nullptr &&
        loot_step_script->m_Functions->m_ScriptFunction != nullptr)
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] obj_loot Step_0 : TROUVE"
        );


        last_status =
            MmCreateHook(
                g_ArSelfModule,
                "LootStep",
                loot_step_script->m_Functions->m_ScriptFunction,
                LootStepHook,
                nullptr
            );


        if (AurieSuccess(last_status))
        {
            g_ModuleInterface->Print(
                CM_LIGHTGREEN,
                "[AutoLoot] HOOK obj_loot_Step_0 INSTALLE"
            );
        }
        else
        {
            g_ModuleInterface->Print(
                CM_LIGHTRED,
                "[AutoLoot] ECHEC hook LootStep : 0x%llX",
                static_cast<unsigned long long>(last_status)
            );
        }
    }
    else
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "[AutoLoot] obj_loot Step_0 : INTROUVABLE"
        );
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
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

            // On limite l'affichage pour éviter de remplir le log
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
    // AUTRE TYPE
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


    // --------------------------------------------------------
    // Affichage des arguments
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
    static uint32_t call_counter = 0;

    call_counter++;


    g_ModuleInterface->Print(
        CM_LIGHTGREEN,
        "[AutoLoot] HERO_ACTIVATE call=%u args=%d",
        call_counter,
        ArgumentCount
    );


    // --------------------------------------------------------
    // Affichage des arguments
    // --------------------------------------------------------

    for (int i = 0; i < ArgumentCount; i++)
    {
        if (Arguments == nullptr)
        {
            g_ModuleInterface->Print(
                CM_LIGHTRED,
                "[AutoLoot] HERO_ACTIVATE Arguments = NULL"
            );

            break;
        }


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


    // --------------------------------------------------------
    // Informations sur Self / Other
    // --------------------------------------------------------

    if (Self != nullptr)
    {
        g_ModuleInterface->Print(
            CM_LIGHTYELLOW,
            "[AutoLoot] HERO_ACTIVATE Self=%p",
            Self
        );
    }

    if (Other != nullptr)
    {
        g_ModuleInterface->Print(
            CM_LIGHTYELLOW,
            "[AutoLoot] HERO_ACTIVATE Other=%p",
            Other
        );
    }


    // --------------------------------------------------------
    // Trampoline original
    // --------------------------------------------------------

    const PFUNC_YYGMLScript original =
        reinterpret_cast<PFUNC_YYGMLScript>(
            MmGetHookTrampoline(
                g_ArSelfModule,
                "HeroActivate"
            )
            );


    if (!original)
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "[AutoLoot] HERO_ACTIVATE : trampoline introuvable"
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
    // HOOK 2 : scr_hero_activate
    // ========================================================

    CScript* hero_activate_script = nullptr;


    last_status =
        g_ModuleInterface->GetNamedRoutinePointer(
            "gml_Script_scr_hero_activate",
            reinterpret_cast<PVOID*>(&hero_activate_script)
        );


    if (!AurieSuccess(last_status) ||
        hero_activate_script == nullptr)
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "[AutoLoot] scr_hero_activate : INTROUVABLE"
        );
    }
    else
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] scr_hero_activate : TROUVE"
        );


        if (hero_activate_script->m_Functions == nullptr ||
            hero_activate_script->m_Functions->m_ScriptFunction == nullptr)
        {
            g_ModuleInterface->Print(
                CM_LIGHTRED,
                "[AutoLoot] scr_hero_activate : ScriptFunction invalide"
            );
        }
        else
        {
            g_ModuleInterface->Print(
                CM_LIGHTGREEN,
                "[AutoLoot] ScriptFunction HeroActivate = %p",
                hero_activate_script->m_Functions->m_ScriptFunction
            );


            last_status =
                MmCreateHook(
                    g_ArSelfModule,
                    "HeroActivate",
                    hero_activate_script->m_Functions->m_ScriptFunction,
                    HeroActivateHook,
                    nullptr
                );


            if (!AurieSuccess(last_status))
            {
                g_ModuleInterface->Print(
                    CM_LIGHTRED,
                    "[AutoLoot] ECHEC hook HeroActivate : 0x%llX",
                    static_cast<unsigned long long>(last_status)
                );
            }
            else
            {
                g_ModuleInterface->Print(
                    CM_LIGHTGREEN,
                    "[AutoLoot] HOOK scr_hero_activate INSTALLE"
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
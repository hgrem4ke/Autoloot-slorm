#include <YYToolkit/YYTK_Shared.hpp>

using namespace Aurie;
using namespace YYTK;

static YYTKInterface* g_ModuleInterface = nullptr;


// ============================================================
// Affiche un RValue
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
                RValue& Element = Values[i];


                std::string ElementPrefix =
                    std::string(Prefix) +
                    "[" +
                    std::to_string(i) +
                    "]";


                PrintRValue(
                    &Element,
                    ElementPrefix.c_str()
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
    UNREFERENCED_PARAMETER(Self);
    UNREFERENCED_PARAMETER(Other);

    static uint32_t call_counter = 0;

    call_counter++;


    // --------------------------------------------------------
    // Informations générales
    // --------------------------------------------------------

    g_ModuleInterface->Print(
        CM_LIGHTGREEN,
        "[AutoLoot] PICKUP call=%u args=%d",
        call_counter,
        ArgumentCount
    );


    // --------------------------------------------------------
    // Analyse des arguments
    // --------------------------------------------------------

    for (int i = 0; i < ArgumentCount; i++)
    {
        if (Arguments == nullptr)
        {
            g_ModuleInterface->Print(
                CM_LIGHTRED,
                "[AutoLoot] Arguments = NULL"
            );

            break;
        }


        if (Arguments[i] == nullptr)
        {
            g_ModuleInterface->Print(
                CM_LIGHTYELLOW,
                "[AutoLoot] ARG[%d] = NULL",
                i
            );

            continue;
        }


        char Prefix[64];

        snprintf(
            Prefix,
            sizeof(Prefix),
            "[AutoLoot] ARG[%d]",
            i
        );


        PrintRValue(
            Arguments[i],
            Prefix
        );
    }


    // --------------------------------------------------------
    // Récupération du trampoline original
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
            "[AutoLoot] ERREUR : trampoline introuvable"
        );

        return Result;
    }


    // --------------------------------------------------------
    // Appel de la fonction originale
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


    // --------------------------------------------------------
    // Récupération de l'interface YYToolkit
    // --------------------------------------------------------

    g_ModuleInterface = YYTK::GetInterface();

    if (!g_ModuleInterface)
    {
        return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;
    }


    g_ModuleInterface->Print(
        CM_LIGHTGREEN,
        "[AutoLoot] ModuleInitialize OK"
    );


    // --------------------------------------------------------
    // Recherche de scr_pick_up_item
    // --------------------------------------------------------

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

        return last_status;
    }


    g_ModuleInterface->Print(
        CM_LIGHTGREEN,
        "[AutoLoot] scr_pick_up_item : TROUVE"
    );


    // --------------------------------------------------------
    // Vérification ScriptFunction
    // --------------------------------------------------------

    if (pick_up_script->m_Functions == nullptr ||
        pick_up_script->m_Functions->m_ScriptFunction == nullptr)
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "[AutoLoot] scr_pick_up_item : ScriptFunction invalide"
        );

        return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;
    }


    g_ModuleInterface->Print(
        CM_LIGHTGREEN,
        "[AutoLoot] ScriptFunction = %p",
        pick_up_script->m_Functions->m_ScriptFunction
    );


    // --------------------------------------------------------
    // Installation du hook
    // --------------------------------------------------------

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
            "[AutoLoot] ECHEC MmCreateHook : 0x%llX",
            static_cast<unsigned long long>(last_status)
        );

        return last_status;
    }


    g_ModuleInterface->Print(
        CM_LIGHTGREEN,
        "[AutoLoot] HOOK scr_pick_up_item INSTALLE"
    );


    return AURIE_SUCCESS;
}
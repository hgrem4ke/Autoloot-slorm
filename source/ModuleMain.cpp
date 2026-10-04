#include <YYToolkit/YYTK_Shared.hpp>

using namespace Aurie;
using namespace YYTK;

static YYTKInterface* g_ModuleInterface = nullptr;


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


        RValue* Arg = Arguments[i];


        // ----------------------------------------------------
        // Type de l'argument
        // ----------------------------------------------------

        std::string kind = Arg->GetKindName();


        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] ARG[%d] KIND = %s",
            i,
            kind.c_str()
        );


        // ----------------------------------------------------
        // REAL
        // ----------------------------------------------------

        if (Arg->m_Kind == VALUE_REAL)
        {
            g_ModuleInterface->Print(
                CM_LIGHTGREEN,
                "[AutoLoot] ARG[%d] REAL = %f",
                i,
                Arg->ToDouble()
            );
        }


        // ----------------------------------------------------
        // INT32
        // ----------------------------------------------------

        else if (Arg->m_Kind == VALUE_INT32)
        {
            g_ModuleInterface->Print(
                CM_LIGHTGREEN,
                "[AutoLoot] ARG[%d] INT32 = %d",
                i,
                Arg->ToInt32()
            );
        }


        // ----------------------------------------------------
        // INT64
        // ----------------------------------------------------

        else if (Arg->m_Kind == VALUE_INT64)
        {
            g_ModuleInterface->Print(
                CM_LIGHTGREEN,
                "[AutoLoot] ARG[%d] INT64 = %lld",
                i,
                static_cast<long long>(Arg->ToInt64())
            );
        }


        // ----------------------------------------------------
        // BOOL
        // ----------------------------------------------------

        else if (Arg->m_Kind == VALUE_BOOL)
        {
            g_ModuleInterface->Print(
                CM_LIGHTGREEN,
                "[AutoLoot] ARG[%d] BOOL = %s",
                i,
                Arg->ToBoolean() ? "true" : "false"
            );
        }


        // ----------------------------------------------------
        // STRING
        // ----------------------------------------------------

        else if (Arg->m_Kind == VALUE_STRING)
        {
            std::string text = Arg->ToString();

            g_ModuleInterface->Print(
                CM_LIGHTGREEN,
                "[AutoLoot] ARG[%d] STRING = %s",
                i,
                text.c_str()
            );
        }


        // ----------------------------------------------------
        // ARRAY
        // ----------------------------------------------------

        else if (Arg->m_Kind == VALUE_ARRAY)
        {
            g_ModuleInterface->Print(
                CM_LIGHTYELLOW,
                "[AutoLoot] ARG[%d] ARRAY",
                i
            );

            g_ModuleInterface->Print(
                CM_LIGHTYELLOW,
                "[AutoLoot] ARG[%d] ARRAY PTR = %p",
                i,
                Arg->m_Pointer
            );
        }


        // ----------------------------------------------------
        // OBJECT
        // ----------------------------------------------------

        else if (Arg->m_Kind == VALUE_OBJECT)
        {
            g_ModuleInterface->Print(
                CM_LIGHTYELLOW,
                "[AutoLoot] ARG[%d] OBJECT PTR = %p",
                i,
                Arg->m_Object
            );
        }


        // ----------------------------------------------------
        // AUTRE TYPE
        // ----------------------------------------------------

        else
        {
            g_ModuleInterface->Print(
                CM_LIGHTYELLOW,
                "[AutoLoot] ARG[%d] TYPE NON TRAITE = %s",
                i,
                kind.c_str()
            );
        }
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
    // Vérification de la fonction du script
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
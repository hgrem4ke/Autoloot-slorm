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
    // Affichage de l'appel
    // --------------------------------------------------------

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
                "[AutoLoot]   Arguments = NULL"
            );

            break;
        }

        if (Arguments[i] == nullptr)
        {
            g_ModuleInterface->Print(
                CM_LIGHTYELLOW,
                "[AutoLoot]   ARG[%d] = NULL",
                i
            );

            continue;
        }

        RValue* Arg = Arguments[i];

        std::string kind;
        std::string text;

        try
        {
            kind = Arg->GetKindName();
        }
        catch (...)
        {
            kind = "UNKNOWN";
        }

        try
        {
            text = Arg->ToString();
        }
        catch (...)
        {
            text = "<ToString ERROR>";
        }

        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot]   ARG[%d] kind=%s int=%d int64=%lld text=%s",
            i,
            kind.c_str(),
            Arg->ToInt32(),
            static_cast<long long>(Arg->ToInt64()),
            text.c_str()
        );
    }


    // --------------------------------------------------------
    // Appel de la fonction originale
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
    // Recherche du script GameMaker
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
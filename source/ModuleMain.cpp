#include <YYToolkit/YYTK_Shared.hpp>

using namespace Aurie;
using namespace YYTK;

static YYTKInterface* g_ModuleInterface = nullptr;


// ============================================================
// Hook de scr_pick_up_item
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
    UNREFERENCED_PARAMETER(Arguments);

    static uint32_t call_counter = 0;

    call_counter++;

    // Pour éviter de remplir complètement le log,
    // on affiche les 20 premiers appels puis 1 appel sur 100.
    if (call_counter <= 20 ||
        (call_counter % 100) == 0)
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] scr_pick_up_item APPELE - call=%u args=%d",
            call_counter,
            ArgumentCount
        );
    }


    // --------------------------------------------------------
    // Récupération de la fonction originale
    // --------------------------------------------------------

    const PFUNC_YYGMLScript original =
        reinterpret_cast<PFUNC_YYGMLScript>(
            MmGetHookTrampoline(
                g_ArSelfModule,
                "PickUpItem"
            )
            );


    // Sécurité
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
// Initialisation du plugin
// ============================================================

EXPORTED AurieStatus ModuleInitialize(
    IN AurieModule* Module,
    IN const fs::path& ModulePath
)
{
    UNREFERENCED_PARAMETER(ModulePath);
    UNREFERENCED_PARAMETER(Module);

    AurieStatus last_status =
        AURIE_SUCCESS;


    // --------------------------------------------------------
    // Récupération de l'interface YYToolkit
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
    // Vérification de la fonction compilée
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
    // Création du hook
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
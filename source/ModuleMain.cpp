#include <YYToolkit/YYTK_Shared.hpp>

using namespace Aurie;
using namespace YYTK;

static YYTKInterface* g_ModuleInterface = nullptr;


// ============================================================
// Callback exécuté à chaque frame
// ============================================================

static void FrameCallback(FWFrame& FrameContext)
{
    UNREFERENCED_PARAMETER(FrameContext);

    static uint32_t frame_counter = 0;

    frame_counter++;

    // Message toutes les 60 frames
    if ((frame_counter % 60) == 0)
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] CALLBACK OK - frame %u",
            frame_counter
        );
    }
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

    AurieStatus last_status = AURIE_SUCCESS;


    // --------------------------------------------------------
    // Récupération de l'interface YYToolkit
    // --------------------------------------------------------

    g_ModuleInterface = YYTK::GetInterface();

    if (!g_ModuleInterface)
    {
        return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;
    }


    // --------------------------------------------------------
    // Message de démarrage
    // --------------------------------------------------------

    g_ModuleInterface->Print(
        CM_LIGHTGREEN,
        "[AutoLoot] ModuleInitialize OK"
    );


    // --------------------------------------------------------
    // Recherche de scr_pick_up_item
    // --------------------------------------------------------

    PVOID routine = nullptr;

    AurieStatus routine_status =
        g_ModuleInterface->GetNamedRoutinePointer(
            "gml_Script_scr_pick_up_item",
            &routine
        );


    if (AurieSuccess(routine_status) &&
        routine != nullptr)
    {
        g_ModuleInterface->Print(
            CM_LIGHTGREEN,
            "[AutoLoot] scr_pick_up_item : TROUVE (%p)",
            routine
        );
    }
    else
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "[AutoLoot] scr_pick_up_item : INTROUVABLE"
        );
    }


    // --------------------------------------------------------
    // Enregistrement du callback
    // --------------------------------------------------------

    last_status =
        g_ModuleInterface->CreateCallback(
            Module,
            EVENT_FRAME,
            FrameCallback,
            0
        );


    if (!AurieSuccess(last_status))
    {
        g_ModuleInterface->Print(
            CM_LIGHTRED,
            "[AutoLoot] Echec CreateCallback"
        );

        return last_status;
    }


    g_ModuleInterface->Print(
        CM_LIGHTGREEN,
        "[AutoLoot] FrameCallback OK"
    );


    return AURIE_SUCCESS;
}
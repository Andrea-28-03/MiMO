#pragma once

#include "CanvasGraph.h"
#include <vector>

namespace mimo
{

class PresetCatalog
{
public:
    static void initCatalogs(std::vector<CatalogItem>& instrumentCatalog,
                             std::vector<CatalogItem>& effectCatalog,
                             std::vector<CatalogItem>& utilityCatalog);

    static void initDefaultStudioGraph(std::vector<CanvasNode>& nodes,
                                       std::vector<CanvasCable>& cables,
                                       const std::vector<CatalogItem>& effectCatalog);

    static void initDefaultLivePresets(std::vector<PolaroidItem>& photoAlbum,
                                       PolaroidItem sceneSlots[5],
                                       const std::vector<CanvasNode>& nodes,
                                       const std::vector<CanvasCable>& cables);
};

} // namespace mimo

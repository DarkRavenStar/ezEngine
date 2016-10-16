#pragma once

#include <EditorPluginAssets/ModelImporter/Importers/PbrtImporter_Declarations.h>

namespace ezModelImporter
{
  namespace PbrtScopeFunctions
  {
    void WorldBegin(Pbrt::ParseContext& context);

    void WorldEnd(Pbrt::ParseContext& context);

    void AttributeBegin(Pbrt::ParseContext& context);
    void AttributeEnd(Pbrt::ParseContext& context);

    void TransformBegin(Pbrt::ParseContext& context);
    void TransformEnd(Pbrt::ParseContext& context);
  }

  namespace PbrtTransformFunctions
  {
    void Translate(Pbrt::ParseContext& context, ezStringView& remainingSceneText);
  }

  namespace PbrtObjectParseFunctions
  {
    void ParseShape(ezStringView type, ezArrayPtr<Pbrt::Parameter> parameters, Pbrt::ParseContext& context, ezModelImporter::Scene& outScene);
    void ParseMaterial(ezStringView type, ezArrayPtr<Pbrt::Parameter> parameters, Pbrt::ParseContext& context, ezModelImporter::Scene& outScene);
  }
}
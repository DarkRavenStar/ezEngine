#include <PCH.h>
#include <EditorPluginAssets/ModelImporter/Importers/PbrtImporter_Context.h>
#include <EditorPluginAssets/ModelImporter/Importers/PbrtImporter_ImportFunctions.h>
#include <EditorPluginAssets/ModelImporter/Importers/PbrtImporter_ParsingFunctions.h>
#include <EditorPluginAssets/ModelImporter/Mesh.h>
#include <EditorPluginAssets/ModelImporter/Material.h>
#include <EditorPluginAssets/ModelImporter/ModelImporter.h>

#include <Foundation/Logging/Log.h>

namespace ezModelImporter
{
  using namespace Pbrt;

  namespace PbrtScopeFunctions
  {
    void WorldBegin(ParseContext& context)
    {
      context.EnterWorld();
    }

    void WorldEnd(ParseContext& context)
    {
      context.ExitWorld();
    }

    void AttributeBegin(ParseContext& context)
    {
      //context.PushActiveMaterial();
      context.PushActiveTransform();
    }

    void AttributeEnd(ParseContext& context)
    {
      //context.PopActiveMaterial();
      context.PopActiveTransform();
    }

    void TransformBegin(ParseContext& context)
    {
      context.PushActiveTransform();
    }

    void TransformEnd(ParseContext& context)
    {
      context.PopActiveTransform();
    }
  }

  namespace PbrtTransformFunctions
  {
    void Translate(ParseContext& context, ezStringView& remainingSceneText)
    {
      ezVec3 translation;
      if (PbrtParseHelper::ParseVec3(remainingSceneText, translation).Succeeded())
      {
        context.PeekActiveTransform().SetGlobalTransform(context.PeekActiveTransform(), ezTransform(translation));
      }
      else
        ezLog::Error("Failed parsing Translate transform command.");
    }
  }

  namespace PbrtObjectParseFunctions
  {
    template<typename T>
    void CopyParamToArray(ezDynamicArray<T>& targetArray, const Parameter& param)
    {
      targetArray.Reserve(param.data.GetCount());
      for (ezUInt32 elem = 0; elem < param.data.GetCount(); ++elem)
        targetArray.PushBack(param.data[elem].Get<T>());
    }

    void ParseShape(ezStringView type, ezArrayPtr<Parameter> parameters, ParseContext& context, ezModelImporter::Scene& outScene)
    {
      // Get/Create node for current transform.
      ObjectHandle parentNode;
      if (!context.PeekActiveTransform().IsIdentical(ezTransform::Identity()))
      {
        // todo: Recycle node if it hasn't changed since last time.
        ezUniquePtr<Node> node = EZ_DEFAULT_NEW(Node);
        node->m_RelativeTransform = context.PeekActiveTransform();
        parentNode = outScene.AddNode(std::move(node));
      }

      ezUniquePtr<ezModelImporter::Mesh> mesh;

      if (type.IsEqual_NoCase("plymesh"))
      {
        if (parameters.GetCount() != 1)
        {
          ezLog::Error("Expected a single parameter for plymesh shape.", type.GetData());
          return;
        }
        if (!parameters[0].name.IsEqual_NoCase("filename") || parameters[0].data.GetCount() != 1 || !parameters[0].data[0].IsA<ezString>())
        {
          ezLog::Error("Expected filename parameter for plymesh shape.", type.GetData());
          return;
        }

        // Load mesh file
        ezStringBuilder meshFilename = context.GetModelFilename();
        meshFilename = meshFilename.GetFileDirectory();
        meshFilename.AppendPath(parameters[0].data[0].Get<ezString>());
        ezUniquePtr<Scene> subScene = ezModelImporter::Importer::GetSingleton()->ImportScene(meshFilename);
        if (!subScene)
        {
          ezLog::Error("Failed to load mesh '%s'.", meshFilename.GetData());
          return;
        }
        mesh = EZ_DEFAULT_NEW(ezModelImporter::Mesh, std::move(*subScene->MergeAllMeshes()));
      }
      else if (type.IsEqual_NoCase("trianglemesh"))
      {
        // Read data.
        ezDynamicArray<int> indices;
        ezDynamicArray<ezVec3> positions;
        ezDynamicArray<ezVec3> normals;
        ezDynamicArray<ezVec3> tangents;
        ezDynamicArray<float> texcoords;
        for (ezUInt32 i = 0; i < parameters.GetCount(); ++i)
        {
          if (parameters[i].name.IsEqual_NoCase("P"))
          {
            if(parameters[i].type != ParamType::VECTOR3)
              ezLog::Warning("PBRT triangle mesh parameter 'P' is not a vec3 array.", type.GetData());
            CopyParamToArray(positions, parameters[i]);
          }
          else if (parameters[i].name.IsEqual_NoCase("N"))
          {
            if (parameters[i].type != ParamType::VECTOR3)
              ezLog::Warning("PBRT triangle mesh parameter 'N' is not a vec3 array.", type.GetData());
            CopyParamToArray(normals, parameters[i]);
          }
          else if (parameters[i].name.IsEqual_NoCase("S"))
          {
            if (parameters[i].type != ParamType::VECTOR3)
              ezLog::Warning("PBRT triangle mesh parameter 'S' is not a vec3 array.", type.GetData());
            CopyParamToArray(tangents, parameters[i]);
          }
          else if (parameters[i].name.IsEqual_NoCase("uv"))
          {
            if (parameters[i].type != ParamType::FLOAT)
              ezLog::Warning("PBRT triangle mesh parameter 'uv' is not a float array.", type.GetData());
            CopyParamToArray(texcoords, parameters[i]);
          }
          else if (parameters[i].name.IsEqual_NoCase("indices"))
          {
            if (parameters[i].type != ParamType::INT)
              ezLog::Warning("PBRT triangle mesh parameter 'indece' is not an int array.", type.GetData());
            CopyParamToArray(indices, parameters[i]);
          }
        }


        if (positions.IsEmpty())
        {
          ezLog::Error("PBRT triangle mesh has no positions.", type.GetData());
          return;
        }
        if (indices.IsEmpty())
        {
          ezLog::Error("PBRT triangle mesh has no indices.", type.GetData());
          return;
        }
        if (indices.GetCount() % 3 != 0)
        {
          ezLog::Error("PBRT triangle mesh has not n*3 indices.", type.GetData());
          return;
        }
        if (texcoords.GetCount() % 2 != 0)
        {
          ezLog::Error("PBRT triangle mesh has not n*2 floats in its texcoord array.", type.GetData());
          return;
        }

        // Build mesh.
        mesh = EZ_DEFAULT_NEW(ezModelImporter::Mesh);
        mesh->AddTriangles(indices.GetCount() / 3);

        ezHybridArray<VertexDataStream*, 4> streams;

        VertexDataStream* positionStream = mesh->AddDataStream(ezGALVertexAttributeSemantic::Position, 3);
        streams.PushBack(positionStream);
        positionStream->ReserveData(positions.GetCount());
        for (ezUInt32 i = 0; i < positions.GetCount(); ++i)
          positionStream->AddValue(positions[i]);

        if (!normals.IsEmpty())
        {
          VertexDataStream* normalStream = mesh->AddDataStream(ezGALVertexAttributeSemantic::Normal, 3);
          streams.PushBack(normalStream);
          normalStream->ReserveData(normals.GetCount());
          for (ezUInt32 i = 0; i < normals.GetCount(); ++i)
            normalStream->AddValue(normals[i]);
        }
        if (!tangents.IsEmpty())
        {
          VertexDataStream* tangentStream = mesh->AddDataStream(ezGALVertexAttributeSemantic::Tangent, 3);
          streams.PushBack(tangentStream);
          tangentStream->ReserveData(tangents.GetCount());
          for (ezUInt32 i = 0; i < tangents.GetCount(); ++i)
            tangentStream->AddValue(tangents[i]);
        }
        if (!texcoords.IsEmpty())
        {
          VertexDataStream* texcoordStream = mesh->AddDataStream(ezGALVertexAttributeSemantic::TexCoord0, 2);
          streams.PushBack(texcoordStream);
          texcoordStream->AddValues(texcoords);
        }

        ezArrayPtr<Mesh::Triangle> triangleList = mesh->GetTriangles();
        for (VertexDataStream* stream : streams)
        {
          ezUInt32 numComponents = stream->GetNumElementsPerVertex();
          for (ezUInt32 i = 0; i < triangleList.GetCount(); ++i)
          {
            stream->SetDataIndex(triangleList[i].m_Vertices[0], indices[i * 3] * numComponents);
            stream->SetDataIndex(triangleList[i].m_Vertices[1], indices[i * 3 + 1] * numComponents);
            stream->SetDataIndex(triangleList[i].m_Vertices[2], indices[i * 3 + 2] * numComponents);
          }
        }
      }
      else
      {
        ezLog::Warning("PBRT '%s' shapes are not supported.", type.GetData());
        return;
      }

      if (mesh)
      {
        // Wire in last node.
        mesh->SetParent(parentNode);

        // Wire in last material. If active material is invalid, still do it to remove any reference to old scene.
        if (context.PeekActiveMaterial())
        {
          if (mesh->GetNumSubMeshes() == 0)
          {
            SubMesh submesh;
            submesh.m_Material = *context.PeekActiveMaterial();
            submesh.m_uiFirstTriangle = 0;
            submesh.m_uiTriangleCount = mesh->GetNumTriangles();
            mesh->AddSubMesh(submesh);
          }
          else
          {
            for (ezUInt32 i = 0; i < mesh->GetNumSubMeshes(); ++i)
              mesh->GetSubMesh(i).m_Material = *context.PeekActiveMaterial();
          }
        }
        else
        {
          for (ezUInt32 i = 0; i < mesh->GetNumSubMeshes(); ++i)
            mesh->GetSubMesh(i).m_Material = MaterialHandle();
        }

        // Add to output scene.
        outScene.AddMesh(std::move(mesh));
      }
    }

    void ReadMaterialParameter(Material::SemanticHint::Enum semantic, const char* materialParameter, Material& material, ezArrayPtr<Parameter> parameters, ezVariant default)
    {
      for (Parameter& param : parameters)
      {
        if (!param.data.IsEmpty() && param.name.IsEqual_NoCase(materialParameter))
        {
          if (param.type == ParamType::TEXTURE)
          {
            // TODO: Search texture
            //material.m_Textures.PushBack()
          }
          else
          {
            material.m_Properties.ExpandAndGetRef() = Material::Property(semantic, materialParameter, param.data[0]);
          }
          return;
        }
      }

      if (default.IsValid())
        material.m_Properties.PushBack(Material::Property(semantic, materialParameter, default));
    }

    void ParseMaterial(ezStringView type, ezArrayPtr<Parameter> parameters, ParseContext& context, ezModelImporter::Scene& outScene)
    {
      ezUniquePtr<Material> newMaterial = EZ_DEFAULT_NEW(Material);

      newMaterial->m_Name = "";
      newMaterial->m_Properties.PushBack(Material::Property("type", type));

      // http://www.pbrt.org/fileformat.html#materials
      if (type.IsEqual_NoCase("glass"))
      {
        ReadMaterialParameter(Material::SemanticHint::REFLECTIVITY, "Kr", *newMaterial, parameters, 1.0f); // The reflectivity of the surface.
        ReadMaterialParameter(Material::SemanticHint::OPACITY, "Kt", *newMaterial, parameters, 1.0f); // The transmissivity of the surface.
        ReadMaterialParameter(Material::SemanticHint::REFRACTIONINDEX, "index", *newMaterial, parameters, 1.5f); // The index of refraction of the inside of the object. (pbrt implicitly assumes that the exterior of objects is a vacuum, with IOR of 1.)
      }
      else if (type.IsEqual_NoCase("KdSubsurface"))
      {
        ReadMaterialParameter(Material::SemanticHint::DIFFUSE, "Kd", *newMaterial, parameters, 0.5f); // Diffuse scattering coefficient used to derive scattering properties.
        ReadMaterialParameter(Material::SemanticHint::UNKNOWN, "meanfreepath", *newMaterial, parameters, 1.0f); // Average distance light travels in the medium before scattering.
        ReadMaterialParameter(Material::SemanticHint::REFRACTIONINDEX, "index", *newMaterial, parameters, 1.0f); // The index of refraction inside the object.
        ReadMaterialParameter(Material::SemanticHint::REFLECTIVITY, "Kr", *newMaterial, parameters, 1.0f); // Specular reflection term; this coefficient is modulated with the dielectric Fresnel equation to give the amount of specular reflection.
      }
      if (type.IsEqual_NoCase("matte"))
      {
        ReadMaterialParameter(Material::SemanticHint::DIFFUSE, "Kd", *newMaterial, parameters, 0.5f); // The diffuse reflectivity of the surface.
        ReadMaterialParameter(Material::SemanticHint::UNKNOWN, "sigma", *newMaterial, parameters, 0.0f); // The sigma parameter for the Oren-Nayar model, in degrees. If this is zero, the surface exhibits pure Lambertian reflection.
      }
      else if (type.IsEqual_NoCase("measured"))
      {
        ReadMaterialParameter(Material::SemanticHint::UNKNOWN, "filename", *newMaterial, parameters, ezVariant()); // The diffuse reflectivity of the surface.
      }
      else if (type.IsEqual_NoCase("metal"))
      {
        ReadMaterialParameter(Material::SemanticHint::REFRACTIONINDEX, "eta", *newMaterial, parameters, 0.5f); // Index of refraction to use in computing the material's reflectance.
        ReadMaterialParameter(Material::SemanticHint::REFLECTIVITY, "k", *newMaterial, parameters, 0.5f); // Absorption coefficient to use in computing the material's reflectance.
        ReadMaterialParameter(Material::SemanticHint::ROUGHNESS, "roughness", *newMaterial, parameters, 0.01f); // Roughness of the material's microfacet distribution. Smaller values become increasingly close to perfect specular reflection. This value should be between zero and one.
      }
      else if (type.IsEqual_NoCase("mirror"))
      {
        ReadMaterialParameter(Material::SemanticHint::REFLECTIVITY, "Kr", *newMaterial, parameters, 0.5f); // The reflectivity of the mirror. This value can be used to make colored or dim reflections.
      }
      else if (type.IsEqual_NoCase("mixture"))
      {
        ReadMaterialParameter(Material::SemanticHint::UNKNOWN, "amount", *newMaterial, parameters, 0.5f); // Weighting factor for the blend between materials. A value of zero corresponds to just "namedmaterial1", a value of one corredponds to just "namedmaterial2", and values in between interpolate linearly.
        ReadMaterialParameter(Material::SemanticHint::UNKNOWN, "namedmaterial1", *newMaterial, parameters, ezVariant()); // Name of first material to be interpolated between.
        ReadMaterialParameter(Material::SemanticHint::UNKNOWN, "namedmaterial2", *newMaterial, parameters, ezVariant()); // Name of second material to be interpolated between.
      }
      else if (type.IsEqual_NoCase("plastic"))
      {
        ReadMaterialParameter(Material::SemanticHint::DIFFUSE, "Kd", *newMaterial, parameters, 0.25f); // The diffuse reflectivity of the surface.
        ReadMaterialParameter(Material::SemanticHint::METALLIC, "Ks", *newMaterial, parameters, 0.25f); // The specular reflectivity of the surface.
        ReadMaterialParameter(Material::SemanticHint::ROUGHNESS, "roughness", *newMaterial, parameters, 0.1f); // The roughness of the surface, from 0 to 1. Larger values result in larger, more blurry highlights.
      }
      else if (type.IsEqual_NoCase("shinymetal"))
      {
        ReadMaterialParameter(Material::SemanticHint::ROUGHNESS, "roughness", *newMaterial, parameters, 0.1f); // The roughness of the surface.
        ReadMaterialParameter(Material::SemanticHint::ROUGHNESS, "Ks", *newMaterial, parameters, 1.0f); // The coefficient of glossy reflection.
        ReadMaterialParameter(Material::SemanticHint::METALLIC, "Kr", *newMaterial, parameters, 1.0f); // The coefficient of specular reflection.
      }
      else if (type.IsEqual_NoCase("substrate"))
      {
        ReadMaterialParameter(Material::SemanticHint::DIFFUSE, "Kd", *newMaterial, parameters, 0.5f); // The coefficient of diffuse reflection.
        ReadMaterialParameter(Material::SemanticHint::METALLIC, "Ks", *newMaterial, parameters, 0.5f); // The coefficient of specular reflection.
        ReadMaterialParameter(Material::SemanticHint::ROUGHNESS, "uroughness", *newMaterial, parameters, 0.1f); // The roughness of the surface in the u direction.
        ReadMaterialParameter(Material::SemanticHint::ROUGHNESS, "vroughness", *newMaterial, parameters, 0.1f); // The roughness of the surface in the v direction.
      }
      else if (type.IsEqual_NoCase("subsurface"))
      {
        ReadMaterialParameter(Material::SemanticHint::UNKNOWN, "name", *newMaterial, parameters, ezVariant()); // Name of measured subsurface scattering coefficients. See the file src/core/volume.cpp in the pbrt distribution for all of the measurements that are available.
        ReadMaterialParameter(Material::SemanticHint::UNKNOWN, "sigma_a", *newMaterial, parameters, ezVec3(0.0011f, 0.0024f, 0.014f)); // Absorption coefficient of the volume, measured in mm^-1.
        ReadMaterialParameter(Material::SemanticHint::UNKNOWN, "sigma_prime_s", *newMaterial, parameters, ezVec3(2.55f, 3.12f, 3.77f)); // Reduced scattering coefficient of the volume, measured in mm^-1.
        ReadMaterialParameter(Material::SemanticHint::UNKNOWN, "scale", *newMaterial, parameters, 1.0f); // Scale factor that is applied to sigma_a and sigma_prime_s. This is particularly useful when the scene is not measured in mm and the coefficients need to be scaled accordingly. For example, if the scene is modeled in meters, then a scale factor of 0.001 would be appropriate.
        ReadMaterialParameter(Material::SemanticHint::REFRACTIONINDEX, "index", *newMaterial, parameters, 1.3f); // Index of refraction of the scattering volume.
      }
      else if (type.IsEqual_NoCase("translucent"))
      {
        ReadMaterialParameter(Material::SemanticHint::DIFFUSE, "Kd", *newMaterial, parameters, 0.25f); // The coefficient of diffuse reflection and transmission.
        ReadMaterialParameter(Material::SemanticHint::ROUGHNESS, "Ks", *newMaterial, parameters, 0.25f); // The coefficient of specular reflection and transmission.
        ReadMaterialParameter(Material::SemanticHint::METALLIC, "reflect", *newMaterial, parameters, 0.5f); // Fraction of light reflected.
        ReadMaterialParameter(Material::SemanticHint::OPACITY, "transmit", *newMaterial, parameters, 0.5f); // Fraction of light transmitted.
        ReadMaterialParameter(Material::SemanticHint::ROUGHNESS, "roughness", *newMaterial, parameters, 0.1f); // The roughness of the surface. (This value should be between 0 and 1).
      }
      else if (type.IsEqual_NoCase("uber"))
      {
        ReadMaterialParameter(Material::SemanticHint::DIFFUSE, "Kd", *newMaterial, parameters, 0.25f); // The coefficient of diffuse reflection.
        ReadMaterialParameter(Material::SemanticHint::ROUGHNESS, "Ks", *newMaterial, parameters, 0.25f); // The coefficient of glossy reflection.
        ReadMaterialParameter(Material::SemanticHint::METALLIC, "Kr", *newMaterial, parameters, 0.25f); // The coefficient of specular reflection.
        ReadMaterialParameter(Material::SemanticHint::ROUGHNESS, "roughness", *newMaterial, parameters, 0.1f); // The roughness of the surface.
        ReadMaterialParameter(Material::SemanticHint::REFRACTIONINDEX, "index", *newMaterial, parameters, 0.1f); // Index of refraction of the surface. This value is used in both the microfacet model for specular reflection as well as for computing a Fresnel reflection term for perfect specular reflection.
        ReadMaterialParameter(Material::SemanticHint::OPACITY, "opacity", *newMaterial, parameters, 1.0f); // The opacity of the surface.Note that when less than one, the uber material transmits light without refracting it.
      }

      // Put in all parameters not loaded yet.
      for (Parameter& param : parameters)
      {
        bool found = false;
        if (param.type == ParamType::TEXTURE)
        {
          for (ezUInt32 i = 0; i < newMaterial->m_Textures.GetCount(); ++i)
          {
            if (param.name.IsEqual_NoCase(newMaterial->m_Textures[i].m_Semantic.GetData()))
            {
              found = true;
              break;
            }
          }
          if (!found)
          {
            // TODO add texture.
          }
        }
        else
        {
          for (ezUInt32 i = 0; i < newMaterial->m_Properties.GetCount(); ++i)
          {
            if (param.name.IsEqual_NoCase(newMaterial->m_Properties[i].m_Semantic.GetData()))
            {
              found = true;
              break;
            }
          }
          if (!found)
          {
            newMaterial->m_Properties.PushBack(Material::Property(ezString(param.name), param.data[0]));
          }
        }
      }

      context.PushActiveMaterial(outScene.AddMaterial(std::move(newMaterial)));
    }
  }
}
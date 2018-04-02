#include <PCH.h>
#include <EditorPluginAssets/SkeletonAsset/SkeletonAsset.h>
#include <EditorFramework/Assets/AssetCurator.h>
#include <EditorFramework/EditorApp/EditorApp.moc.h>
#include <Foundation/Utilities/Progress.h>

//////////////////////////////////////////////////////////////////////////

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezSkeletonAssetProperties, 1, ezRTTIDefaultAllocator<ezSkeletonAssetProperties>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("File", m_sAnimationFile),
  }
  EZ_END_PROPERTIES
}
EZ_END_DYNAMIC_REFLECTED_TYPE

ezSkeletonAssetProperties::ezSkeletonAssetProperties()
{
}

//////////////////////////////////////////////////////////////////////////

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezSkeletonAssetDocument, 1, ezRTTINoAllocator);
EZ_END_DYNAMIC_REFLECTED_TYPE

ezSkeletonAssetDocument::ezSkeletonAssetDocument(const char* szDocumentPath)
  : ezSimpleAssetDocument<ezSkeletonAssetProperties>(szDocumentPath, true)
{
}

ezStatus ezSkeletonAssetDocument::InternalTransformAsset(ezStreamWriter& stream, const char* szOutputTag, const char* szPlatform, const ezAssetFileHeader& AssetHeader, bool bTriggeredManually)
{
  ezProgressRange range("Transforming Asset", 2, false);

  ezSkeletonAssetProperties* pProp = GetProperties();

  range.SetStepWeighting(0, 0.9);
  range.BeginNextStep("Importing Mesh");

  range.BeginNextStep("Writing Result");

  return ezStatus(EZ_SUCCESS);
}

ezStatus ezSkeletonAssetDocument::InternalCreateThumbnail(const ezAssetFileHeader& AssetHeader)
{
  ezStatus status = ezAssetDocument::RemoteCreateThumbnail(AssetHeader);
  return status;
}

//////////////////////////////////////////////////////////////////////////

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezSkeletonAssetDocumentGenerator, 1, ezRTTIDefaultAllocator<ezSkeletonAssetDocumentGenerator>)
EZ_END_DYNAMIC_REFLECTED_TYPE

ezSkeletonAssetDocumentGenerator::ezSkeletonAssetDocumentGenerator()
{
  AddSupportedFileType("fbx");
}

ezSkeletonAssetDocumentGenerator::~ezSkeletonAssetDocumentGenerator()
{

}

void ezSkeletonAssetDocumentGenerator::GetImportModes(const char* szParentDirRelativePath, ezHybridArray<ezAssetDocumentGenerator::Info, 4>& out_Modes) const
{
  ezStringBuilder baseOutputFile = szParentDirRelativePath;
  baseOutputFile.ChangeFileExtension(GetDocumentExtension());

  {
    ezAssetDocumentGenerator::Info& info = out_Modes.ExpandAndGetRef();
    info.m_Priority = ezAssetDocGeneratorPriority::Undecided;
    info.m_sName = "SkeletonImport";
    info.m_sOutputFileParentRelative = baseOutputFile;
    info.m_sIcon = ":/AssetIcons/Skeleton.png";
  }
}

ezStatus ezSkeletonAssetDocumentGenerator::Generate(const char* szDataDirRelativePath, const ezAssetDocumentGenerator::Info& info, ezDocument*& out_pGeneratedDocument)
{
  auto pApp = ezQtEditorApp::GetSingleton();

  out_pGeneratedDocument = pApp->CreateOrOpenDocument(true, info.m_sOutputFileAbsolute, false, false);
  if (out_pGeneratedDocument == nullptr)
    return ezStatus("Could not create target document");

  ezSkeletonAssetDocument* pAssetDoc = ezDynamicCast<ezSkeletonAssetDocument*>(out_pGeneratedDocument);
  if (pAssetDoc == nullptr)
    return ezStatus("Target document is not a valid ezSkeletonAssetDocument");

  auto& accessor = pAssetDoc->GetPropertyObject()->GetTypeAccessor();
  accessor.SetValue("File", szDataDirRelativePath);

  return ezStatus(EZ_SUCCESS);
}

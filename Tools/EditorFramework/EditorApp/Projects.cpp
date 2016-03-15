#include <PCH.h>
#include <EditorFramework/EditorApp/EditorApp.moc.h>
#include <GuiFoundation/UIServices/ImageCache.moc.h>
#include <GuiFoundation/Dialogs/ModifiedDocumentsDlg.moc.h>

void UpdateInputDynamicEnumValues();

void ezQtEditorApp::CloseProject()
{
  QMetaObject::invokeMethod(this, "SlotQueuedCloseProject", Qt::ConnectionType::QueuedConnection);
}

void ezQtEditorApp::SlotQueuedCloseProject()
{
  // purge the image loading queue when a project is closed, but keep the existing cache
  ezQtImageCache::StopRequestProcessing(false);

  ezToolsProject::CloseProject();

  // enable image loading again, the queue is purged now
  ezQtImageCache::EnableRequestProcessing();
}

void ezQtEditorApp::OpenProject(const char* szProject)
{
  QMetaObject::invokeMethod(this, "SlotQueuedOpenProject", Qt::ConnectionType::QueuedConnection, Q_ARG(QString, szProject));
}

void ezQtEditorApp::SlotQueuedOpenProject(QString sProject)
{
  CreateOrOpenProject(false, sProject.toUtf8().data());
}


void ezQtEditorApp::CreateOrOpenProject(bool bCreate, const char* szFile)
{
  if (ezToolsProject::IsProjectOpen() && ezToolsProject::GetInstance()->GetProjectFile() == szFile)
  {
    ezUIServices::MessageBoxInformation("The selected project is already open");
    return;
  }

  if (!ezToolsProject::CanCloseProject())
    return;

  ezStatus res;
  if (bCreate)
    res = ezToolsProject::CreateProject(szFile);
  else
    res = ezToolsProject::OpenProject(szFile);

  if (res.m_Result.Failed())
  {
    ezStringBuilder s;
    s.Format("Failed to open project:\n'%s'", szFile);

    ezUIServices::MessageBoxStatus(res, s);
    return;
  }

  const ezRecentFilesList allDocs = LoadOpenDocumentsList();

  for (auto& doc : allDocs.GetFileList())
  {
    OpenDocument(doc);
  }
}

void ezQtEditorApp::ProjectEventHandler(const ezToolsProject::Event& r)
{
  switch (r.m_Type)
  {
  case ezToolsProject::Event::Type::ProjectOpened:
    {
      SetupDataDirectories();
      ReadEnginePluginConfig();
      ReadTagRegistry();
      UpdateInputDynamicEnumValues();

      // tell the engine process which file system and plugin configuration to use
      ezEditorEngineProcessConnection::GetInstance()->SetFileSystemConfig(m_FileSystemConfig);
      ezEditorEngineProcessConnection::GetInstance()->SetPluginConfig(m_EnginePluginConfig);

      if (ezEditorEngineProcessConnection::GetInstance()->RestartProcess().Failed())
      {
        ezLog::Error("Failed to start the engine process. Project loading incomplete.");
      }

      m_AssetCurator.Initialize(m_FileSystemConfig);

      m_sLastDocumentFolder = ezToolsProject::GetInstance()->GetProjectFile();
      m_sLastProjectFolder = ezToolsProject::GetInstance()->GetProjectFile();
    }
    // fall through

  case ezToolsProject::Event::Type::ProjectClosing:
    {
      s_RecentProjects.Insert(ezToolsProject::GetInstance()->GetProjectFile());
      SaveSettings();
    }
    break;
  case ezToolsProject::Event::Type::ProjectClosed:
    {
      ezEditorEngineProcessConnection::GetInstance()->ShutdownProcess();

      m_AssetCurator.Deinitialize();

      // make sure to clear all project settings when a project is closed
      s_ProjectSettings.Clear();

      ezApplicationConfig::SetProjectDirectory("");

      s_ReloadProjectRequiredReasons.Clear();
      UpdateGlobalStatusBarMessage();
    }
    break;
  }
}

void ezQtEditorApp::ProjectRequestHandler(ezToolsProject::Request& r)
{
  switch (r.m_Type)
  {
  case ezToolsProject::Request::Type::CanProjectClose:
    {
      if (r.m_bProjectCanClose == false)
        return;

      ezHybridArray<ezDocument*, 32> ModifiedDocs;

      for (ezDocumentManager* pMan : ezDocumentManager::GetAllDocumentManagers())
      {
        for (ezDocument* pDoc : pMan->GetAllDocuments())
        {
          if (pDoc->IsModified())
            ModifiedDocs.PushBack(pDoc);
        }
      }

      if (!ModifiedDocs.IsEmpty())
      {
        ezModifiedDocumentsDlg dlg(QApplication::activeWindow(), ModifiedDocs);
        if (dlg.exec() == 0)
          r.m_bProjectCanClose = false;
      }
    }
    return;
  }
}


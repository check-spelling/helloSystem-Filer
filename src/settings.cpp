/*

    Copyright (C) 2013  Hong Jen Yee (PCMan) <pcman.tw@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include "settings.h"
#include <QDir>
#include <QFile>
#include <QStringBuilder>
#include <QSettings>
#include <QApplication>
#include "desktopwindow.h"
#include "utilities.h"
// #include <QDesktopServices>

using namespace Filer;

inline static const char* bookmarkOpenMethodToString(OpenDirTargetType value);
inline static OpenDirTargetType bookmarkOpenMethodFromString(const QString str);

inline static const char* wallpaperModeToString(int value);
inline static int wallpaperModeFromString(const QString str);

inline static const char* viewModeToString(Fm::FolderView::ViewMode value);
inline static Fm::FolderView::ViewMode viewModeFromString(const QString str);

inline static const char* sidePaneModeToString(Fm::SidePane::Mode value);
inline static Fm::SidePane::Mode sidePaneModeFromString(const QString& str);

inline static const char* sortOrderToString(Qt::SortOrder order);
inline static Qt::SortOrder sortOrderFromString(const QString str);

inline static const char* sortColumnToString(Fm::FolderModel::ColumnId value);
inline static Fm::FolderModel::ColumnId sortColumnFromString(const QString str);

Settings::Settings():
  QObject(),
  supportTrash_(Fm::uriExists("trash:///")), // check if trash:/// is supported
  fallbackIconThemeName_(),
  useFallbackIconTheme_(QIcon::themeName().isEmpty() || QIcon::themeName() == "hicolor"),
  bookmarkOpenMethod_(OpenInCurrentTab),
  suCommand_(),
  terminal_(),
  mountOnStartup_(true),
  mountRemovable_(true),
  autoRun_(true),
  closeOnUnmount_(true), // probono: Closes the window when the device/network share is unmounted. This is awesome!
  wallpaperMode_(0),
  wallpaper_(),
  desktopBgColor_(),
  desktopFgColor_(),
  desktopShadowColor_(),
  desktopShowHidden_(false),
  desktopSortOrder_(Qt::AscendingOrder),
  desktopSortColumn_(Fm::FolderModel::ColumnFileName),
  alwaysShowTabs_(false), // probono: Do not show tabs when there is only one tab
  showTabClose_(true),
  rememberWindowSize_(true),
  fixedWindowWidth_(640),
  fixedWindowHeight_(480),
  lastWindowWidth_(640),
  lastWindowHeight_(480),
  lastWindowMaximized_(false),
  splitterPos_(120),
  sidePaneMode_(Fm::SidePane::ModePlaces),
  viewMode_(Fm::FolderView::IconMode),
  showHidden_(false),
  sortOrder_(Qt::AscendingOrder),
  sortColumn_(Fm::FolderModel::ColumnFileName),
  sortFolderFirst_(true),
  showFilter_(false),
  spatialMode_(false),
  dirInfoWrite_(true),
  // settings for use with libfm
  singleClick_(false),
  autoSelectionDelay_(600),
  confirmDelete_(true),
  noUsbTrash_(false),
  confirmTrash_(false),
  quickExec_(true), // probono: By default execute executable files without asking the user questions
  showThumbnails_(true),
  archiver_(),
  siUnit_(false),
  bigIconSize_(48),
  smallIconSize_(24),
  sidePaneIconSize_(16), // probono: Use 16 instead of 24
  thumbnailIconSize_(128) {
}

Settings::~Settings() {

}

QString Settings::profileDir(QString profile, bool useFallback) {
  // NOTE: it's a shame that QDesktopServices does not handle XDG_CONFIG_HOME
  // try user-specific config file first
  QString dirName = QLatin1String(qgetenv("XDG_CONFIG_HOME"));
  if (dirName.isEmpty())
    dirName = QDir::homePath() % QLatin1String("/.config");
  dirName = dirName % "/filer/" % profile;
  QDir dir(dirName);

  // if user config dir does not exist, try system-wide config dirs instead
  if(!dir.exists() && useFallback) {
    QString fallbackDir;
    for(const char* const* configDir = g_get_system_config_dirs(); *configDir; ++configDir) {
      fallbackDir = QString(*configDir) % "/filer/" % profile;
      dir.setPath(fallbackDir);
      if(dir.exists()) {
	dirName = fallbackDir;
	break;
      }
    }
  }
  return dirName;
}

bool Settings::load(QString profile) {
  profileName_ = profile;
  QString fileName = profileDir(profile, true) % "/settings.conf";
  return loadFile(fileName);
}

bool Settings::save(QString profile) {
  QString fileName = profileDir(profile.isEmpty() ? profileName_ : profile) % "/settings.conf";
  return saveFile(fileName);
}

bool Settings::loadFile(QString filePath) {
  QSettings settings(filePath, QSettings::IniFormat);
  settings.beginGroup("System");
  fallbackIconThemeName_ = settings.value("FallbackIconThemeName").toString();
  if(fallbackIconThemeName_.isEmpty()) {
    // FIXME: we should choose one from installed icon themes or get
    // the value from XSETTINGS instead of hard code a fallback value.
    fallbackIconThemeName_ = "elementary"; // fallback icon theme name
  }
  suCommand_ = settings.value("SuCommand", "gksu %s").toString();
  setTerminal(settings.value("Terminal", "xterm").toString()); // probono: TODO: Check if qterminal is on the $PATH and if it is, prefer it over xterm
  setArchiver(settings.value("Archiver", "file-roller").toString());
  setSiUnit(settings.value("SIUnit", false).toBool());

  setOnlyUserTemplates(settings.value("OnlyUserTemplates", false).toBool());
  setTemplateTypeOnce(settings.value("OemplateTypeOnce", false).toBool());
  setTemplateRunApp(settings.value("TemplateRunApp", false).toBool());

  settings.endGroup();

  settings.beginGroup("Behavior");
  bookmarkOpenMethod_ = bookmarkOpenMethodFromString(settings.value("BookmarkOpenMethod").toString());
  spatialMode_ = settings.value("SpatialMode", false).toBool();
  dirInfoWrite_ = settings.value("DirInfoWrite", true).toBool();
  // settings for use with libfm
  singleClick_ = settings.value("SingleClick", false).toBool();
  autoSelectionDelay_ = settings.value("AutoSelectionDelay", 600).toInt();
  confirmDelete_ = settings.value("ConfirmDelete", true).toBool();
  setNoUsbTrash(settings.value("NoUsbTrash", false).toBool());
  confirmTrash_ = settings.value("ConfirmTrash", false).toBool();
  setQuickExec(settings.value("QuickExec", true).toBool()); // probono: Do not ask what to do with executable files when they are double-clicked and have the executable bit set
  // bool thumbnailLocal_;
  // bool thumbnailMax;
  settings.endGroup();

  settings.beginGroup("Desktop");
  wallpaperMode_ = wallpaperModeFromString(settings.value("WallpaperMode").toString());
  wallpaper_ = settings.value("Wallpaper").toString();
  desktopBgColor_.setNamedColor(settings.value("BgColor", "#4e7fb4").toString());
  desktopFgColor_.setNamedColor(settings.value("FgColor", "#ffffff").toString());
  desktopShadowColor_.setNamedColor(settings.value("ShadowColor", "#000000").toString());
  if(settings.contains("Font"))
    desktopFont_.fromString(settings.value("Font").toString());
  else
    desktopFont_ = QApplication::font();
  desktopShowHidden_ = settings.value("ShowHidden", false).toBool();

  desktopSortOrder_ = sortOrderFromString(settings.value("SortOrder").toString());
  desktopSortColumn_ = sortColumnFromString(settings.value("SortColumn").toString());
  settings.endGroup();

  settings.beginGroup("Volume");
  mountOnStartup_ = settings.value("MountOnStartup", true).toBool();
  mountRemovable_ = settings.value("MountRemovable", true).toBool();
  autoRun_ = settings.value("AutoRun", true).toBool();
  closeOnUnmount_ = settings.value("CloseOnUnmount", true).toBool();
  settings.endGroup();

  settings.beginGroup("Thumbnail");
  showThumbnails_ = settings.value("ShowThumbnails", true).toBool();
  setMaxThumbnailFileSize(settings.value("MaxThumbnailFileSize", 4096).toInt());
  setThumbnailLocalFilesOnly(settings.value("ThumbnailLocalFilesOnly", true).toBool());
  settings.endGroup();

  settings.beginGroup("FolderView");
  viewMode_ = viewModeFromString(settings.value("Mode", Fm::FolderView::IconMode).toString());
  showHidden_ = settings.value("ShowHidden", false).toBool();
  sortOrder_ = sortOrderFromString(settings.value("SortOrder").toString());
  sortColumn_ = sortColumnFromString(settings.value("SortColumn").toString());
  sortFolderFirst_ = settings.value("SortFolderFirst", true).toBool();
  showFilter_ = settings.value("ShowFilter", false).toBool();

  setBackupAsHidden(settings.value("BackupAsHidden", false).toBool());
  showFullNames_ = settings.value("ShowFullNames", false).toBool();
  shadowHidden_ = settings.value("ShadowHidden", false).toBool();

  // override config in libfm's FmConfig
  bigIconSize_ = settings.value("BigIconSize", 36).toInt(); // probono: 36 instead of 48
  smallIconSize_ = settings.value("SmallIconSize", 16).toInt(); // probono: 16 instead of 24
  sidePaneIconSize_ = settings.value("SidePaneIconSize", 12).toInt(); // probono: 12 instead of 24
  thumbnailIconSize_ = settings.value("ThumbnailIconSize", 128).toInt();
  settings.endGroup();

  settings.beginGroup("Window");
  fixedWindowWidth_ = settings.value("FixedWidth", 640).toInt();
  fixedWindowHeight_ = settings.value("FixedHeight", 480).toInt();
  lastWindowWidth_ = settings.value("LastWindowWidth", 640).toInt();
  lastWindowHeight_ = settings.value("LastWindowHeight", 480).toInt();
  lastWindowMaximized_ = settings.value("LastWindowMaximized", false).toBool();
  rememberWindowSize_ = settings.value("RememberWindowSize", true).toBool();
  alwaysShowTabs_ = settings.value("AlwaysShowTabs", false).toBool(); // probonod: false instead of true, here too (like further above)
  showTabClose_ = settings.value("ShowTabClose", true).toBool();
  splitterPos_ = settings.value("SplitterPos", 150).toInt();
  sidePaneMode_ = sidePaneModeFromString(settings.value("SidePaneMode").toString());
  settings.endGroup();

  return true;
}

bool Settings::saveFile(QString filePath) {
  QSettings settings(filePath, QSettings::IniFormat);

  settings.beginGroup("System");
  settings.setValue("FallbackIconThemeName", fallbackIconThemeName_);
  settings.setValue("SuCommand", suCommand_);
  settings.setValue("Terminal", terminal_);
  settings.setValue("Archiver", archiver_);
  settings.setValue("SIUnit", siUnit_);

  settings.setValue("OnlyUserTemplates", onlyUserTemplates_);
  settings.setValue("OemplateTypeOnce", templateTypeOnce_);
  settings.setValue("TemplateRunApp", templateRunApp_);

  settings.endGroup();

  settings.beginGroup("Behavior");
  settings.setValue("BookmarkOpenMethod", bookmarkOpenMethodToString(bookmarkOpenMethod_));
  // settings for use with libfm
  settings.setValue("SingleClick", singleClick_);
  settings.setValue("SpatialMode", spatialMode_);
  settings.setValue("DirInfoWrite", dirInfoWrite_);
  settings.setValue("AutoSelectionDelay", autoSelectionDelay_);
  settings.setValue("ConfirmDelete", confirmDelete_);
  settings.setValue("NoUsbTrash", noUsbTrash_);
  settings.setValue("ConfirmTrash", confirmTrash_);
  settings.setValue("QuickExec", quickExec_);
  // bool thumbnailLocal_;
  // bool thumbnailMax;
  settings.endGroup();

  settings.beginGroup("Desktop");
  settings.setValue("WallpaperMode", wallpaperModeToString(wallpaperMode_));
  settings.setValue("Wallpaper", wallpaper_);
  settings.setValue("BgColor", desktopBgColor_.name());
  settings.setValue("FgColor", desktopFgColor_.name());
  settings.setValue("ShadowColor", desktopShadowColor_.name());
  settings.setValue("Font", desktopFont_.toString());
  settings.setValue("ShowHidden", desktopShowHidden_);
  settings.setValue("SortOrder", sortOrderToString(desktopSortOrder_));
  settings.setValue("SortColumn", sortColumnToString(desktopSortColumn_));
  settings.endGroup();

  settings.beginGroup("Volume");
  settings.setValue("MountOnStartup", mountOnStartup_);
  settings.setValue("MountRemovable", mountRemovable_);
  settings.setValue("AutoRun", autoRun_);
  settings.setValue("CloseOnUnmount", closeOnUnmount_);
  settings.endGroup();

  settings.beginGroup("Thumbnail");
  settings.setValue("ShowThumbnails", showThumbnails_);
  settings.setValue("MaxThumbnailFileSize", maxThumbnailFileSize());
  settings.setValue("ThumbnailLocalFilesOnly", thumbnailLocalFilesOnly());
  settings.endGroup();

  settings.beginGroup("FolderView");
  settings.setValue("Mode", viewModeToString(viewMode_));
  settings.setValue("ShowHidden", showHidden_);
  settings.setValue("SortOrder", sortOrderToString(sortOrder_));
  settings.setValue("SortColumn", sortColumnToString(sortColumn_));
  settings.setValue("SortFolderFirst", sortFolderFirst_);
  settings.setValue("ShowFilter", showFilter_);

  settings.setValue("BackupAsHidden", backupAsHidden_);
  settings.setValue("ShowFullNames", showFullNames_);
  settings.setValue("ShadowHidden", shadowHidden_);

  // override config in libfm's FmConfig
  settings.setValue("BigIconSize", bigIconSize_);
  settings.setValue("SmallIconSize", smallIconSize_);
  settings.setValue("SidePaneIconSize", sidePaneIconSize_);
  settings.setValue("ThumbnailIconSize", thumbnailIconSize_);
  settings.endGroup();

  settings.beginGroup("Window");
  settings.setValue("FixedWidth", fixedWindowWidth_);
  settings.setValue("FixedHeight", fixedWindowHeight_);
  settings.setValue("LastWindowWidth", lastWindowWidth_);
  settings.setValue("LastWindowHeight", lastWindowHeight_);
  settings.setValue("LastWindowMaximized", lastWindowMaximized_);
  settings.setValue("RememberWindowSize", rememberWindowSize_);
  settings.setValue("AlwaysShowTabs", alwaysShowTabs_);
  settings.setValue("ShowTabClose", showTabClose_);
  settings.setValue("SplitterPos", splitterPos_);
  settings.setValue("SidePaneMode", sidePaneModeToString(sidePaneMode_));
  settings.endGroup();
  return true;
}

static const char* bookmarkOpenMethodToString(OpenDirTargetType value) {
  switch(value) {
  case OpenInCurrentTab:
  default:
    return "current_tab";
  case OpenInNewTab:
    return "new_tab";
  case OpenInNewWindow:
    return "new_window";
  case OpenInLastActiveWindow:
    return "last_window";
  }
  return "";
}

static OpenDirTargetType bookmarkOpenMethodFromString(const QString str) {

  if(str == QStringLiteral("new_tab"))
    return OpenInNewTab;
  else if(str == QStringLiteral("new_window"))
    return OpenInNewWindow;
  else if(str == QStringLiteral("last_window"))
    return OpenInLastActiveWindow;
  return OpenInCurrentTab;
}

static const char* viewModeToString(Fm::FolderView::ViewMode value) {
  const char* ret;
  switch(value) {
    case Fm::FolderView::IconMode:
    default:
      ret = "icon";
      break;
    case Fm::FolderView::CompactMode:
      ret = "compact";
      break;
    case Fm::FolderView::DetailedListMode:
      ret = "detailed";
      break;
    case Fm::FolderView::ThumbnailMode:
      ret = "thumbnail";
      break;
  }
  return ret;
}

Fm::FolderView::ViewMode viewModeFromString(const QString str) {
  Fm::FolderView::ViewMode ret;
  if(str == "icon")
    ret = Fm::FolderView::IconMode;
  else if(str == "compact")
    ret = Fm::FolderView::CompactMode;
  else if(str == "detailed")
    ret = Fm::FolderView::DetailedListMode;
  else if(str == "thumbnail")
    ret = Fm::FolderView::ThumbnailMode;
  else
    ret = Fm::FolderView::IconMode;
  return ret;
}

static const char* sortOrderToString(Qt::SortOrder order) {
  return (order == Qt::DescendingOrder ? "descending" : "ascending");
}

static Qt::SortOrder sortOrderFromString(const QString str) {
  return (str == "descending" ? Qt::DescendingOrder : Qt::AscendingOrder);
}

static const char* sortColumnToString(Fm::FolderModel::ColumnId value) {
  const char* ret;
  switch(value) {
    case Fm::FolderModel::ColumnFileName:
    default:
      ret = "name";
      break;
    case Fm::FolderModel::ColumnFileType:
      ret = "type";
      break;
    case Fm::FolderModel::ColumnFileSize:
      ret = "size";
      break;
    case Fm::FolderModel::ColumnFileMTime:
      ret = "mtime";
      break;
    case Fm::FolderModel::ColumnFileOwner:
      ret = "owner";
      break;
  }
  return ret;
}

static Fm::FolderModel::ColumnId sortColumnFromString(const QString str) {
  Fm::FolderModel::ColumnId ret;
  if(str == "name")
      ret = Fm::FolderModel::ColumnFileName;
  else if(str == "type")
    ret = Fm::FolderModel::ColumnFileType;
  else if(str == "size")
    ret = Fm::FolderModel::ColumnFileSize;
  else if(str == "mtime")
    ret = Fm::FolderModel::ColumnFileMTime;
  else if(str == "owner")
    ret = Fm::FolderModel::ColumnFileOwner;
  else
    ret = Fm::FolderModel::ColumnFileName;
  return ret;
}

static const char* wallpaperModeToString(int value) {
  const char* ret;
  switch(value) {
    case DesktopWindow::WallpaperNone:
    default:
      ret = "none";
      break;
    case DesktopWindow::WallpaperTransparent:
      ret = "transparent";
     break;
    case DesktopWindow::WallpaperStretch:
      ret = "stretch";
      break;
    case DesktopWindow::WallpaperFit:
      ret = "fit";
      break;
    case DesktopWindow::WallpaperCenter:
      ret = "center";
      break;
    case DesktopWindow::WallpaperTile:
      ret = "tile";
      break;
  }
  return ret;
}

static int wallpaperModeFromString(const QString str) {
  int ret;
  if(str == "transparent")
    ret = DesktopWindow::WallpaperTransparent;
  else if(str == "stretch")
    ret = DesktopWindow::WallpaperStretch;
  else if(str == "fit")
    ret = DesktopWindow::WallpaperFit;
  else if(str == "center")
    ret = DesktopWindow::WallpaperCenter;
  else if(str == "tile")
    ret = DesktopWindow::WallpaperTile;
  else
    ret = DesktopWindow::WallpaperNone;
  return ret;
}

static const char* sidePaneModeToString(Fm::SidePane::Mode value) {
  const char* ret;
  switch(value) {
    case Fm::SidePane::ModePlaces:
    default:
      ret = "places";
      break;
    case Fm::SidePane::ModeDirTree:
      ret = "dirtree";
      break;
    case Fm::SidePane::ModeNone:
      ret = "none";
      break;
  }
  return ret;
}

static Fm::SidePane::Mode sidePaneModeFromString(const QString& str) {
  Fm::SidePane::Mode ret;
  if(str == "none")
    ret = Fm::SidePane::ModeNone;
  else if(str == "dirtree")
    ret = Fm::SidePane::ModeDirTree;
  else
    ret = Fm::SidePane::ModePlaces;
  return ret;
}

void Settings::setTerminal(QString terminalCommand) {
    terminal_ = terminalCommand;
    // override the settings in libfm FmConfig.
    g_free(fm_config->terminal);
    fm_config->terminal = g_strdup(terminal_.toLocal8Bit().constData());
    g_signal_emit_by_name(fm_config, "changed::terminal");
  }

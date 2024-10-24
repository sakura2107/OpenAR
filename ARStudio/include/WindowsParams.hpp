#pragma once

struct ARStudioWindowData {
    bool ShowAppAssetsBrowser = false;
    bool ShowAppConsole = false;
    bool ShowAppCustomRendering = false;
    bool ShowAppDocuments = false;
    bool ShowAppLog = false;
    bool ShowAppLayout = false;
    bool ShowAppPropertyEditor = false;
    bool ShowAppSimpleOverlay = false;
    bool ShowAppAutoResize = false;
    bool ShowAppConstrainedResize = false;
    bool ShowAppFullscreen = false;
    bool ShowAppLongText = false;
    bool ShowAppWindowTitles = false;

    bool ShowIDStackTool = false;
    bool ShowStyleEditor = false;
    bool ShowAbout = false;
    // ARStudio Menu (accessible from the "Menu" menu)
    bool IsAutoSave = false;

    // ARStudio Tool (accessible from the "Tool" menu)
    bool IsShowLogTraceTool = false;
    // ARStudio Window (accessible from the "Window" menu)
    bool IsSaveWindowLayout = false;
    bool IsSelectedTheme = false;
    bool IsEditTheme = false;
    // ARStudio Help (accessible from the "Help" menu)
    bool IsViewTutorial = false;
};
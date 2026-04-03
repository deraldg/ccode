#pragma once

namespace foxtalk {

enum : int {
    cmTalkExit     = 9000,
    cmShowOutput   = 9001,
    cmRunCmd       = 9002,
    cmFocusCmd     = 9003,
    cmFlushLog     = 9004,
    cmZoomCmd      = 9005,
    cmResetCmdWin  = 9006,
    cmZoomOut      = 9007,
    cmResetOutWin  = 9008,
    cmWorkspace    = 9009,

    cmFileOpen     = 9100,
    cmFileClose    = 9101,
    cmAreaSelect   = 9102,
    cmIndexOpen    = 9103,
    cmIndexClose   = 9104,
    cmWinSave      = 9105,
    cmWinRestore   = 9106,
    cmWinDefaults  = 9107,

    cmBrowse       = 9200,
    cmRecordView   = 9201,
    cmList         = 9202,
    cmSmartList    = 9203,
    cmCount        = 9204,
    cmFind         = 9205,
    cmLocate       = 9206,
    cmWhere        = 9207,
    cmWhereClear   = 9208,
    cmShowDeletedStub = 9209,
    cmStatusStub      = 9210,

    cmSeek         = 9300,
    cmSetOrder     = 9301,
    cmReindex      = 9302,
    cmTagManagerStub = 9303,
    cmOrderInfoStub  = 9304,

    cmSetRelation  = 9400,
    cmRelRefresh   = 9401,
    cmRelClear     = 9402,
    cmRelBrowserStub = 9403,

    cmAppend       = 9500,
    cmEdit         = 9501,
    cmDelete       = 9502,
    cmRecall       = 9503,
    cmPack         = 9504,
    cmTurboPack    = 9505,
    cmZapStub      = 9506,
    cmBlankStub    = 9507,

    cmCalc         = 9600,
    cmEval         = 9601,
    cmHelp         = 9602,
    cmSelfTest     = 9603,
    cmHistorySizeStub = 9604,
    cmVerbosityStub   = 9605,
    cmOutputModeStub  = 9606,
    cmPaletteColor    = 9607,

    cmKeysStub     = 9700,
    cmAboutStub    = 9701
};

} // namespace foxtalk

#pragma once

#include "domain/types.hpp"

namespace skipper::text {

inline constexpr const char* kChooseDirectory = "选择目录…";
inline constexpr const char* kDirectory = "目录";
inline constexpr const char* kTopLevelOnly = "只读取该目录第一层，不进入子目录。";
inline constexpr const char* kNoDirectory = "尚未选择目录";
inline constexpr const char* kVideoList = "视频列表（可选其中一集作为标准）";
inline constexpr const char* kStandardHint = "标准视频需填写它的片头 / 片尾时间";
inline constexpr const char* kStandard = "标准";
inline constexpr const char* kFile = "文件";
inline constexpr const char* kDuration = "时长";
inline constexpr const char* kNoStandard = "未指定标准";
inline constexpr const char* kStandardPrefix = "标准视频：";
inline constexpr const char* kStandardHelp = "其余视频用这段声音来对齐；时间可留空，空表示不剪这一头";
inline constexpr const char* kIntroStart = "片头起点";
inline constexpr const char* kIntroEnd = "片头终点";
inline constexpr const char* kCreditsStart = "片尾起点";
inline constexpr const char* kCreditsEnd = "片尾终点";
inline constexpr const char* kAnalyzeHint = "只填一个时间或时间超出时长时，不能开始分析";
inline constexpr const char* kMethods = "定位手段";
inline constexpr const char* kMethodStandard = "标准";
inline constexpr const char* kMethodChapter = "章节";
inline constexpr const char* kMethodAudio = "音频";
inline constexpr const char* kMethodBlack = "黑帧";
inline constexpr const char* kClearStandard = "清空标准";
inline constexpr const char* kAnalyze = "开始分析";
inline constexpr const char* kNoVideos = "这个目录下没有可处理的视频";
inline constexpr const char* kStandardInvalid = "标准时间不正确";
inline constexpr const char* kStandardUnreadable = "无法读取标准视频";
inline constexpr const char* kBadTime = "时间格式不正确";
inline constexpr const char* kAudioSkipped = "未能进行音频比对，已改用其余已选手段";
inline constexpr const char* kResults = "分析结果（可修改后导出）";
inline constexpr const char* kManualHint = "修改过的来源会显示为「手动」";
inline constexpr const char* kExclude = "排除";
inline constexpr const char* kSource = "来源";
inline constexpr const char* kNotFound = "未检出";
inline constexpr const char* kConfirmHint = "两头都空的集不会被导出。时间可改可清空，格式为 时:分:秒 或秒数。";
inline constexpr const char* kExportTo = "确认后导出到";
inline constexpr const char* kBack = "返回选择";
inline constexpr const char* kExport = "开始导出";
inline constexpr const char* kCannotRead = "无法读取";
inline constexpr const char* kProgress = "导出进度";
inline constexpr const char* kAnalyzing = "分析进度";
inline constexpr const char* kCancel = "取消";
inline constexpr const char* kDone = "完成";
inline constexpr const char* kCancelledNote = "已取消，其余未处理";
inline constexpr const char* kCannotCreateOutput = "无法创建输出目录";
inline constexpr const char* kRunning = "进行中";
inline constexpr const char* kWaiting = "等待";
inline constexpr const char* kProcessed = "已处理";
inline constexpr const char* kCancelling = "正在取消";
inline constexpr const char* kAllDone = "全部完成";
inline constexpr const char* kCancelled = "已取消";
inline constexpr const char* kOutputDir = "输出目录";
inline constexpr const char* kUntouched = "未处理";
inline constexpr const char* kEditInvalid = "时间不成立，请修正后再导出";
inline constexpr const char* kHalfEmpty = "起点和终点需要同时填写";
inline constexpr const char* kOrder = "起点必须早于终点";
inline constexpr const char* kOutOfRange = "时间超出了视频时长";
inline constexpr const char* kOverlap = "片头终点不能晚于片尾起点";
inline constexpr const char* kStepSelect = "选择目录";
inline constexpr const char* kStepConfirm = "确认时间";
inline constexpr const char* kStepResult = "导出结果";
inline constexpr const char* kTrayTip = "Skipper — 剪掉片头和片尾";
inline constexpr const char* kShowMainWindow = "显示主界面";
inline constexpr const char* kQuit = "退出";

inline const char* stage(Stage value) {
  switch (value) {
    case Stage::Probe: return "探测";
    case Stage::Fingerprint: return "指纹";
    case Stage::BlackFrame: return "画面";
    case Stage::Match: return "定位";
    case Stage::Export: return "导出";
  }
  return "";
}

inline const char* source(SegmentSource value) {
  switch (value) {
    case SegmentSource::Standard: return "标准";
    case SegmentSource::Chapter: return "章节";
    case SegmentSource::Audio: return "音频";
    case SegmentSource::BlackFrame: return "黑帧";
    case SegmentSource::BlackFrameAndAudio: return "黑帧+音频";
    case SegmentSource::Manual: return "手动";
  }
  return "";
}

inline const char* sourceKind(SegmentSource value) {
  switch (value) {
    case SegmentSource::Standard: return "std";
    case SegmentSource::Chapter: return "chap";
    case SegmentSource::Audio: return "audio";
    case SegmentSource::BlackFrame: return "black";
    case SegmentSource::BlackFrameAndAudio: return "both";
    case SegmentSource::Manual: return "manual";
  }
  return "none";
}

inline const char* status(ItemStatus value) {
  switch (value) {
    case ItemStatus::Exported: return "已导出";
    case ItemStatus::Excluded: return "已排除";
    case ItemStatus::Skipped: return "已跳过";
    case ItemStatus::Failed: return "失败";
  }
  return "";
}

inline const char* statusKind(ItemStatus value) {
  switch (value) {
    case ItemStatus::Exported: return "ok";
    case ItemStatus::Excluded: return "muted";
    case ItemStatus::Skipped: return "warn";
    case ItemStatus::Failed: return "danger";
  }
  return "muted";
}

inline const char* detail(ItemStatus itemStatus, Reason reason) {
  if (itemStatus == ItemStatus::Exported) return "";
  if (itemStatus == ItemStatus::Excluded) return "已排除";
  if (itemStatus == ItemStatus::Skipped && reason == Reason::NoSegments) return "没有可剪的片头或片尾";
  if (itemStatus == ItemStatus::Skipped && reason == Reason::OutputExists) return "输出文件已存在";
  if (reason == Reason::CannotRead) return "无法读取";
  if (reason == Reason::ExportFailed) return "导出失败";
  if (reason == Reason::Cancelled) return "已取消";
  return "";
}

}  // namespace skipper::text

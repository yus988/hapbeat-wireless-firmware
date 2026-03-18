// 頻繁に調整したい変数はこちらで調整（環境別ファイルを取り込む集約ファイル）
#include "adjustParams.h"

// 注意:
// - lib/ 以下から参照される可能性があるため、extern 宣言は include/adjustParams.h に集約
// - 実体定義はビルド環境ごとにここで取り込む .inc 側に寄せる

// まず sample_tasks を優先して取り込む
#if defined(TASK_BAND_GEN_ESPNOW)
  #include "sample_tasks/taskBandGenESPNOW/adjustParams.hpp"
#elif defined(TASK_NECK_GEN_ESPNOW)
  #include "sample_tasks/taskNeckGenESPNOW/adjustParams.hpp"
#elif defined(TASK_NECK_STREAM_ESPNOW)
  #include "sample_tasks/taskStreamESPNOW/adjustParams.hpp"
#elif __has_include("private_tasks/adjustParams.hpp")
  // sample 対象以外は private override を使用
  #include "private_tasks/adjustParams.hpp"
#else
  #error "Define one of TASK_NECK_GEN_ESPNOW or TASK_BAND_GEN_ESPNOW (or provide private override)"
#endif

// サイズ定義は各 adjustProfiles/*.hpp 内で inline 変数として提供

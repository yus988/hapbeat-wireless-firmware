#ifndef AUDIO_MANAGER_SETTINGS_HPP
#define AUDIO_MANAGER_SETTINGS_HPP

// Minimal definitions to satisfy audioManager.h compilation
// (audioManager is not used in streaming mode, but globals.h includes it)
#define CATEGORY_NUM 1
#define SOUND_FILE_NUM 1
#define DATA_NUM 1
#define SUB_DATA_NUM 1
#define IS_EVENT_MODE false
#define VOLUME_MAX 10
#define STUB_NUM 2
#define POSITION_NUM 1
#define LOOP_CONTINUE_TIMEOUT_MS 3000

#endif

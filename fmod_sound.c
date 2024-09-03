#include "fmod/fmod_errors.h"
#include "fmod/fmod.h"
#include "fmod/fmod_studio.h"

FMOD_STUDIO_SYSTEM* fmod_studio_system = 0;
FMOD_STUDIO_BANK* fmod_studio_bank = 0;
FMOD_STUDIO_BANK* fmod_studio_strings_bank = 0;

FMOD_STUDIO_EVENTINSTANCE* play_sound(char* path) {
  FMOD_STUDIO_EVENTDESCRIPTION* event_desc;
  FMOD_RESULT ok = FMOD_Studio_System_GetEvent(fmod_studio_system, path, &event_desc);
  assert(ok == FMOD_OK, "error playing - %s - %s", path, FMOD_ErrorString(ok));

	FMOD_STUDIO_EVENTINSTANCE* instance;
	ok = FMOD_Studio_EventDescription_CreateInstance(event_desc, &instance);
  assert(ok == FMOD_OK, "%s", FMOD_ErrorString(ok));

	// set the cooldown so we don't get doubles (won't play if another one hits within 40ms)
	ok = FMOD_Studio_EventInstance_SetProperty(instance, FMOD_STUDIO_EVENT_PROPERTY_COOLDOWN, 40.0/1000.0);
  assert(ok == FMOD_OK, "%s", FMOD_ErrorString(ok));

	ok = FMOD_Studio_EventInstance_Start(instance);
  assert(ok == FMOD_OK, "%s", FMOD_ErrorString(ok));

	// this auto-releases when event is finished
	ok = FMOD_Studio_EventInstance_Release(instance);
  assert(ok == FMOD_OK, "%s", FMOD_ErrorString(ok));

	return instance;
}

void stop_sound(FMOD_STUDIO_EVENTINSTANCE* instance) {
	FMOD_RESULT ok = FMOD_Studio_EventInstance_Stop(instance, FMOD_STUDIO_STOP_ALLOWFADEOUT);
  assert(ok == FMOD_OK, "%s", FMOD_ErrorString(ok));
}

void fmod_init() {
  FMOD_RESULT ok;

  ok = FMOD_Studio_System_Create(&fmod_studio_system, FMOD_VERSION);
  assert(ok == FMOD_OK, "%s", FMOD_ErrorString(ok));

  ok = FMOD_Studio_System_Initialize(fmod_studio_system, 512, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, null);
  assert(ok == FMOD_OK, "%s", FMOD_ErrorString(ok));

	// load bank files
	FMOD_Studio_System_LoadBankFile(fmod_studio_system, "res/fmod/Master.bank", FMOD_STUDIO_LOAD_BANK_NORMAL, &fmod_studio_bank);
	// pretty sure the strings are used so we can have a handle when playing events, idk tho
	FMOD_Studio_System_LoadBankFile(fmod_studio_system, "res/fmod/Master.strings.bank", FMOD_STUDIO_LOAD_BANK_NORMAL, &fmod_studio_strings_bank);
}
void fmod_shutdown() {
  FMOD_RESULT ok = FMOD_Studio_System_Release(fmod_studio_system);
  assert(ok == FMOD_OK, "%s", FMOD_ErrorString(ok));
}

void fmod_update() {
  FMOD_RESULT ok = FMOD_Studio_System_Update(fmod_studio_system);
  assert(ok == FMOD_OK, "%s", FMOD_ErrorString(ok));
}
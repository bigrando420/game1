FMOD_STUDIO_SYSTEM* fmod_studio_system = 0;
FMOD_STUDIO_BANK* fmod_studio_bank = 0;
FMOD_STUDIO_BANK* fmod_studio_strings_bank = 0;

FMOD_STUDIO_EVENTINSTANCE* play_sound_at_pos(char* path, Vector2 pos) {
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

  // set pos
	FMOD_3D_ATTRIBUTES attributes;
	attributes.position = (FMOD_VECTOR){pos.x, pos.y, 0};
	attributes.forward = (FMOD_VECTOR){0, 0, 1};
	attributes.up = (FMOD_VECTOR){0, 1, 0};
	FMOD_Studio_EventInstance_Set3DAttributes(instance, &attributes);

	// this auto-releases when event is finished
	ok = FMOD_Studio_EventInstance_Release(instance);
  assert(ok == FMOD_OK, "%s", FMOD_ErrorString(ok));

  return instance;
}


FMOD_STUDIO_EVENTINSTANCE* play_sound(char* path) {
	return play_sound_at_pos(path, v2(99999, 99999));
}

// todo - some kind of automated continuous sound system...
// #continualsound
/*
typedef struct ContinuousSound {
  FMOD_STUDIO_EVENTINSTANCE* event;

  u64 last_frame_update;
  // we call update_sound on this every frame
  // It's stored in an array somewhere.
  // so in the global fmod update function, we check if it's been updated, if not, we kill it.

} ContinuousSound;
*/

bool is_sound_playing(FMOD_STUDIO_EVENTINSTANCE* sound) {
    FMOD_STUDIO_PLAYBACK_STATE state;
    FMOD_RESULT result = FMOD_Studio_EventInstance_GetPlaybackState(sound, &state);
    if (result != FMOD_OK) {
        // log_error("FMOD error: %s", FMOD_ErrorString(result));
        return false;
    }
    return state != FMOD_STUDIO_PLAYBACK_STOPPED;
}

void update_sound_position(FMOD_STUDIO_EVENTINSTANCE* instance, Vector2 pos) {
    FMOD_RESULT ok;
    FMOD_3D_ATTRIBUTES attributes;

    // Get the current 3D attributes of the sound instance
    ok = FMOD_Studio_EventInstance_Get3DAttributes(instance, &attributes);
    if (ok != FMOD_OK) {
        log_error("FMOD error getting attributes: %s", FMOD_ErrorString(ok));
        return;
    }

    // Update the position
    attributes.position = (FMOD_VECTOR){pos.x, pos.y, 0};

    // Set the updated 3D attributes back to the sound instance
    ok = FMOD_Studio_EventInstance_Set3DAttributes(instance, &attributes);
    if (ok != FMOD_OK) {
        log_error("FMOD error setting attributes: %s", FMOD_ErrorString(ok));
    }
}

void stop_sound(FMOD_STUDIO_EVENTINSTANCE* instance) {
	FMOD_RESULT ok = FMOD_Studio_EventInstance_Stop(instance, FMOD_STUDIO_STOP_ALLOWFADEOUT);
  if (ok != FMOD_OK) {
    log_error("FMOD error: %s", FMOD_ErrorString(ok));
  }
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
  FMOD_RESULT ok;

  // update listener pos
  FMOD_3D_ATTRIBUTES attributes;
	attributes.position = (FMOD_VECTOR){get_player()->pos.x, get_player()->pos.y, 0};
	attributes.forward = (FMOD_VECTOR){0, 0, 1};
	attributes.up = (FMOD_VECTOR){0, 1, 0};
	ok = FMOD_Studio_System_SetListenerAttributes(fmod_studio_system, 0, &attributes, 0);
  assert(ok == FMOD_OK, "%s", FMOD_ErrorString(ok));

  ok = FMOD_Studio_System_Update(fmod_studio_system);
  assert(ok == FMOD_OK, "%s", FMOD_ErrorString(ok));
}
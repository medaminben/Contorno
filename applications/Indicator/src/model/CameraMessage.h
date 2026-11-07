#ifndef INDICATOR_CAMERAMESSAGE_H
#define INDICATOR_CAMERAMESSAGE_H
#include "model/Message.h"

#include <Vision/Photo.h>

/// A note of Vision::resolve_camera as a sentence: "the photo's focal length 0.00 mm is not valid".
Message note_message(Vision::CameraNote const& note);

/// The log line, built from its parts: "Camera: …: X.XXX mm/px."
Message camera_message(Vision::PhotoCamera const& camera, bool rotated = false);

#endif  // INDICATOR_CAMERAMESSAGE_H

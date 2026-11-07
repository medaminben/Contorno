#include "model/CameraMessage.h"

#include "model/Measure.h"

#include <charconv>
#include <string>
#include <vector>

namespace {

using Messages = std::vector<Message>;

/// A number with the given decimals, for the log.
std::string num(double v, int decimals) {
    char buffer[400];
    auto const result = std::to_chars(buffer, buffer + sizeof buffer, v, std::chars_format::fixed, decimals);
    return std::string(buffer, result.ptr);
}

/// The 35 mm of the "35 mm equivalent": a number with its unit, never translated.
Message equivalent_35mm(char const* key) { return Message(key).with("size", "35 mm"); }

Message note_pixel(Message const& what, double p) {
    return Message("camera.msg.note_pixel").with("what", what).with("value", num(p, 2) + " µm");
}

/// The items from `from` on.
Messages tail(Messages const& items, std::size_t from = 1) { return Messages(items.begin() + from, items.end()); }

/// "a", "a and b", "a, b and c": each join a message, so the words follow the language.
Message join_items(Messages const& items) {
    if (items.empty()) return {};
    if (items.size() == 1) return items.front();
    if (items.size() == 2) return Message("camera.msg.and").with("first", items[0]).with("second", items[1]);
    return Message("camera.msg.comma").with("first", items.front()).with("rest", join_items(tail(items)));
}

/// The notes one after the other, separated as the language does.
Message join_notes(Messages const& notes) {
    if (notes.empty()) return {};
    if (notes.size() == 1) return notes.front();
    return Message("camera.msg.notes_more").with("first", notes.front()).with("rest", join_notes(tail(notes)));
}

}  // namespace

Message note_message(Vision::CameraNote const& note) {
    switch (note.kind) {
        case Vision::CameraNoteKind::focal_length_invalid:
            return Message("camera.msg.note_focal").with("value", num(note.value, 2) + " mm");
        case Vision::CameraNoteKind::focal_plane_invalid:
            return note_pixel(Message("camera.msg.what_focal_plane"), note.value);
        case Vision::CameraNoteKind::equivalent_35mm_invalid:
            return note_pixel(equivalent_35mm("camera.msg.what_35mm"), note.value);
        case Vision::CameraNoteKind::possibly_cropped:
            return Message("camera.msg.note_crop");
    }
    return {};
}

Message camera_message(Vision::PhotoCamera const& camera, bool rotated) {
    std::string const scale = scale_text(Vision::lens_scale(camera.lens, rotated)) + " mm/px";
    if (camera.focal_source == Vision::CameraSource::settings && camera.pixel_source == Vision::CameraSource::settings
        && camera.notes.empty())
        return Message("camera.msg.no_data").with("scale", scale);

    Messages from_photo, from_settings;
    Message const focal =
        Message("camera.msg.focal").with("value", num(camera.lens.focal_length_mm, 2) + " mm");
    Message pixel = Message("camera.msg.pixel")
                        .with("value", camera.lens.pixel_width_um == camera.lens.pixel_height_um
                                           ? num(camera.lens.pixel_width_um, 2) + " µm"
                                           : num(camera.lens.pixel_width_um, 2) + " × "
                                                 + num(camera.lens.pixel_height_um, 2) + " µm");
    if (camera.pixel_source == Vision::CameraSource::photo_35mm)
        pixel = equivalent_35mm("camera.msg.pixel_35mm").with("pixel", pixel);
    (camera.focal_source == Vision::CameraSource::settings ? from_settings : from_photo).push_back(focal);
    (camera.pixel_source == Vision::CameraSource::settings ? from_settings : from_photo).push_back(pixel);
    from_settings.push_back(Message("camera.msg.distance").with("value", num(camera.lens.distance_mm, 0) + " mm"));

    Message const settings = Message("camera.msg.from_settings").with("items", join_items(from_settings));
    Message groups = settings;
    if (!from_photo.empty())
        groups = Message("camera.msg.groups")
                     .with("photo", Message("camera.msg.from_photo").with("items", join_items(from_photo)))
                     .with("settings", settings);
    if (camera.notes.empty()) return Message("camera.msg.full").with("groups", groups).with("scale", scale);
    Messages notes;
    for (auto const& note : camera.notes) notes.push_back(note_message(note));
    return Message("camera.msg.full_notes")
        .with("groups", groups)
        .with("notes", join_notes(notes))
        .with("scale", scale);
}

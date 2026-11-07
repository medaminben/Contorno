#ifndef INDICATOR_INSPECTION_H
#define INDICATOR_INSPECTION_H
#include "model/Message.h"

#include <Inspection/Inspection.h>

#include <filesystem>
#include <optional>
#include <variant>
#include <vector>

/// A result whose error is a message, rendered when shown (see `Message`).
template <class T = std::monostate>
using InspectionResult = Result<T, Core::Error<Message>>;

/// An inspection zip as the window uses it: the photo as a file, the camera,
/// the minimum area and the saved result (the format is Inspection's).
struct InspectionFiles {
    std::filesystem::path photo_path {};
    Vision::PhotoCamera camera {};
    double min_area_mm2 {100};
    /// The saved result; none for an older inspection.
    std::optional<Vision::ObjectsAnalysis> result {};
    /// An older hide inspection: its parameters and result are ignored, the camera comes from it.
    bool legacy {false};
};

/// The entries of an inspection zip (see Inspection::to_entries) for the photo
/// at `files.photo_path`, written with the analyser's outline tolerance (0.5 mm).
/// Error when the photo cannot be read or an entry cannot be written.
InspectionResult<std::vector<Tools::ZipEntry>> inspection_entries(InspectionFiles const& files,
                                                                  Vision::ObjectsAnalysis const& analysis);

/// Reads an inspection zip (see Inspection::from_entries): writes its photo
/// into `into_dir` and returns the photo's path with the rest. Error naming
/// what is missing or invalid.
InspectionResult<InspectionFiles> unpack_inspection(std::filesystem::path const& zip,
                                                    std::filesystem::path const& into_dir);

#endif  // INDICATOR_INSPECTION_H

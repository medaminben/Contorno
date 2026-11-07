#include "model/Inspection.h"

#include "model/Analyser.h"
#include "model/ImageSize.h"
#include "model/LibraryText.h"
#include "model/Utf8Path.h"

#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace {

template <class T>
InspectionResult<T> failure(Message message) {
    return InspectionResult<T>(Core::Error<Message>(std::move(message)));
}

}  // namespace

InspectionResult<std::vector<Tools::ZipEntry>> inspection_entries(
    InspectionFiles const& files, Vision::ObjectsAnalysis const& analysis) {
    using Entries = std::vector<Tools::ZipEntry>;
    std::ifstream in(files.photo_path, std::ios::binary);
    if (!in)
        return failure<Entries>(
            Message("inspection.cannot_read").with("path", utf8(files.photo_path)));
    Inspection::Record record;
    record.photo_name = Inspection::photo_name(utf8(files.photo_path));
    record.photo.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    record.camera = files.camera;
    record.min_area_mm2 = files.min_area_mm2;
    record.outline_tolerance_mm = Analyser::outline_tolerance_mm;
    auto entries = Inspection::to_entries(record, analysis, shown_rotated(files.photo_path));
    if (entries.isError())
        return failure<Entries>(from_library(entries.error()));
    return InspectionResult<Entries>(std::move(entries)());
}

InspectionResult<InspectionFiles> unpack_inspection(std::filesystem::path const& zip,
                                                    std::filesystem::path const& into_dir) {
    auto read = Tools::read_zip(utf8(zip));
    if (read.isError())
        return failure<InspectionFiles>(from_library(read.error()));
    auto record = Inspection::from_entries(read(), utf8(zip.filename()));
    if (record.isError())
        return failure<InspectionFiles>(from_library(record.error()));
    auto& r = record();

    InspectionFiles files;
    files.photo_path = into_dir / path_from_utf8(r.photo_name);
    std::ofstream photo(files.photo_path, std::ios::binary);
    photo.write(r.photo.data(), static_cast<std::streamsize>(r.photo.size()));
    photo.close();
    if (!photo)
        return failure<InspectionFiles>(
            Message("inspection.cannot_write").with("path", utf8(files.photo_path)));
    files.camera = r.camera;
    files.min_area_mm2 = r.min_area_mm2;
    files.result = std::move(r.result);
    files.legacy = r.legacy;
    return InspectionResult<InspectionFiles>(std::move(files));
}

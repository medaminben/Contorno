#include <gtest/gtest.h>

#include <wx/intl.h>
#include <wx/translation.h>

#include "ui/Catalogue.h"
#include "ui/Localise.h"

namespace test_Catalogue {

TEST(WxCatalogue, GermanFileDialogTexts) {
    if (!wx_catalogues_embedded())
        GTEST_SKIP() << "no wxstd catalogue embedded (wx3.2-i18n not installed)";
    apply_language("de");
    EXPECT_NE(wxGetTranslation("Cancel"), "Cancel");
    apply_language("en");
    EXPECT_EQ(wxGetTranslation("Cancel"), "Cancel");
}

}  // namespace test_Catalogue

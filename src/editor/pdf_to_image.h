
#include "../shared/xpdf.h"

#include <wx/image.h>

wxImage pdf_to_image(const std::string &app_dir, const std::string &pdf, int page);
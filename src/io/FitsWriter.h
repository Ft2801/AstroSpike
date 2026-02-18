#ifndef FITSWRITER_H
#define FITSWRITER_H

#include <QString>
#include "ImageBuffer.h"

class FitsWriter {
public:
    /**
     * @brief Save an ImageBuffer to a FITS file (32-bit float).
     * @param filePath Path to save.
     * @param buffer The image buffer to save.
     * @param errorMsg Optional pointer to catch error messages.
     * @return true if successful, false otherwise.
     */
    static bool write(const QString& filePath, const ImageBuffer& buffer, QString* errorMsg = nullptr);
};

#endif // FITSWRITER_H

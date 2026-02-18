#include "FitsWriter.h"
#include <fitsio.h>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>

bool FitsWriter::write(const QString& filePath, const ImageBuffer& buffer, QString* errorMsg) {
    if (!buffer.isValid()) {
        if (errorMsg) *errorMsg = "Empty buffer.";
        return false;
    }

    fitsfile* fptr;
    int status = 0;

    // FITS uses planar storage. Interleave -> Planar conversion is needed.
    int w = buffer.width();
    int h = buffer.height();
    int c = buffer.channels();
    long naxes[3] = { (long)w, (long)h, (long)c };
    int naxis = (c > 1) ? 3 : 2;

    // Delete existing file if present (CFITSIO won't overwrite unless path starts with !)
    QString nativePath = QDir::toNativeSeparators(filePath);
    if (QFileInfo::exists(nativePath)) {
        QFile::remove(nativePath);
    }

    if (fits_create_file(&fptr, nativePath.toUtf8().constData(), &status)) {
        if (errorMsg) {
            char statusStr[FLEN_STATUS];
            fits_get_errstatus(status, statusStr);
            *errorMsg = QCoreApplication::translate("FitsWriter", "CFITSIO Create Error: %1").arg(statusStr);
        }
        return false;
    }

    // Create primary image HDU (32-bit float)
    if (fits_create_img(fptr, FLOAT_IMG, naxis, naxes, &status)) {
        if (errorMsg) {
            char statusStr[FLEN_STATUS];
            fits_get_errstatus(status, statusStr);
            *errorMsg = QCoreApplication::translate("FitsWriter", "CFITSIO Image Create Error: %1").arg(statusStr);
        }
        fits_close_file(fptr, &status);
        return false;
    }

    const std::vector<float>& data = buffer.data();
    long npixelsPerPlane = (long)w * h;

    // Write plane by plane
    std::vector<float> plane(npixelsPerPlane);
    for (int ch = 0; ch < c; ++ch) {
        long firstpix[3] = { 1, 1, ch + 1 };
        
        // Convert interleaved to planar for this channel
        for (long i = 0; i < npixelsPerPlane; ++i) {
            plane[i] = data[i * c + ch];
        }

        if (fits_write_pix(fptr, TFLOAT, firstpix, npixelsPerPlane, plane.data(), &status)) {
            if (errorMsg) {
                char statusStr[FLEN_STATUS];
                fits_get_errstatus(status, statusStr);
                *errorMsg = QCoreApplication::translate("FitsWriter", "CFITSIO Write Error (Plane %1): %2").arg(ch).arg(statusStr);
            }
            fits_close_file(fptr, &status);
            return false;
        }
    }

    // Write basic metadata if available
    const auto& meta = buffer.metadata();
    if (!meta.objectName.isEmpty()) {
        fits_write_key(fptr, TSTRING, "OBJECT", (void*)meta.objectName.toUtf8().constData(), "Object Name", &status);
    }
    if (meta.exposure > 0) {
        fits_write_key(fptr, TDOUBLE, "EXPTIME", (void*)&meta.exposure, "Exposure Time (s)", &status);
    }
    status = 0; // Ignore metadata errors for now

    fits_close_file(fptr, &status);
    return (status == 0);
}
